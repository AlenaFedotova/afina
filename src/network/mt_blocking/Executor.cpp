#include <afina/Executor.h>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace Afina {

// See Executor.h
Executor::Executor(std::string name, int low_watermark, int hight_watermark, int max_queue_size, int idle_time) : _low_watermark(low_watermark), _hight_watermark(hight_watermark), _max_queue_size(max_queue_size), _idle_time(idle_time) {
    _state.store(State::kReady);
}

// See Executor.h
Executor::~Executor() {}

// See Executor.h
void Executor::Start(std::shared_ptr<spdlog::logger> logger) {
    _logger = logger;
    _state.store(State::kRun);
    std::unique_lock<std::mutex> lock(this->_mutex);
    for (int i = 0; i < _low_watermark; i++) {
        _threads.push_back(std::thread(&perform, this));
    }
    _free_threads = 0;
}

// See Executor.h
void Executor::Stop(bool await) {
    _state.store(State::kStopping);
    _empty_condition.notify_all();
    {
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_threads.empty()) {
            _cv_stopping.wait(lock);
        }
    }
    _state.store(State::kStopped);
}

// See Executor.h
void perform(Executor *executor) {
    executor->_logger->debug("new thread");
    while (executor->_state.load() == Executor::State::kRun) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->_mutex);
            auto time_until = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->_idle_time);
            while (executor->_tasks.empty() && executor->_state.load() == Executor::State::kRun) {
                executor->_logger->debug("waiting");
                executor->_free_threads++;
                if (executor->_empty_condition.wait_until(lock, time_until) == std::cv_status::timeout) {
                    if (executor->_threads.size() > executor->_low_watermark) {
                        executor->_erase_thread();
                        return;
                    }
                    else {
                        executor->_empty_condition.wait(lock);
                    }
                }
                executor->_free_threads--; 
            }
            executor->_logger->debug("stop waiting");
            if (executor->_tasks.empty()) {
                continue;
            }
            task = executor->_tasks.front();
            executor->_tasks.pop_front();
        }
        task();
    }
    {
        std::unique_lock<std::mutex> lock(executor->_mutex);
        executor->_erase_thread();
        if (executor->_threads.empty()) {
            executor->_cv_stopping.notify_all();
        }
    }
}

void Executor::_erase_thread() {
    std::thread::id this_id = std::this_thread::get_id();
    auto iter = std::find_if(_threads.begin(), _threads.end(), [=](std::thread &t) { return (t.get_id() == this_id); });
    if (iter != _threads.end()) {
        iter->detach();
        _free_threads--; 
        _threads.erase(iter);
        _logger->debug("i died");
        return;
    }
    throw std::runtime_error("error while erasing thread");
}

void Executor::_add_thread() {
    _threads.push_back(std::thread(&perform, this));
}

} // namespace Afina
