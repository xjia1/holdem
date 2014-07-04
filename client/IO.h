#pragma once
#include <iostream>
#include <string>
#include <boost/asio.hpp>

namespace holdem {

using boost::asio::ip::tcp;

class IO {
public:
    IO(const std::string &host, const std::string &service) : io_service_(), socket_(io_service_)
    {
        tcp::resolver resolver(io_service_);
        tcp::resolver::query query(tcp::v4(), host, service);
        tcp::resolver::iterator iterator = resolver.resolve(query);
        boost::asio::connect(socket_, iterator);
    }

    void receive(std::string &message)
    {
        boost::asio::streambuf b;
        boost::asio::read_until(socket_, b, '\n');
        std::istream is(&b);
        std::getline(is, message);
    }

    void send(const std::string &message)
    {
        boost::asio::write(socket_, boost::asio::buffer(message.data(), message.size()));
    }

private:
    boost::asio::io_service io_service_;
    tcp::socket socket_;
};

}
