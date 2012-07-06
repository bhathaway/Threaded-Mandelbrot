#if !defined __BAH_BLOCKING_QUEUE_H_
#define __BAH_BLOCKING_QUEUE_H_

#include <queue>
#include <boost/thread/condition_variable.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// This class initializes to an active state, i.e. shutdown is false to start.
template <typename T, unsigned item_limit>
class BlockingQueue : private std::queue<T>
{
public: // Exceptions
    struct Timeout { };

public:
    BlockingQueue() : std::queue<T>() { }

    T pop()
    {
        boost::unique_lock<boost::mutex> lock(_transaction_mutex);

        if (std::queue<T>::empty()) {
            _item_can_pop.wait(lock);
        }

        T ret(std::queue<T>::front());
        std::queue<T>::pop();
        _item_can_push.notify_one();

        return ret;
    }

    // Can throw Timeout
    T pop(const boost::posix_time::time_duration & timeout)
    {
        boost::unique_lock<boost::mutex> lock(_transaction_mutex);

        if (std::queue<T>::empty()) {
            if (!_item_can_pop.timed_wait(lock, timeout)) {
                throw Timeout();
            }
        }

        T ret(std::queue<T>::front());
        std::queue<T>::pop();
        _item_can_push.notify_one();

        return ret;
    }

    void push(const T & item)
    {
        boost::unique_lock<boost::mutex> lock(_transaction_mutex);

        if (std::queue<T>::size() >= item_limit) {
            _item_can_push.wait(lock);
        }

        std::queue<T>::push(item);
        _item_can_pop.notify_one();
    }

    // Can throw Timeout
    void push(const T & item, const boost::posix_time::time_duration & timeout)
    {
        boost::unique_lock<boost::mutex> lock(_transaction_mutex);

        if (std::queue<T>::size() >= item_limit) {
            if (!_item_can_push.timed_wait(lock, timeout)) {
                throw Timeout();
            }
        }
        
        std::queue<T>::push(item);
        _item_can_pop.notify_one();
    }

private:
    mutable boost::mutex _transaction_mutex;
    mutable boost::condition_variable _item_can_push;
    mutable boost::condition_variable _item_can_pop;
};

#endif
