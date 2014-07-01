#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include "Server.h"

using namespace holdem;

int main(int argc, char* argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: server <port> <numPlayers> <initialChips>\n";
        return 1;
    }

    const int port = std::atoi(argv[1]);
    const int num_players = std::atoi(argv[2]);
    const int initial_chips = std::atoi(argv[3]);

    try
    {
        boost::asio::io_service io_service;
        Server s(io_service, port, num_players, initial_chips);
        io_service.run();
    }
    catch (std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
