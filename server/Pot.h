#pragma once
#include <set>

namespace holdem {

struct Pot {
    int amount;
    std::set<int> contributors;

    Pot() : amount(0) {}

    Pot(const Pot &o) : amount(o.amount), contributors(o.contributors) {}

    Pot(Pot &&o) : amount(o.amount), contributors(std::move(o.contributors)) {}

    void add(int x, int player)
    {
        amount += x;
        contributors.emplace(player);
    }
};

}
