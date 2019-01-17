#include "Connection.h"

#include <iostream>
#include <sys/uio.h>

#include <chrono>
#include <thread>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Start worker: {} ", _socket);
    // Subscribe to events
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _alive = true;
}

// See Connection.h
void Connection::OnError() {
    _alive = false;
    _logger->debug("OnError worker: {}", _socket);
}

// See Connection.h
void Connection::OnClose() {
    _alive = false;
    _logger->debug("OnClose worker: {}", _socket);
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("DoRead worker: {} ", _socket);
    // Process new connection:
    // - read commands until socket alive
    // - execute each command
    // - send response
    try {
        int readed_bytes_part = -1;
        while ((readed_bytes_part = read(_socket, _client_buffer + _readed_bytes, sizeof(_client_buffer) - _readed_bytes)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes_part);
            _readed_bytes += readed_bytes_part;
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (_readed_bytes > 0) {
                _logger->debug("Process {} bytes", _readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(_client_buffer, _readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
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
                        std::memmove(_client_buffer, _client_buffer + parsed, _readed_bytes - parsed);
                        _readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", _readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(_readed_bytes));
                    argument_for_command.append(_client_buffer, to_read);

                    std::memmove(_client_buffer, _client_buffer + to_read, _readed_bytes - to_read);
                    arg_remains -= to_read;
                    _readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    // _logger->debug("Waiting for 5 sec...");
                    // std::this_thread::sleep_for(std::chrono::seconds(5));
                    command_to_execute->Execute(*_ps, argument_for_command, result);


                    // Send response
                    result += "\r\n";
                    _logger->debug("Result: {}", result);
                    _responses.push_back(result);

                    _logger->debug("Set socket to write {}", _socket);
                    _event.events |= EPOLLOUT;
                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (_readed_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
  _logger->debug("DoWrite worker: {}", _socket);
  std::size_t response_size = _responses.size();
  std::size_t nr;
  struct iovec iov[response_size];
  for (std::size_t i = 0; i < response_size; i++)
  {
    iov[i].iov_base = &_responses[i][0];
    iov[i].iov_len = _responses[i].size();
  }

  iov[0].iov_base = static_cast<char*>(iov[0].iov_base) + _written_bytes;
  iov[0].iov_len -= _written_bytes;

  nr = writev(_socket, iov, response_size);
  if (nr == -1)
  {
    _logger->error("Failed iovec write, {}", _socket);
    OnClose();
    return;
  }

  _logger->debug("Written {} bytes", nr);
  _written_bytes += nr;
  std::size_t pos_w = 0;
  while (pos_w < response_size && _written_bytes >= iov[pos_w].iov_len)
  {
    pos_w++;
    _written_bytes -= iov[pos_w].iov_len;
  }
  _responses.erase(_responses.begin(), _responses.begin() + pos_w);

  if (_responses.empty()) {
    _event.events &= ~EPOLLOUT;
    _logger->debug("End DoWrite. No responses");
  }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
