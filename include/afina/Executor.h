#ifndef AFINA_THREADPOOL_H
#define AFINA_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <iostream>
#include <thread>
#include <spdlog/logger.h>
#include <afina/logging/Service.h>

namespace spdlog {
class logger;
}

namespace Afina {

/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is ready to start
        kReady,
    
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadpool is stopped
        kStopped
    };

    Executor(std::string name, int low_watermark, int hight_watermark, int max_queue_size, int idle_time);
    ~Executor();
    
    /**
     * Creating low_watermark of threads.
     */
    void Start(std::shared_ptr<spdlog::logger> logger);

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);
        {
            std::unique_lock<std::mutex> lock(this->_mutex);
            if (_state.load() != State::kRun || _tasks.size() >= _max_queue_size) {
                _logger->debug("queue is full");
                return false;
            }

            // Enqueue new task
            _tasks.push_back(exec);
            if (_free_threads == 0 && _threads.size() < _hight_watermark) {
                _add_thread();
            } 
            _empty_condition.notify_one();
        }
        return true;
    }
    
    void set_logger();

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex _mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable _empty_condition;
    
    /**
     * Conditional variable to await finish all threads
     */
    std::condition_variable _cv_stopping;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> _threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> _tasks;

    /**
     * Flag to stop bg threads
     */
    std::atomic<State> _state;
    
    /**
     * Parameters of the pool
     */
    const int _low_watermark;
    const int _hight_watermark;
    const int _max_queue_size;
    const int _idle_time;
    int _free_threads;
    
    std::shared_ptr<spdlog::logger> _logger;
    
    void _add_thread();
};

void perform(Executor *executor);

} // namespace Afina

#endif // AFINA_THREADPOOL_H
