#pragma once
#include <cassert>
#include <cstdarg>
#include <array>
#include <sstream>
#include <string>
#include <vector>
#include "Card.h"
#include "Deck.h"
#include "IO.h"
#include "Pot.h"

namespace holdem {

class Game {
public:
    Game(IO &io, const std::vector<std::string> &names, std::vector<int> &chips, int blind)
        : io(io), names(names), chips(chips), blind(blind), n(chips.size()), dealer(0), hole_cards(n), actioned(n, false), checked(n, false), folded(n, false)
    {
        reset_current_bets();
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
                        showdown();
                        return;
                    }
                }
            }
        }

        // TODO only one player left
    }

private:
    void showdown()
    {
        std::vector<std::pair<std::array<Card, 5>, int>> hands(n);

        for (int player = 0; player < n; player++)
        {
            if (folded[player])
                continue;

            send(player, "showdown");

            for (int i = 0; i < 5; i++)
                receive(player, hands[player].first[i]);

            hands[player].second = player;
        }

        // TODO check validity of hands

        // TODO compare and win pots
    }

    // return true if game continues
    bool bet_loop()
    {
        std::cerr << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n";

        broadcast("round starts");

        for (int i = 0; i < n; i++)
            broadcast("player %s has %d chips", name_of(i), chips[i]);

        // 从庄家下一个人开始说话
        int start_player = (dealer + 1) % n;
        if (is_pre_flop_round())
            // 第一轮下注有大小盲，从大盲下一个人开始说话
            start_player = (dealer + 3) % n;

        // 上一个raise的玩家，不算大小盲
        last_raiser = -1;

        for (int player = 0; player < n; player++)
            actioned[player] = checked[player] = false;

        // 下注结束条件：
        // 0. 不考虑已经fold的人
        // 1. 每个人都说过话
        // 2. 所有人下注相同，或者all-in
        // 3. 不能再raise了

        int current_player = start_player;
        for (;;)
        {
            if (folded[current_player])
            {
                current_player = (current_player + 1) % n;
                continue;
            }

            std::cerr << "current player is " << name_of(current_player) << "\n";

            int amount = get_bet_from(current_player);
            if (amount >= 0)
                bet(current_player, amount);
            else
                fold(current_player);
            actioned[current_player] = true;

            if (all_except_one_fold())
            {
                std::cerr << "all_except_one_fold\n";
                break;
            }

            if (all_players_checked())
            {
                std::cerr << "all_players_checked\n";
                break;
            }

            do current_player = (current_player + 1) % n; while (folded[current_player]);

            std::cerr << "next player is " << name_of(current_player) << "\n";

            if (all_players_actioned() && all_bet_amounts_are_equal() && there_is_no_possible_raise(current_player))
                break;
        }

        broadcast("round ends");

        // calculate pots and contributions from current_bets
        while (!all_zero(current_bets))
        {
            int x = minimum_positive(current_bets);
            Pot pot;
            for (int player = 0; player < n; player++)
            {
                if (current_bets[player] >= x)
                {
                    current_bets[player] -= x;
                    pot.add(x, player);
                }
            }
            pots.emplace_back(pot);
        }

        // print pots and contributions
        for (const Pot &pot : pots)
        {
            std::ostringstream oss;
            for (int player : pot.contributors())
                oss << " " << name_of(player);
            std::string contributors = oss.str();
            broadcast("pot has %d chips contributed by%s", pot.amount(), contributors.c_str());
        }

        // only one player left, do not deal more cards, and do not require showdown
        if (all_except_one_fold())
            return false;

        // reset *after* the loop to keep blinds
        reset_current_bets();
        return true;
    }

    template<class Container>
    bool all_zero(const Container &v)
    {
        for (size_t i = 0; i < v.size(); i++)
            if (v[i] != 0)
                return false;
        return true;
    }

    template<class Container>
    int minimum_positive(const Container &v)
    {
        int result = 0;
        for (size_t i = 0; i < v.size(); i++)
            if (v[i] > 0 && (result == 0 || v[i] < result))
                result = v[i];
        return result;
    }

    bool all_players_checked()
    {
        for (int player = 0; player < n; player++)
        {
            if (folded[player]) continue;
            if (!checked[player]) return false;
        }
        return true;
    }

    bool all_players_actioned()
    {
        for (int player = 0; player < n; player++)
        {
            if (folded[player]) continue;
            if (!actioned[player]) return false;
        }
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
            int actual_bet = current_bets[player] + amount;

            if (chips[player] > amount && actual_bet < previous_bet)
            {
                std::cerr << "illegal bet: have sufficient chips but didn't bet as much as the previous player\n";
                fold(player);
            }
            else if (chips[player] > amount && actual_bet > previous_bet && actual_bet - previous_bet < blind)
            {
                std::cerr << "illegal bet: have sufficient chips but didn't raise as much as the blind\n";
                fold(player);
            }
            else
            {
                chips[player] -= amount;
                current_bets[player] += amount;

                if (amount > 0 && actual_bet == previous_bet)
                {
                    std::cerr << "player " << name_of(player) << " calls\n";
                }
                else if (amount > 0 && actual_bet > previous_bet)
                {
                    std::cerr << "player " << name_of(player) << " raises\n";
                    last_raiser = player;
                }

                if (amount == 0)
                {
                    checked[player] = true;
                    broadcast("player %s checks", name_of(player));
                }
                else
                {
                    broadcast("player %s bets %d", name_of(player), amount);
                }

                broadcast("player %s total bet is %d", name_of(player), current_bets[player]);
            }
        }
    }

    void fold(int player)
    {
        folded[player] = true;
        broadcast("player %s folds", name_of(player));
    }

    void reset_current_bets()
    {
        current_bets.resize(n);
        for (int player = 0; player < n; player++)
            current_bets[player] = 0;
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

    void receive(int player, Card &card)
    {
        std::string message;
        io.receive(player, message);
        std::istringstream iss(message);
        std::string suit;
        iss >> card.rank >> suit;
        if (suit == "club")
            card.suit = 'C';
        else if (suit == "diamond")
            card.suit = 'D';
        else if (suit == "heart")
            card.suit = 'H';
        else if (suit == "spade")
            card.suit = 'S';
        else
            std::cerr << "invalid suit " << suit << "\n";
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

    // 只有一个人没有fold
    bool all_except_one_fold()
    {
        return num_folded() == n-1;
    }

    int num_folded()
    {
        int cnt = 0;
        for (int player = 0; player < n; player++)
            if (folded[player])
                cnt++;
        return cnt;
    }

    // 所有人下注相同，或者已经all-in
    bool all_bet_amounts_are_equal()
    {
        int amt = -1;
        for (int player = 0; player < n; player++)
        {
            if (folded[player])
                continue;
            else if (amt == -1)
            {
                amt = current_bets[player];
                std::cerr << "set amt=" << amt << " by " << name_of(player) << "\n";
            }
            else if (chips[player] == 0)
                continue;
            else if (amt != current_bets[player])
            {
                std::cerr << "return false because amt<>" << current_bets[player] << " by " << name_of(player) << "\n";
                return false;
            }
        }
        std::cerr << "all_bet_amounts_are_equal=" << amt << "\n";
        return true;
    }

    bool there_is_no_possible_raise(int next_to_play)
    {
        // FIXME not sure about pre-flop round
        return is_pre_flop_round() || last_raiser == next_to_play;
    }

    bool is_pre_flop_round()
    {
        return community_cards.size() == 0;
    }

    // 找前一个还没fold的玩家
    int previous_player(int player)
    {
        for (int i = 1; i < n; i++)
        {
            int p = (player - i + n) % n;
            if (folded[p]) continue;
            std::cerr << "previous player of " << name_of(player) << " is " << name_of(p) << "\n";
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
    std::vector<bool> actioned;
    std::vector<bool> checked;
    std::vector<bool> folded;
    int last_raiser;
};

}
