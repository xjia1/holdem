#pragma once
#include <cassert>
#include <cstdarg>
#include <array>
#include <sstream>
#include <string>
#include <vector>
#include "Action.h"
#include "Card.h"
#include "Deck.h"
#include "IO.h"
#include "Pot.h"

namespace holdem {

class Game {
public:
    Game(IO &io, const std::vector<std::string> &names, std::vector<int> &chips, int blind)
        : io(io), names(names), chips(chips), blind(blind), n(chips.size()), dealer(0), hole_cards(n)
    {
        reset_current_state();
    }

    void run()
    {
        broadcast("game starts");
        broadcast("number of players is %d", n);
        broadcast("dealer is %s", name_of(dealer));

        chips[(dealer + 1) % n] -= blind;
        current_bets[(dealer + 1) % n] = blind;
        broadcast("player %s blind bet %d", name_of(dealer + 1), blind);

        chips[(dealer + 2) % n] -= blind * 2;
        current_bets[(dealer + 2) % n] = blind * 2;
        broadcast("player %s blind bet %d", name_of(dealer + 2), blind * 2);

        for (int i = 0; i < n; i++)
        {
            hole_cards[i][0] = deck.deal();
            send(i, "hole card %c %s", hole_cards[i][0].rank, suit_of(hole_cards[i][0]));
        }

        for (int i = 0; i < n; i++)
        {
            hole_cards[i][1] = deck.deal();
            send(i, "hole card %c %s", hole_cards[i][1].rank, suit_of(hole_cards[i][1]));
        }

        // pre-flop betting round (0 community cards dealt)
        if (bet_loop())
        {
            // flop betting round (3 community cards dealt)
            deck.burn();
            deal_community_card("flop");
            deal_community_card("flop");
            deal_community_card("flop");

            if (bet_loop())
            {
                // turn betting round (4 community cards dealt)
                deck.burn();
                deal_community_card("turn");

                if (bet_loop())
                {
                    // river betting round (5 community cards dealty)
                    deck.burn();
                    deal_community_card("river");

                    if (bet_loop())
                    {
                        // TODO showdown: receive player cards and compare
                        return;
                    }
                }
            }
        }

        // TODO only one player left
    }

private:
    // return true if game continues
    bool bet_loop()
    {
        broadcast("round starts");

        for (int i = 0; i < n; i++)
            broadcast("player %s has %d chips", name_of(i), chips[i]);

        while (!is_round_finished())
        {
            int player = current_player();
            int amount = get_bet_from(player);
            if (amount >= 0)
                bet(player, amount);
            else
                fold(player);
        }

        broadcast("round ends");
        // TODO print pots and contributions

        // only one player left, do not deal more cards, and do not require showdown
        if (all_except_one_fold())
            return false;

        // reset *after* the loop to keep blinds
        reset_current_state();
        return true;
    }

    int get_bet_from(int player)
    {
        send(player, "action");

        std::string message;
        receive(player, message);
        std::istringstream iss(message);

        std::string action_name;
        iss >> action_name;
        if (action_name == "bet")
        {
            int bet;
            iss >> bet;
            return bet;
        }
        else if (action_name == "check")
        {
            return 0;
        }
        else if (action_name == "fold")
        {
            return -1;
        }
        else
        {
            std::cerr << "unknown action " << action_name << "\n";
            return -1;
        }
    }

    void bet(int player, int amount)
    {
        if (chips[player] < amount)
        {
            std::cerr << "illegal bet: insufficient chips\n";
            fold(player);
        }
        else
        {
            int previous_bet = current_bets[previous_player(player)];
            int expected_bet = current_bets[player] + amount;

            if (chips[player] > amount && expected_bet < previous_bet)
            {
                std::cerr << "illegal bet: have sufficient chips but didn't bet as much as the previous player\n";
                fold(player);
            }
            else if (chips[player] > amount && expected_bet > previous_bet && expected_bet - previous_bet < blind)
            {
                std::cerr << "illegal bet: have sufficient chips but didn't raise as much as the blind\n";
                fold(player);
            }
            else
            {
                chips[player] -= amount;
                current_bets[player] += amount;
                current_actions[player].emplace_back(Action::BET);
                // TODO contribute to pots

                broadcast("player %s bets %d", name_of(player), amount);
                broadcast("player %s total bet is %d", name_of(player), current_bets[player]);
            }
        }
    }

    void fold(int player)
    {
        current_actions[player].emplace_back(Action::FOLD);
        broadcast("player %s folds", name_of(player));
    }

    bool folded(int player)
    {
        for (const Action &x : current_actions[player])
            if (x == Action::FOLD)
                return true;
        return false;
    }

    void reset_current_state()
    {
        current_bets.resize(n);
        current_actions.resize(n);
        for (int player = 0; player < n; player++)
        {
            current_bets[player] = 0;
            if (folded(player))
            {
                current_actions[player].clear();
                current_actions[player].emplace_back(Action::FOLD);
            }
            else
            {
                current_actions[player].clear();
            }
        }
    }

    void broadcast(const char *format, ...)
    {
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, 4096, format, args);
        io.broadcast(std::string(buffer));
        va_end(args);
    }

    void send(int player, const char *format, ...)
    {
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, 4096, format, args);
        io.send(player, std::string(buffer));
        va_end(args);
    }

    void receive(int player, std::string &message)
    {
        io.receive(player, message);
    }

    const char *name_of(int player)
    {
        return names[player % n].c_str();
    }

    const char *suit_of(const Card &card)
    {
        return suit_of(card.suit);
    }

    const char *suit_of(char suit)
    {
        switch (suit) {
        case 'C': return "club";
        case 'D': return "diamond";
        case 'H': return "heart";
        case 'S': return "spade";
        }
        assert(false);
    }

    void deal_community_card(const char *round_name)
    {
        Card card = deck.deal();
        community_cards.emplace_back(card);
        broadcast("%s card %c %s", round_name, card.rank, suit_of(card));
    }

    bool is_round_finished()
    {
        return all_except_one_fold() || (all_action_counts_are_equal() && all_bet_amounts_are_equal());
    }

    bool all_except_one_fold()
    {
        return previous_player(dealer) == previous_player(previous_player(dealer));
    }

    bool all_action_counts_are_equal()
    {
        int cnt = -1;
        for (int player = 0; player < n; player++)
        {
            if (folded(player)) continue;
            if (cnt == -1)
                cnt = current_actions[player].size();
            else if (cnt != static_cast<int>(current_actions[player].size()))
                return false;
        }
        return true;
    }

    bool all_bet_amounts_are_equal()
    {
        int amt = -1;
        for (int player = 0; player < n; player++)
        {
            if (folded(player)) continue;
            if (amt == -1)
                amt = current_bets[player];
            else if (amt != current_bets[player])
                return false;
        }
        return true;
    }

    int current_player()
    {
        int from = 1, to = n;
        if (is_pre_flop_round())
            from = 3, to = n + 2;

        // FIXME XXX BUG 如果已经fold不能再action

        if (no_current_actions())
        {
            for (int i = from; i <= to; i++)
            {
                int p = (dealer + i) % n;
                if (!folded(p))
                    return p;
            }
        }
        else
        {
            // not folded, have fewer actions than previous player, or bets fewer than previous player
            for (int i = from; i <= to; i++)
            {
                int p = (dealer + i) % n;
                if (folded(p))
                    continue;
                if (current_actions[p].size() < current_actions[previous_player(p)].size())
                    return p;
                if (chips[p] > 0 && current_bets[p] < current_bets[previous_player(p)])
                    return p;
            }
        }

        assert(false);
    }

    bool no_current_actions()
    {
        for (int i = 0; i < n; i++)
            if (current_actions[i].size() > 0)
                return false;
        return true;
    }

    bool is_pre_flop_round()
    {
        return community_cards.size() == 0;
    }

    int previous_player(int player)
    {
        for (int i = 1; i < n; i++)
        {
            int p = (player - i + n) % n;
            if (folded(p)) continue;
            return p;
        }
        assert(false);
    }

    IO &io;
    const std::vector<std::string> &names;
    std::vector<int> &chips;
    const int blind;
    const int n;
    int dealer;
    Deck deck;
    std::vector<std::array<Card, 2>> hole_cards;
    std::vector<Card> community_cards;
    std::vector<Pot> pots;
    std::vector<int> current_bets;
    std::vector<std::vector<Action>> current_actions;
};

}
