#if !defined __BAH_BLOCKING_QUEUE_H_
#define __BAH_BLOCKING_QUEUE_H_

#include <queue>
#include <boost/thread/condition_variable.hpp>

template <typename T>
class BlockingQueue : private std::queue<T>
{
public: // Exceptions
    struct Shutdown { };

public:
    BlockingQueue() : std::queue<T>(), _shutdown(false) { }

    T pop()
    {
        boost::mutex::scoped_lock lock(_transaction_mutex);

        if (std::queue<T>::empty() && !_shutdown) {
            _wake_up_condition.wait(lock);
        }

        if (std::queue<T>::empty() && _shutdown) {
            throw Shutdown();
        } else if (!std::queue<T>::empty()) {
            T ret(std::queue<T>::front());
            std::queue<T>::pop();

            return ret;
        } else {
            assert(false);
        }
    }

    void push(const T & x)
    {
        boost::mutex::scoped_lock lock(_transaction_mutex);
        std::queue<T>::push(x);
        _wake_up_condition.notify_one();
    }

    void shutdown()
    {
        boost::mutex::scoped_lock lock(_transaction_mutex);
        _shutdown = true;
    }

private:
    bool _shutdown;
    mutable boost::mutex::mutex _transaction_mutex;
    mutable boost::condition_variable _wake_up_condition;
};

#endif
