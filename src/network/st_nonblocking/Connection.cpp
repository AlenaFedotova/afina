#include "Connection.h"

#include <memory>
#include <stdexcept>

#include <unistd.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() { 
    std::cout << "Start" << std::endl; 
    _event.events = mask_read;
    _event.data.fd = _socket;
    _event.data.ptr = this;
}

// See Connection.h
void Connection::OnError() { 
    std::cout << "OnError" << std::endl; 
    _isAlive = false;
}

// See Connection.h
void Connection::OnClose() { 
    std::cout << "OnClose" << std::endl; 
    _isAlive = false;
}

// See Connection.h
void Connection::DoRead() { 
    std::cout << "DoRead" << std::endl; 
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
                    _answers.push_back(result);
                    struct iovec tmp;
                    tmp.iov_len = _answers.back().size();
                    tmp.iov_base = &(_answers.back()[0]);
                    _iovecs.push_back(tmp);
                    
                    _event.events = mask_read_write;

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "Failed to process connection on descriptor" << _socket << ": " << ex.what() << std::endl;
    }
}

// See Connection.h
void Connection::DoWrite() { 
    std::cout << "DoWrite" << std::endl;
    int written;
    if ((written = writev(_socket, _iovecs.data(), _iovecs.size())) <= 0) {
        std::cerr << "Failed to send response\n";
    }
    _position += written;
    int N = 0;
    int i = 0;
    while (N < _position) {
        N += _answers[i].size();
        i++;
    }
    _position -= N;
    _answers.erase(_answers.begin(), _answers.begin() + i);
    _iovecs.erase(_iovecs.begin(), _iovecs.begin() + i);
    if (_iovecs.size() == 0) {
        _event.events = mask_read;
    }
    else {
        _iovecs[0].iov_base = static_cast<char*>(_iovecs[0].iov_base) + _position;
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
