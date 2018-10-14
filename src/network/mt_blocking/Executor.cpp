#include <afina/Executor.h>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace Afina {

// See Executor.h
Executor::Executor(std::string name, int low_watermark, int hight_watermark, int max_queue_size, int idle_time) : _low_watermark(low_watermark), _hight_watermark(hight_watermark), _max_queue_size(max_queue_size), _idle_time(idle_time) {
    _state.store(State::kRun);
    std::unique_lock<std::mutex> lock(this->_threads_mutex);
    for (int i = 0; i < _low_watermark; i++) {
        _threads.push_back(std::thread(&perform, this));
    }
    _free_threads.store(0);
}

// See Executor.h
Executor::~Executor() {}

// See Executor.h
void Executor::Stop(bool await) {
    _state.store(State::kStopping); //TODO: await ???
    _empty_condition.notify_all();
    {
        std::unique_lock<std::mutex> lock(_threads_mutex);
        while (_threads.size() != 0) {
            _cv_stopping.wait(lock);
        }
    }
    _state.store(State::kStopped);
}

// See Executor.h
void perform(Executor *executor) {
    std::cout << "new thread\n";
    while (executor->_state.load() == Executor::State::kRun) {
        std::cout << "new iteration\n";
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> tasks_lock(executor->_tasks_mutex);
            while (executor->_tasks.size() == 0 && executor->_state.load() == Executor::State::kRun) {
                std::cout << "waiting\n";
                executor->_free_threads.store(executor->_free_threads.load() + 1);
                if (executor->_empty_condition.wait_for(tasks_lock, std::chrono::milliseconds(executor->_idle_time)) == std::cv_status::timeout) {
                    std::unique_lock<std::mutex> threads_lock(executor->_threads_mutex);
                    if (executor->_threads.size() > executor->_low_watermark) {
                        tasks_lock.unlock();
                        std::thread::id this_id = std::this_thread::get_id();
                        auto iter = std::find_if(executor->_threads.begin(), executor->_threads.end(), [=](std::thread &t) { return (t.get_id() == this_id); });
                        if (iter != executor->_threads.end()) {
                            iter->detach();
                            executor->_free_threads.store(executor->_free_threads.load() - 1); 
                            executor->_threads.erase(iter);
                            std::cout << "i died\n";
                        }
                        return;
                    }
                    else {
                        threads_lock.unlock();
                        executor->_empty_condition.wait(tasks_lock);
                    }
                }
                executor->_free_threads.store(executor->_free_threads.load() - 1); 
            }
            std::cout << "stop waiting\n";
            if (executor->_tasks.size() == 0) {
                continue;
            }
            task = executor->_tasks.front();
            executor->_tasks.pop_front();
        }
        task();
    }
    {
        std::unique_lock<std::mutex> lock(executor->_threads_mutex);
        std::thread::id this_id = std::this_thread::get_id();
        auto iter = std::find_if(executor->_threads.begin(), executor->_threads.end(), [=](std::thread &t) { return (t.get_id() == this_id); });
        if (iter != executor->_threads.end()) {
            iter->detach();
            executor->_threads.erase(iter);
        }
        if (executor->_threads.size() == 0) {
            executor->_cv_stopping.notify_all();
        }
    }
}

void Executor::_add_thread() {
    std::unique_lock<std::mutex> lock(this->_threads_mutex);
    if (_threads.size() < _hight_watermark) {
        _threads.push_back(std::thread(&perform, this));
    }   
}

} // namespace Afina
