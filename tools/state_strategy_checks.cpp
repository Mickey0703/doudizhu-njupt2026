#include "state_strategy.h"
#include "ai_policy.h"
#include "deck.h"
#include "playhand.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
static void require(bool ok, const char *message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
static Cards cards(std::initializer_list<int> points) {
    Cards result; int counts[17] = {};
    for (int p : points) result.add(Card(static_cast<Card::CardPoint>(p), static_cast<Card::CardSuit>(++counts[p])));
    return result;
}
int main() {
    const auto began = std::chrono::steady_clock::now();
    GameState s; s.phase = GamePhase::Playing; s.currentSeat = 0;
    s.players[0].role = PlayerRole::Farmer; s.players[1].role = PlayerRole::Farmer; s.players[2].role = PlayerRole::Lord;
    s.players[0].hand = cards({1,2,3,4,5,8}); s.pendingSeat = 1; s.pendingCards = cards({1});
    s.players[2].handCount = 10;
    require(StateStrategy::decideCommand(s,0).type == GameCommandType::Pass, "allow farmer partner to retain control");
    s.players[2].handCount = 1;
    auto c = StateStrategy::decideCommand(s,0);
    require(c.type == GameCommandType::PlayCards && c.cards.maxPoint() == Card::Card_10, "block enemy last single using highest single");
    s.pendingCards.clear(); s.pendingSeat = -1;
    c = StateStrategy::decideCommand(s,0);
    require(c.type == GameCommandType::PlayCards && PlayHand(c.cards).getHandType() != PlayHand::Hand_Unknown, "legal lead");
    require(c.cards.cardCount() > 1,"lead multiple cards against enemy last single");
    require(s.players[0].hand.cardCount() == 6, "decision does not mutate state");
    require(StateStrategy::decideCommand(s,-1).seat == -1, "invalid seat");
    // Every supported family must be available as an immediate finishing play.
    const std::initializer_list<int> patterns[] = {
        {1},{1,1},{1,1,1},{1,1,1,2},{1,1,1,2,2},
        {1,1,1,2,2,2},{1,1,1,2,2,2,3,4},
        {1,1,1,2,2,2,3,3,4,4},{1,1,2,2,3,3},{1,2,3,4,5},
        {1,1,1,1},{1,1,1,1,2,3},{1,1,1,1,2,2,3,3},
        {1,1,1,1,2,2},{14,15},{14,15,1},{14,15,1,1},{14,15,1,2},
        {1,1,1,2,2,2,3,3}, // two single wings of the same rank
        {1,1,1,2,2,2,3,3,3,4,5,6} // longer plane
    };
    for (const auto &pattern : patterns) {
        s.players[0].hand = cards(pattern); s.pendingCards.clear(); s.pendingSeat = -1;
        const auto suggestion = StateStrategy::decideCommand(s,0);
        require(suggestion.cards == s.players[0].hand,"enumerate legal whole-hand family");
        require(PlayHand(suggestion.cards).getHandType() != PlayHand::Hand_Unknown,"whole hand valid");
    }
    s.players[0].hand = cards({1,2,3,4,5,6,7});
    s.pendingSeat = 2; s.pendingCards = cards({1,2,3,4,5});
    c = StateStrategy::decideCommand(s,0);
    require(c.cards.cardCount() == 5 && PlayHand(c.cards).canBeat(PlayHand(s.pendingCards)),"follow only equal-length straight");
    s.players[0].hand = cards({1,1,1,1,3}); s.pendingCards.clear();
    c = StateStrategy::decideCommand(s,0);
    require(c.cards.pointCount(Card::Card_3) == 0 || c.cards.pointCount(Card::Card_3) == 4,"do not split preserved bomb on lead");
    s.players[0].hand = cards({3,3,3}); s.pendingSeat = 1; s.pendingCards = cards({2,2,2});
    s.players[2].handCount = 10;
    require(StateStrategy::decideCommand(s,0).cards == s.players[0].hand,"finish even over farmer partner");
    s.players[0].hand = cards({1,3,3,3}); s.pendingSeat = 2; s.pendingCards = cards({2,2});
    c = StateStrategy::decideCommand(s,0);
    require(c.cards == cards({3,3}),"take lowest suits for duplicate rank");
    s.phase = GamePhase::Waiting;
    require(StateStrategy::decideCommand(s,0).seat == -1,"unknown phase invalid command");
    for (int seed = 0; seed < 1000; ++seed) {
        GameEngine engine; require(engine.startRound(Deck::shuffled(seed),seed%3).accepted,"start round");
        int moves = 0;
        while (engine.state().phase != GamePhase::RoundFinished && ++moves < 400) {
            const int seat = engine.state().currentSeat;
            const auto full = StateStrategy::decideCommand(engine.state(),seat);
            const auto projected = StateStrategy::decideCommand(engine.projectedStateFor(seat),seat);
            const auto repeated = StateStrategy::decideCommand(engine.state(),seat);
            require(full.type == repeated.type && full.cards == repeated.cards && full.bid == repeated.bid,"repeated decisions deterministic");
            require(full.type == projected.type && full.cards == projected.cards && full.bid == projected.bid,"projection decisions match");
            require(AIPolicy::performMove(engine,seat).accepted,"AI move accepted");
            require(engine.validateState() == GameError::None,"state invariant");
        }
        require(moves < 400,"game terminates");
    }
    std::cout << "StateStrategy checks passed: 1000 complete games, "
              << std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count() << " seconds\n";
}

