#pragma once
#include <string.h>

namespace holdem {

class IO {
public:
    virtual ~IO() {}
    virtual void broadcast(const std::string &message) = 0;
    virtual void send(int i, const std::string &message) = 0;
    virtual void receive(int i, std::string &message) = 0;
};

}
