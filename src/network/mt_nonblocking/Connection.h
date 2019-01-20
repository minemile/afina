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
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> pl) :
     _socket(s), _ps(ps), _logger(pl) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
    }

    inline bool isAlive() const { return _alive.load(); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    static const int READ_EVENT = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
    static const int READ_WRITE_EVENT = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT | EPOLLONESHOT;

    int _socket;
    std::atomic<bool> _alive;
    struct epoll_event _event;
    std::shared_ptr<Afina::Storage> _ps;
    std::shared_ptr<spdlog::logger> _logger;

    std::mutex _con_mutex;

    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    char _client_buffer[4096];

    uint32_t _readed_bytes;
    std::size_t _written_bytes;
    std::vector<std::string> _responses;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
