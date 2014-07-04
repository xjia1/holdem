#pragma once
#include <algorithm>
#include <chrono>
#include <random>
#include <vector>
#include "Card.h"

namespace holdem {

class Deck {
public:
    Deck()
    {
        static unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
        static std::default_random_engine generator(seed);

        for (char suit : "CDHS")
        for (char rank : "23456789TJQKA")
        {
            Card card;
            card.rank = rank;
            card.suit = suit;
            cards.emplace_back(card);
        }

        std::shuffle(cards.begin(), cards.end(), generator);
    }

    void burn()
    {
        cards.pop_back();
    }

    Card deal()
    {
        Card card = cards.back();
        cards.pop_back();
        return card;
    }

private:

    std::vector<Card> cards;
};

}
