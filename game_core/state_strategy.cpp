#include "state_strategy.h"
#include "playhand.h"
#include <array>
#include <algorithm>
#include <functional>
#include <limits>

namespace {
using Counts = std::array<int, Card::Card_End>;
Counts countsOf(const Cards &hand) {
    Counts counts{};
    for (const Card &card : hand.toCardList(Cards::Asc)) ++counts[card.getpoint()];
    return counts;
}

// Rank groups, not physical-card subsets: each rank selection maps to exactly
// one command, taking its lowest suits. Wings are bounded by a 20-card hand.
class Candidates {
public:
    explicit Candidates(const Cards &hand) : counts(countsOf(hand)) {
        for (const Card &card : hand.toCardList(Cards::Asc)) ranks[card.getpoint()].append(card);
    }
    void enumerate(const std::function<void(const Cards &, const Counts &)> &visit) const {
        auto offer = [&](const Counts &selection) {
            Cards result;
            for (int p = Card::Card_3; p < Card::Card_End; ++p)
                for (int i = 0; i < selection[p]; ++i) result.add(ranks[p][i]);
            visit(result, selection);
        };
        auto wings = [&](Counts base, int amount, int unit, int maxPerRank) {
            std::function<void(int,int)> choose = [&](int p, int left) {
                if (!left) { offer(base); return; }
                if (p >= Card::Card_End) return;
                choose(p + 1, left);
                if (base[p]) return; // main ranks cannot also be wings
                const int available = std::min({counts[p] / unit, maxPerRank, left});
                for (int n = 1; n <= available; ++n) {
                    base[p] = n * unit;
                    choose(p + 1, left - n);
                }
                base[p] = 0;
            };
            choose(Card::Card_3, amount);
        };
        for (int p = Card::Card_3; p < Card::Card_End; ++p) {
            for (int n = 1; n <= counts[p]; ++n) {
                Counts base{}; base[p] = n; offer(base);
                if (n == 3) { wings(base,1,1,1); wings(base,1,2,1); }
                if (n == 4) { wings(base,2,1,2); wings(base,2,2,1); }
            }
        }
        for (int unit = 1; unit <= 3; ++unit) {
            const int minimum = unit == 1 ? 5 : (unit == 2 ? 3 : 2);
            for (int start = Card::Card_3; start <= Card::Card_A; ++start) {
                Counts base{};
                for (int end = start; end <= Card::Card_A && counts[end] >= unit; ++end) {
                    base[end] = unit;
                    const int length = end - start + 1;
                    if (length < minimum) continue;
                    offer(base);
                    if (unit == 3) {
                        wings(base,length,1,2);
                        wings(base,length,2,1);
                    }
                }
            }
        }
        if (counts[Card::Card_SJ] && counts[Card::Card_BJ]) {
            Counts base{}; base[Card::Card_SJ] = base[Card::Card_BJ] = 1;
            offer(base); wings(base,1,1,1); wings(base,1,2,1); wings(base,2,1,1);
        }
    }
    Counts counts;
private:
    std::array<CardList, Card::Card_End> ranks;
};

// A small, deterministic upper-bound estimate of remaining moves. Long runs
// and triple attachments are consumed greedily; no recursive game-tree search.
int remainingMoves(Counts counts) {
    int moves = 0;
    if (counts[Card::Card_SJ] && counts[Card::Card_BJ]) {
        --counts[Card::Card_SJ]; --counts[Card::Card_BJ]; ++moves;
    }
    for (int p = Card::Card_3; p <= Card::Card_2; ++p)
        if (counts[p] == 4) { counts[p] = 0; ++moves; }
    for (int unit = 3; unit >= 1; --unit) {
        const int minimum = unit == 3 ? 2 : (unit == 2 ? 3 : 5);
        for (int start = Card::Card_3; start <= Card::Card_A;) {
            int end = start;
            while (end <= Card::Card_A && counts[end] >= unit) ++end;
            if (end - start >= minimum) {
                for (int p = start; p < end; ++p) counts[p] -= unit;
                ++moves;
            }
            start = end + 1;
        }
    }
    for (int p = Card::Card_3; p < Card::Card_End; ++p) {
        if (counts[p] != 3) continue;
        counts[p] = 0; ++moves;
        for (int q = Card::Card_3; q < Card::Card_End; ++q)
            if (counts[q] == 1 || counts[q] == 2) { counts[q] = 0; break; }
    }
    for (int n : counts) if (n) ++moves;
    return moves;
}
}

GameCommand StateStrategy::decideCommand(const GameState &state, int seat) {
    if (!isValidSeat(seat) || state.currentSeat != seat)
        return GameCommand::pass(kInvalidSeat);
    const Cards &hand = state.players[seat].hand;
    if (state.phase == GamePhase::CallingLord) {
        int weight = hand.pointCount(Card::Card_BJ)*6 + hand.pointCount(Card::Card_SJ)*5
                   + hand.pointCount(Card::Card_2)*4 + hand.pointCount(Card::Card_A)*3
                   + hand.pointCount(Card::Card_K)*2;
        const int desired = weight >= 18 ? 3 : (weight >= 14 ? 2 : (weight >= 8 ? 1 : 0));
        return GameCommand::callLord(seat, desired > state.highestBid ? desired : 0);
    }
    if (state.phase != GamePhase::Playing || hand.isEmpty())
        return GameCommand::pass(kInvalidSeat);
    const bool leading = state.pendingCards.isEmpty() || state.pendingSeat == seat;
    const PlayHand pending(state.pendingCards);
    bool threat = false;
    for (int other = 0; other < kPlayerCount; ++other) {
        if (other == seat) continue;
        const bool enemy = state.players[seat].role == PlayerRole::Lord || state.players[other].role == PlayerRole::Lord;
        if (enemy && state.players[other].handCount == 1) threat = true;
    }
    const bool partner = !leading && isValidSeat(state.pendingSeat)
        && state.players[seat].role == PlayerRole::Farmer
        && state.players[state.pendingSeat].role == PlayerRole::Farmer;
    Candidates candidates(hand);
    Cards best;
    int bestScore = std::numeric_limits<int>::max();
    candidates.enumerate([&](const Cards &cards, const Counts &selection) {
        const PlayHand play(cards);
        const auto type = play.getHandType();
        if (type == PlayHand::Hand_Unknown || type == PlayHand::Hand_Pass || (!leading && !play.canBeat(pending))) return;
        const bool finish = cards.cardCount() == hand.cardCount();
        if (partner && !finish && !(threat && pending.getHandType() == PlayHand::Hand_Single)) return;
        int score = 0;
        if (finish) score = -100000;
        else {
            Counts remainder = candidates.counts;
            for (int p = Card::Card_3; p < Card::Card_End; ++p) {
                remainder[p] -= selection[p];
                if (candidates.counts[p] == 4 && selection[p] && selection[p] < 4) score += 650;
                if (p >= Card::Card_A) score += selection[p] * (p - Card::Card_K) * 9;
            }
            if (candidates.counts[Card::Card_SJ] && candidates.counts[Card::Card_BJ]
                && selection[Card::Card_SJ] != selection[Card::Card_BJ]) score += 650;
            if (type == PlayHand::Hand_Bomb || type == PlayHand::Hand_Bomb_Jokers) score += 450;
            score += remainingMoves(remainder) * 150 + play.getCardPoint() * 3 - cards.cardCount() * 2;
            if (threat && type == PlayHand::Hand_Single) {
                // Do not give an opponent with one card a cheap single.
                // Leading a multi-card pattern is safer than any guessed high
                // single. When following a single, use the strongest blocker.
                score += leading ? 1000 - play.getCardPoint() * 20
                                 : -play.getCardPoint() * 250;
            }
        }
        const CardList ordered = cards.toCardList(Cards::Asc);
        const CardList previous = best.toCardList(Cards::Asc);
        if (score < bestScore || (score == bestScore && (best.isEmpty() ||
            std::lexicographical_compare(ordered.cbegin(), ordered.cend(), previous.cbegin(), previous.cend(), lessSort)))) {
            bestScore = score; best = cards;
        }
    });
    return best.isEmpty() ? GameCommand::pass(seat) : GameCommand::playCards(seat,best);
}

