#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <sys/uio.h>
#include <mutex>
#include <atomic>

#include <sys/epoll.h>
#include <afina/execute/Command.h>
#include <afina/Storage.h>
#include "protocol/Parser.h"
#include <spdlog/logger.h>

namespace spdlog {
class logger;
}

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps) : _socket(s), pStorage(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _isAlive.store(true);
    }

    inline bool isAlive() const { return _isAlive.load(); }

    void Start(std::shared_ptr<spdlog::logger> logger);

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;
    
    static const int mask_read = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
    static const int mask_read_write = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT | EPOLLONESHOT;
    
    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    
    std::mutex _mutex;
    std::atomic<bool> _sync_read;

    int _socket;
    std::atomic<bool> _isAlive;
    struct epoll_event _event;
    
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    
    int readed_bytes = 0;
    char client_buffer[4096];
    
    std::vector<std::string> _answers;
    int _position = 0;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
