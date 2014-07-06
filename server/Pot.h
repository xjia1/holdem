#pragma once
#include <map>
#include <set>

namespace holdem {

class Pot {
public:
    Pot() : m() {}
    Pot(const Pot &o) : m(o.m) {}
    Pot(Pot &&o) : m(std::move(o.m)) {}

    void add(int x, int player)
    {
        m[player] += x;
    }

    int amount() const
    {
        int result = 0;
        for (const auto &pair : m)
            result += pair.second;
        return result;
    }

    std::set<int> contributors() const
    {
        std::set<int> result;
        for (const auto &pair : m)
            result.emplace(pair.first);
        return result;
    }

private:
    // map from player to amount
    std::map<int, int> m;
};

}
