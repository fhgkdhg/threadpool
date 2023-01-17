#ifndef __THREADPOOL_HPP
#define __THREADPOOL_HPP

#include <vector>
#include <mutex>
#include <functional>
#include <thread>
#include <future>
#include <queue>
#include <condition_variable>
#include <memory>


class ThreadPool {
private:
    std::mutex _mutex;
    std::condition_variable _cv;
    std::queue<std::function<void()>> _tasks;
    std::vector<std::thread> _threads;
    bool _stopped = false;
public:
    ThreadPool(int n_threads = 4) {
        for (int i = 0; i < n_threads; ++ i) {
            _threads.emplace_back([this] {
                std::function<void()> func;
                while (true) {
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _cv.wait(lock, [this] {
                            return _stopped or !_tasks.empty();
                        });
                        if (_stopped and _tasks.empty()) return;
                        func = std::move(_tasks.front());
                        _tasks.pop();
                    }
                    func();
                }
            });
        }
    };

    ThreadPool(const ThreadPool &rhs) = delete;
    ThreadPool(ThreadPool &&rhs) = delete;
    ThreadPool &operator=(const ThreadPool &rhs) = delete;
    ThreadPool &operator=(ThreadPool &&rhs) = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
        for (std::thread &t : _threads) {
            t.join();
        }
    };

    template<typename F, typename... Args>
    auto submit(F &&f, Args &&... args) -> std::future<decltype(f(args...))> {
        if (_stopped)
            throw std::runtime_error("threadpool stopped.");
        
        using retType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<retType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        {
            std::lock_guard<std::mutex> lock(_mutex);
            _tasks.push([task] {
                (*task)();
            });
        }
        _cv.notify_one();
        return task->get_future();
    };
};



#endif