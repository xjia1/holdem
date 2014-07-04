#include "IO.h"

using namespace holdem;

int main()
{
    IO io("localhost", "12345");
    io.send("login Test");
    std::string message;
    io.receive(message);
    std::cout << message << "\n";
}
