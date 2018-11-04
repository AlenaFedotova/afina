#include <afina/coroutine/EngineEpoll.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

namespace Afina {
namespace Network {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char stack_end;
    ctx.Hight = StackBottom;
    ctx.Low = &stack_end;
    
    char * &buffer = std::get<0>(ctx.Stack);
    uint32_t &av_size = std::get<1>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    
    if (av_size < size) {
        delete[] buffer;
        buffer = new char[size];
        av_size = size;
    }
    
    memcpy(buffer, ctx.Low, size);
}

void Engine::Restore(context &ctx) {
    char stack_end;
    if (&stack_end >= ctx.Low) {
        Restore(ctx);
    }
    
    char* &buffer = std::get<0>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    
    memcpy(ctx.Low, buffer, size);
    longjmp(ctx.Environment, 1);
}

void Engine::Enter(context& ctx) {
    if (cur_routine && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }
    
    cur_routine = &ctx;
    Restore(ctx);
}

void Engine::yield() {
    if (alive) {
        Enter(*alive);
    }
    
    if (cur_routine != idle_ctx) {
        Enter(*idle_ctx);
    }
}

void Engine::sched(void *routine_) {
    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    }
    
    if (cur_routine) {
        return;
    }
    
    yield();
}

void Engine::Wait() {
    if (!cur_routine->block) {
        _swap_list(alive, blocked, cur_routine);
        cur_routine->block = true;
    }
    cur_routine->events = 0;
    yield();
}

void Engine::Notify(context& ctx) {
    if (ctx.block) {
        _swap_list(blocked, alive, &ctx);
        ctx.block = false;
    }
}

void Engine::_swap_list(context * &list1, context * &list2, context * const &routine) {
    if (routine->prev != nullptr) {
        routine->prev->next = routine->next;
    }

    if (routine->next != nullptr) {
        routine->next->prev = routine->prev;
    }

    if (list1 == routine) {
        list1 = list1->next;
    }
        
    routine->next = list2;
    list2 = routine;
        
    if (routine->next != nullptr) {
        routine->next->prev = routine;
    }
}

int Engine::Read(int fd, void *buf, unsigned count) {
    while (_running) {
        int readed_bytes = read(fd, buf, count);
        if (readed_bytes <= 0) {
            _wait_in_epoll(fd, mask_read);
            if (cur_routine->events & EPOLLRDHUP) {
                return 0;
            }
            if (cur_routine->events & (EPOLLERR | EPOLLHUP)) {
                break;
            }
        }
        else {
            return readed_bytes;
        }
    }
    return -1;
}

int Engine::Write(int fd, const void *buf, int count) {
    int written = 0;
    while (_running) {
        written += write(fd, (char *)buf + written, count - written);
        if (written < count) {
            _wait_in_epoll(fd, mask_write);
            if (cur_routine->events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
                break;
            }
        }
        else {
            return written;
        }
    }
    return -1;
}

int Engine::Accept(int s, struct sockaddr * addr, unsigned int * anamelen) {
    while (_running) {
        int infd = accept4(s, addr, anamelen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (infd == -1) {
            _wait_in_epoll(s, EPOLLIN);
        }
        else {
            return infd;
        }
    }
    return -1;
}

void Engine::_wait_in_epoll(int fd, int mask) {
    struct epoll_event * event = new struct epoll_event;
    event->events = mask;
    event->data.ptr = cur_routine;
    
    if (epoll_ctl(_epoll_descr, EPOLL_CTL_ADD, fd, event)) {
        throw std::runtime_error("Failed to add file descriptor to epoll");
    }
    
    Wait();
    
    if (epoll_ctl(_epoll_descr, EPOLL_CTL_DEL, fd, event)) {
        throw std::runtime_error("Failed to delete file descriptor from epoll");
    }
    
    delete event;
}

void Engine::Stop() {
    _running = false;
    if (eventfd_write(_event_fd, 1)) {
        throw std::runtime_error("Failed to wakeup engine");
    }
}

} // namespace Coroutine
} // namespace Network
} // namespace Afina
