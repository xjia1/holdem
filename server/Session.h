#pragma once
#include <iostream>
#include <functional>
#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>

namespace holdem {

using boost::asio::deadline_timer;
using boost::asio::ip::tcp;
using boost::optional;
using boost::system::error_code;

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

    std::string login_name() const
    {
        return login_name_;
    }

    void send(const std::string &message)
    {
        boost::asio::write(socket_, boost::asio::buffer(message.data(), message.size()));
    }

    void receive(std::string &message)
    {
        optional<error_code> timer_result;
        deadline_timer timer(io_service_);
        timer.expires_from_now(boost::posix_time::seconds(5));
        timer.async_wait(boost::bind(set_result, &timer_result, _1));

        optional<error_code> read_result;
        boost::asio::streambuf buf;
        boost::asio::async_read_until(socket_, buf, '\n', boost::bind(set_result, &read_result, _1));

        io_service_.reset();
        while (io_service_.run_one())
        {
            if (read_result)
                timer.cancel();
            else if (timer_result)
                socket_.cancel();
        }

        if (read_result && !*read_result)
        {
            std::istream is(&buf);
            std::getline(is, message);
        }
        else
        {
            message = "";
        }
    }

private:
    static void set_result(optional<error_code> *a, error_code b)
    {
        a->reset(b);
    }

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
