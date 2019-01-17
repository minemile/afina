#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>
//#include <spdlog/logger.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>

#include "protocol/Parser.h"

namespace spdlog {
class logger;
}

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> pl)
        : _socket(s), _ps(ps), _logger(pl) {

        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    bool _alive;
    struct epoll_event _event;
    std::shared_ptr<Afina::Storage> _ps;
    std::shared_ptr<spdlog::logger> _logger;

    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    char _client_buffer[4096];

    uint32_t _readed_bytes;
    std::size_t _written_bytes;
    std::vector<std::string> _responses;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
