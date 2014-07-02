#pragma once
#include <iostream>
#include <functional>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

namespace holdem {

using boost::asio::ip::tcp;

class Session {
public:
    Session(boost::asio::io_service &io_service, std::function<bool(Session *)> login_callback)
        : io_service_(io_service),
          socket_(io_service),
          login_callback_(login_callback)
    {
    }

    tcp::socket &socket()
    {
        return socket_;
    }

    void start()
    {
        boost::asio::async_read_until(socket_, login_buf_, "\n",
            boost::bind(&Session::handle_login, this, boost::asio::placeholders::error));
    }

private:
    void handle_login(const boost::system::error_code &error)
    {
        if (!error)
        {
            std::istream is(&login_buf_);
            std::string login, name;
            is >> login >> name;
            if (login == "login")
            {
                login_name_ = name;
                if (login_callback_(this))
                {
                    std::cout << "login " << name << "\n";
                }
                else
                {
                    std::cerr << "Session handle_login: game is full\n";
                    delete this;
                }
            }
            else
            {
                std::cerr << "Session handle_login: login command expected\n";
                delete this;
            }
        }
        else
        {
            std::cerr << "Session handle_login error: " << error.message() << "\n";
            delete this;
        }
    }

    boost::asio::io_service &io_service_;
    tcp::socket socket_;
    std::function<bool(Session *)> login_callback_;
    boost::asio::streambuf login_buf_;
    std::string login_name_;
};

}
