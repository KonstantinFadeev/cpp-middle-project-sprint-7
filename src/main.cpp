#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <print>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";
constexpr size_t read_chunk_size = 8192;

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {
    try {
        std::string client_request;
        co_await async_read_until(client_socket, dynamic_buffer(client_request), delimiter, use_awaitable);

        auto [host, port] = findHostPort(client_request);

        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);

        tcp::socket server_socket(io_service);
        co_await boost::asio::async_connect(server_socket, endpoints, use_awaitable);

        co_await boost::asio::async_write(server_socket, buffer(client_request), use_awaitable);

        std::string server_response;
        co_await async_read_until(server_socket, dynamic_buffer(server_response), delimiter, use_awaitable);

        auto content_length = findContentLength(server_response);

        co_await boost::asio::async_write(client_socket, buffer(server_response), use_awaitable);

        auto headers_end = server_response.find(delimiter);
        size_t body_already_read = server_response.size() - (headers_end + delimiter.size());

        if (content_length.has_value()) {
            if (*content_length > body_already_read) {
                size_t remaining = *content_length - body_already_read;
                std::string body(remaining, '\0');
                co_await boost::asio::async_read(server_socket, buffer(body), transfer_at_least(remaining),
                                                 use_awaitable);
                co_await boost::asio::async_write(client_socket, buffer(body), use_awaitable);
            }
        } else {
            boost::system::error_code ec;
            std::string chunk(read_chunk_size, '\0');
            for (;;) {
                auto n = co_await server_socket.async_read_some(buffer(chunk),
                                                                boost::asio::redirect_error(use_awaitable, ec));
                if (n > 0) {
                    co_await boost::asio::async_write(client_socket, buffer(chunk.data(), n), use_awaitable);
                }
                if (ec) {
                    break;
                }
            }
        }

        client_socket.close();
        server_socket.close();

    } catch (const std::exception &e) {
        std::println(stderr, "Session error: {}", e.what());
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](error_code ec, tcp::socket socket) {
            if (!ec) {
                co_spawn(io_service_, session(std::move(socket), io_service_), boost::asio::detached);
            }
            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::println(stderr, "Usage: proxy_server <listen_port>");
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::println(stderr, "Exception: {}", e.what());
    }
}
