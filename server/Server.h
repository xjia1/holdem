#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "Session.h"

namespace holdem {

using boost::asio::ip::tcp;

class Server {
public:
    Server(boost::asio::io_service &io_service, const int port, const int num_players, const int initial_chips)
        : io_service_(io_service),
          acceptor_(io_service, tcp::endpoint(tcp::v4(), port)),
          num_players_(num_players),
          initial_chips_(initial_chips)
    {
        start_accept();
    }

private:
    void start_accept()
    {
        Session *new_session = new Session(io_service_,
            [this](Session *new_session) -> bool {
                if (sessions_.size() >= num_players_)
                    return false;

                sessions_.emplace_back(new_session);

                if (sessions_.size() == num_players_)
                    async_start_game();

                return true;
            });

        acceptor_.async_accept(new_session->socket(),
            boost::bind(&Server::handle_accept, this, new_session, boost::asio::placeholders::error));
    }

    void handle_accept(Session *new_session, const boost::system::error_code &error)
    {
        if (!error)
        {
            new_session->start();
        }
        else
        {
            std::cerr << "Server handle_accept error: " << error.message() << "\n";
            delete new_session;
        }

        start_accept();
    }

    void async_start_game()
    {
        io_service_.post(boost::bind(&Server::start_game, this));
    }

    void start_game()
    {
        // TODO start game
        std::cout << "game starts\n";
    }

    boost::asio::io_service &io_service_;
    tcp::acceptor acceptor_;
    const int num_players_;
    const int initial_chips_;
    std::vector<std::unique_ptr<Session>> sessions_;
};

}
