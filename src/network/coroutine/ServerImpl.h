#ifndef AFINA_NETWORK_COROUTINE_SERVER_H
#define AFINA_NETWORK_COROUTINE_SERVER_H

#include <atomic>
#include <thread>
#include <map>
#include <condition_variable>

#include <afina/network/Server.h>
#include <afina/coroutine/EngineEpoll.h>

namespace spdlog {
class logger;
}

namespace Afina {
namespace Network {
namespace Coroutine {

/**
 * # Network resource manager implementation
 * Server that is spawning a separate thread for each connection
 */
class ServerImpl : public Server {
public:
    ServerImpl(std::shared_ptr<Afina::Storage> ps, std::shared_ptr<Logging::Service> pl);
    ~ServerImpl();

    // See Server.h
    void Start(uint16_t port, uint32_t, uint32_t) override;

    // See Server.h
    void Stop() override;

    // See Server.h
    void Join() override;

protected:
    /**
     * Method is running in the connection acceptor thread
     */
    void OnRun();

private:
    // Logger instance
    std::shared_ptr<spdlog::logger> _logger;
    
    // engine
    Engine _engine;

    // Atomic flag to notify threads when it is time to stop. Note that
    // flag must be atomic in order to safely publisj changes cross thread
    // bounds
    bool _running;

    // Server socket to accept connections on
    int _server_socket;

    // Thread to run network on
    std::thread _thread;
    
    void _WorkerFunction(int client_socket);
};

} // namespace MTblocking
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_COROUTINE_SERVER_H
