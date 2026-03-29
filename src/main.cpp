#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
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

        if (content_length.has_value()) {
            auto headers_end = server_response.find(delimiter);
            size_t body_already_read = server_response.size() - (headers_end + delimiter.size());
            size_t remaining = *content_length - body_already_read;

            if (remaining > 0) {
                std::string body(remaining, '\0');
                co_await boost::asio::async_read(server_socket, buffer(body), transfer_at_least(remaining),
                                                 use_awaitable);
                co_await boost::asio::async_write(client_socket, buffer(body), use_awaitable);
            }
        }

        client_socket.close();
        server_socket.close();

    } catch (const std::exception &e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                co_spawn(io_service_, session(std::move(socket_), io_service_), boost::asio::detached);
            }
            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: proxy_server";
            std::cerr << " <listen_port>\n";
            return 1;
        }
        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}
