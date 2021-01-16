#ifndef __ZENDAR_BLOCKING_SHARED_QUEUE__H__
#define __ZENDAR_BLOCKING_SHARED_QUEUE__H__

#include <thread>
#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <bitset>
#include <chrono>
#include <array>

#include <boost/optional.hpp>

// This class is set up for a SMALL number of subscribers.
// It would need different data structures if a large number of
// subscribers were ever needed. This is unlikely.
template <typename T, std::size_t capacity, std::size_t sub_count = 1>
class BlockingSharedQueue {
  static_assert(capacity > 1, "1 element queues are absurd");
  static_assert(sub_count > 0, "Queues must allow at least one subscriber");

private:
  struct RefCountWrapper {
    T data;
    std::bitset<sub_count> refs_bitset;
  };

  using PostIncIndex = std::size_t;

  struct Subscriber {
    Subscriber()
      : name(""), tail(0), live(false)
    { }
    Subscriber(const std::string& n)
      : name(n), tail(0), live(true)
    { }
    Subscriber(const Subscriber&) = default;
    Subscriber& operator=(const Subscriber&) = default;
    ~Subscriber() = default;

    std::string name;
    PostIncIndex tail;
    bool live;
  };

public: // Exceptions
  struct QueueFull : public std::runtime_error {
    QueueFull(const std::string& what) : std::runtime_error(what) { }
  };

  struct WrongSubscriberCount : public std::logic_error {
    WrongSubscriberCount()
      : std::logic_error(std::string("Must provide exactly " +
                         std::to_string(sub_count) + " subscriber names"))
    { }
  };

  struct QueueClosed : public std::logic_error {
    QueueClosed() : std::logic_error("Cannot push to a closed queue") { }
  };

  struct TimeoutOnClose : public std::runtime_error {
    TimeoutOnClose(const std::string& what) : std::runtime_error(what) { }
  };

public:
  BlockingSharedQueue() = delete;
  template <typename Iter>
  BlockingSharedQueue(Iter begin, Iter end)
    : head(0), closing(false)
  {
    std::size_t sub_id = 0;
    for (Iter name = begin; name != end; ++name, ++sub_id) {
      if (sub_id >= sub_count)
        throw WrongSubscriberCount();
      subscribers[sub_id] = Subscriber(*name);
    }
    if (sub_id < sub_count)
      throw WrongSubscriberCount();

    buffer.fill(RefCountWrapper());
  }
  BlockingSharedQueue(const BlockingSharedQueue&) = delete;
  BlockingSharedQueue& operator=(const BlockingSharedQueue&) = delete;

  ~BlockingSharedQueue() = default;

  void
  Push(const T& data)
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (closing)
      throw QueueClosed();

    // Full queue is FATAL.
    if (buffer[head].refs_bitset.any()) {
      std::stringstream message;
      message << "Queue full! Blocking tasks: ";
      for (std::size_t i = 0; i < sub_count - 1; ++i)
        if (buffer[head].refs_bitset[i])
          message << '\"' << subscribers[i].name << "\",";
      if (buffer[head].refs_bitset[sub_count - 1])
        message << '\"' << subscribers[sub_count - 1].name << '\"';
      throw QueueFull(message.str());
    }

    auto& slot = buffer[head];

    // Mark the slot for all subscribers
    slot.refs_bitset.set();
    slot.data = data;

    ++head;
    if (head == capacity)
      head = 0;
    wakeup_cond.notify_all();
  }

  using SubscriberIndex = std::size_t;

  boost::optional<T>
  Pop(SubscriberIndex id)
  {
    if (id >= sub_count)
      throw std::range_error(std::string("Invalid subscriber id: ") +
                             std::to_string(id));

    std::unique_lock<std::mutex> lock(queue_mutex);

    auto& sub = subscribers[id];
    auto& slot = buffer[sub.tail];

    auto ready = [&]() { return slot.refs_bitset[id] || closing; };
    wakeup_cond.wait(lock, ready);

    if (slot.refs_bitset[id]) {
      slot.refs_bitset.reset(id);
      auto& tail = sub.tail;
      ++tail;
      if (tail == capacity)
        tail = 0;
      return slot.data;
    } else {
      sub.live = false;
      sub_ended_cond.notify_one();
      return boost::none;
    }
  }

  // Wakes up all subscribers and waits for them to finish.
  void
  Close(std::chrono::milliseconds time_limit_ms)
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    closing = true;
    wakeup_cond.notify_all();

    auto all_dead = [&]() {
      return std::all_of(subscribers.begin(), subscribers.end(),
          [&](const Subscriber& s) { return !s.live; });
    };
    if (!sub_ended_cond.wait_for(lock, time_limit_ms, all_dead)) {
      std::stringstream message;
      message << "Timeout on Close! Blocking tasks: ";
      for (std::size_t i = 0; i < sub_count - 1; ++i)
        if (subscribers[i].live)
          message << '\"' << subscribers[i].name << "\",";
      if (subscribers[sub_count - 1].live)
        message << '\"' << subscribers[sub_count - 1].name << '\"';
      throw TimeoutOnClose(message.str());
    }
  }

private:
  std::mutex queue_mutex;
  std::condition_variable wakeup_cond;
  std::condition_variable sub_ended_cond;

  std::array<Subscriber, sub_count> subscribers;
  std::array<RefCountWrapper, capacity> buffer;
  PostIncIndex head;
  bool closing;
};
#endif
