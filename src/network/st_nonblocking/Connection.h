#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <sys/uio.h>

#include <sys/epoll.h>
#include <afina/execute/Command.h>
#include <afina/Storage.h>
#include "protocol/Parser.h"

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps) : _socket(s), pStorage(ps) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _isAlive = true;
    }

    inline bool isAlive() const { return _isAlive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    bool _isAlive;
    struct epoll_event _event;
    
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    
    int readed_bytes = 0;
    char client_buffer[4096];
    
    std::shared_ptr<Afina::Storage> pStorage;
    
    std::vector<std::string> _answers;
    int _position = 0;
    
    const int mask_read = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    const int mask_read_write = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
