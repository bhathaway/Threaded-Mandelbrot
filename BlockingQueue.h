//! \file BlockingQueue.h
//! \brief File containing the BlockingQueue template class.
//!
//! This file implements a thread-safe blocking queue typically useful
//! for waking up threads that are waiting to process asynchronous data.
#pragma once

#include <queue>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <type_traits>
#include <cassert>

// TODO: Add a sentinal element parameter with default value.

//! \brief Thread safe queue allowing multiple producers and consumers.
//!
//! An instance of this class is capable of concurrently accepting multiple
//! pushes and pops from separate threads. If the queue is full, push methods
//! will block the calling thread. Likewise, if the queue is empty, pop methods
//! will block the calling thread. This behavior is by design, making this
//! class excellent for asynchronous processing to avoid the dreaded busy-wait
//! loop, which makes it impossible to properly diagnose true CPU utilization.
//!
//! \tparam Item Element type.
//! \tparam item_limit Queue size limit. Zero means unlimited.
template < typename Item, unsigned item_limit = 64 >
class BlockingQueue : private std::queue<std::unique_ptr<Item> >
{
public: // Exceptions, typedefs

  //! \brief Empty exception class for methods that can time-out.
  struct Timeout { };
  typedef std::queue<std::unique_ptr<Item> > Base;

public:
  BlockingQueue() : Base() { }

  //! \brief Be certain the queue is empty before destroying it.
  //! 
  //! The principle design for achieving this by creating a sentinal element
  //! to represent "end-of-queue" to consumers for however many consumers
  //! exist. A potential improvement to this class would be to require that
  //! item as a template parameter so that it would only be necessary for
  //! producers to signal the "end-of-queue" condition once.
  virtual ~BlockingQueue()
  { }

  //! \brief Blocks calling thread until an item is available.
  //!
  //! \return The first item from the queue.
  std::unique_ptr<Item> pop()
  {
    std::unique_lock<std::mutex> lock(_transaction_mutex);
    auto condition = [&] { return ! this->Base::empty(); };
    _item_can_pop.wait(lock, condition);
    std::unique_ptr<Item> ret(std::move(this->front()));

    if (ret) {
      this->Base::pop();
    }
    _item_can_push.notify_all();
    return ret;
  }

  //! \brief Blocks calling thread until an item is available or timeout.
  //!
  //! This method can throw a Timeout exception, which indicates that the
  //! amount of time specified in the parameter has passed. Use this method
  //! to prevent consumers from blocking their thread indefinitely. This is
  //! useful for time-critical processing.
  //!
  //! \tparam timeout A duration.
  //!
  //! \return The first item from the queue.
  template <typename ChronoDuration>
  std::unique_ptr<Item> pop(ChronoDuration timeout)
  {
    std::unique_lock<std::mutex> lock(_transaction_mutex);
    auto condition = [&] { return ! this->Base::empty(); };

    if (!_item_can_pop.wait_for(lock, timeout, condition)) {
      throw Timeout();
    }

    std::unique_ptr<Item> ret(std::move(this->front()));

    if (ret) {
      this->Base::pop();
    }
    _item_can_push.notify_all();
    return ret;
  }

  //! \brief Blocks calling thread until there is room in the queue.
  //! \param item Element to copy to the end of the queue.
  //
  // You can push by value as long as you've defined a copy constructor
  // for type item.
  void push(const Item & item)
  {
    std::unique_ptr<Item> ptr(new Item(item));
    push(move(ptr));
  }

  template <typename ChronoDuration>
  void push(const Item & item, ChronoDuration timeout)
  {
    std::unique_ptr<Item> ptr(new Item(item));
    push(move(ptr), timeout);
  }

  //! \brief Blocks calling thread until there is room in the queue.
  //!
  //! \param item Element to move to the end of the queue.
  void push(std::unique_ptr<Item> item)
  {
    //We should never pass a nullptr because its the poison element
    assert(item != nullptr);

    std::unique_lock<std::mutex> lock(_transaction_mutex);

    if (item_limit && !this->Base::empty()) {
      // the (signed) cast is as elegant a hack around older compilers
      // warning about unsigned comparisons always returning false.
      auto condition = [&] { return (signed)this->size() < (signed)item_limit; };
      _item_can_push.wait(lock, condition);
    }

    this->Base::push(std::move(item));
    _item_can_pop.notify_all();
  }

  //! \brief Blocks calling thread until there is room or time out.
  //!
  //! This method can throw a Timeout exception, which indicates that the
  //! amount of time specified in the parameter has passed. Use this method
  //! to prevent producers from blocking their thread indefinitely. This is
  //! useful for time-critical processing.
  //! 
  //! \param item Element to copy to the end of the queue.
  //! \tparam timeout A duration.
  template <typename ChronoDuration>
  void push(std::unique_ptr<Item> item, ChronoDuration timeout)
  {
    //We should never pass a nullptr because its the poison element
    assert(item != nullptr);

    std::unique_lock<std::mutex> lock(_transaction_mutex);

    if (item_limit && !this->Base::empty()) {
      auto condition = [&] { return (signed)this->size() < (signed)item_limit; };

      if (!_item_can_push.wait_for(lock, timeout, condition)) {
        throw Timeout();
      }
    }

    this->Base::push(std::move(item));
    _item_can_pop.notify_all();
  }

  //! \brief Signals that the queue should shut down.
  //!
  //! Subsequent calls to push() result in undefined behavior. The caller
  //! can rely on pop() eventually returning a nullptr.
  void shutdown()
  {
    std::unique_lock<std::mutex> lock(_transaction_mutex);
    
    // TODO: shutdown should not block.
    if (item_limit && !this->Base::empty()) {
      auto condition = [&] { return (signed)this->size() < (signed)item_limit; };
      _item_can_push.wait(lock, condition);
    }

    std::unique_ptr<Item> thing(nullptr);
    this->Base::push(std::move(thing));
    _item_can_pop.notify_all();
  }

private:
  mutable std::mutex _transaction_mutex;
  mutable std::condition_variable _item_can_push;
  mutable std::condition_variable _item_can_pop;
};
