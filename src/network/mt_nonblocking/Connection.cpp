#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start(std::shared_ptr<spdlog::logger> logger) { 
    _event.events = mask_read;
    _event.data.fd = _socket;
    _event.data.ptr = this;
    _logger = logger;
}

// See Connection.h
void Connection::OnError() { 
    _isAlive.store(false);
}

// See Connection.h
void Connection::OnClose() { 
    _isAlive.store(false);
}

// See Connection.h
void Connection::DoRead() { 
    auto tmp = _sync_read.load();
    try {
        int readed_bytes_new = -1;
        while ((readed_bytes_new = read(_socket, client_buffer + readed_bytes, sizeof(client_buffer) - readed_bytes)) > 0) {
            readed_bytes += readed_bytes_new;
            while (readed_bytes > 0) {
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);
                    result += "\n";

                    // Save response
                    {
                        std::lock_guard<std::mutex> lock(_mutex);
                        _answers.push_back(result);
                        _event.events = mask_read_write;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {} : {}", _socket, ex.what());
    }
    _sync_read.store(true);
}

// See Connection.h
void Connection::DoWrite() {
    struct iovec *iovecs;
    std::size_t ans_size;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        ans_size = _answers.size();
        iovecs = new struct iovec[ans_size];
        for (int i = 0; i < _answers.size(); i++) {
            iovecs[i].iov_len = _answers[i].size();
            iovecs[i].iov_base = &(_answers[i][0]);
        }
    }
    iovecs[0].iov_base = static_cast<char*>(iovecs[0].iov_base) + _position;
    
    assert(iovecs[0].iov_len > _position);
    iovecs[0].iov_len -= _position;
    
    int written;
    if ((written = writev(_socket, iovecs, ans_size)) <= 0) {
        _logger->error("Failed to send response");
    }
    _position += written;
    
    int i = 0;
    for (; i < ans_size && (_position - iovecs[i].iov_len) >= 0; i++) {
        _position -= iovecs[i].iov_len;
    }
    
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _answers.erase(_answers.begin(), _answers.begin() + i);
        if (_answers.empty()) {
            _event.events = mask_read;
        }
    }
    delete[] iovecs;
}
} // namespace MTnonblock
} // namespace Network
} // namespace Afina
