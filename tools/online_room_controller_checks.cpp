#include "src/online_room_controller.h"
#include "relay/relay_server.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include <iostream>

namespace {
bool waitFor(const std::function<bool()> &predicate)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return predicate();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto check = [](bool condition, const char *message) {
        if (!condition) std::cerr << "FAIL: " << message << '\n';
        return condition;
    };
    RelayServer server;
    if (!check(server.start(0), "server starts")) return 1;
    OnlineRoomController host("127.0.0.1", server.serverPort());
    OnlineRoomController second("127.0.0.1", server.serverPort());
    OnlineRoomController third("127.0.0.1", server.serverPort());
    OnlineRoomController *clients[] = {&host, &second, &third};
    int created = 0;
    int rejected = 0;
    int passed = 0;
    int privateHands[3] = {};
    int chats = 0;
    QObject::connect(&host, &OnlineRoomController::roomCreated, [&] { ++created; });
    for (int i = 0; i < 3; ++i) {
        QObject::connect(clients[i], &OnlineRoomController::commandRejected, [&] { ++rejected; });
        QObject::connect(clients[i], &OnlineRoomController::eventApplied, [&, i](const GameEvent &event) {
            if (event.type == GameEventType::PrivateHandDealt) {
                if (event.seat == clients[i]->localSeat()) ++privateHands[i];
                else privateHands[i] = -100;
            }
            if (event.type == GameEventType::PlayerPassed) ++passed;
        });
        QObject::connect(clients[i], &OnlineRoomController::chatReceived, [&](int pid, const QString &text) {
            if (pid == second.localPlayerId() && text == "[[STK:2]]") ++chats;
        });
    }
    if (!check(!host.pass() && rejected == 1, "offline commands reject synchronously")) return 1;
    host.beginHost("host", "avatar-host");
    if (!check(waitFor([&] { return host.roomId() != 0; }), "host creates automatically")) return 1;
    second.beginJoin(host.roomId(), "second", "avatar-second");
    if (!check(waitFor([&] { return second.roomId() != 0; }), "second joins")) return 1;
    third.beginJoin(host.roomId(), "third", "avatar-third");
    if (!check(waitFor([&] { return host.playerCount() == 3 && second.playerCount() == 3 && third.playerCount() == 3; }), "all members counted")) return 1;
    second.sendChat("[[STK:2]]");
    if (!check(waitFor([&] { return chats == 2; }), "chat parsing preserves sticker text for other members")) return 1;
    host.sendStartGame();
    if (!check(waitFor([&] { return privateHands[0] == 1 && privateHands[1] == 1 && privateHands[2] == 1; }), "only local private hand arrives")) return 1;
    for (auto *client : clients) {
        for (int seat = 0; seat < 3; ++seat) {
            if (!check(client->viewState().players[seat].hand.cardCount() == (seat == client->localSeat() ? 17 : 0), "opponent hands hidden")) return 1;
        }
    }
    auto *bidder = clients[host.viewState().currentSeat];
    const int originalSeat = bidder->viewState().currentSeat;
    const int rejectsBefore = rejected;
    if (!check(bidder->callLord(4), "bid validation belongs to server")) return 1;
    if (!check(waitFor([&] { return rejected > rejectsBefore; }) && bidder->viewState().currentSeat == originalSeat, "server rejection is forwarded without changing view")) return 1;
    if (!check(bidder->submit(GameCommand::callLord((originalSeat + 1) % 3, 1)), "seat is forced to local for positive bid")) return 1;
    if (!check(bidder->viewState().currentSeat == originalSeat, "sending does not advance local state")) return 1;
    if (!check(waitFor([&] { return host.viewState().currentSeat != originalSeat; }), "server confirms positive bid")) return 1;
    bidder = clients[host.viewState().currentSeat];
    const int passingSeat = bidder->localSeat();
    if (!check(bidder->callLord(0), "zero bid sends")) return 1;
    if (!check(waitFor([&] { return host.viewState().currentSeat != passingSeat && second.viewState().currentSeat == host.viewState().currentSeat && third.viewState().currentSeat == host.viewState().currentSeat; }), "server confirms zero bid")) return 1;
    for (auto *client : clients)
        if (!check(client->viewState().highestBid == 1 && client->viewState().highestBidder == originalSeat && client->viewState().bidCount == 2, "zero bid preserves public highest bid and counts bids")) return 1;
    bidder = clients[host.viewState().currentSeat];
    if (!check(bidder->callLord(3), "lord bid sends")) return 1;
    if (!check(waitFor([&] { return host.viewState().phase == GamePhase::Playing && second.viewState().phase == GamePhase::Playing && third.viewState().phase == GamePhase::Playing; }), "lord selected")) return 1;
    auto *leader = clients[host.viewState().currentSeat];
    Cards single;
    single.add(leader->viewState().players[leader->localSeat()].hand.toCardList().first());
    if (!check(leader->playCards(single), "leader plays")) return 1;
    if (!check(waitFor([&] { return host.viewState().currentSeat != leader->localSeat() && second.viewState().currentSeat == host.viewState().currentSeat && third.viewState().currentSeat == host.viewState().currentSeat; }), "play confirmed")) return 1;
    auto *follower = clients[host.viewState().currentSeat];
    if (!check(follower->pass(), "follower passes")) return 1;
    if (!check(waitFor([&] { return passed == 3; }), "pass broadcast")) return 1;
    third.reset();
    if (!check(!third.isOnline() && third.localSeat() == -1 && third.roomId() == 0 && third.viewState().phase == GamePhase::Waiting, "reset clears session")) return 1;
    if (!check(waitFor([&] { return host.playerCount() == 2; }), "departure counted")) return 1;
    host.reset();
    if (!check(waitFor([&] { return !second.isOnline(); }), "host departure dissolves room")) return 1;
    host.beginHost("again", "avatar");
    if (!check(waitFor([&] { return host.roomId() != 0; }) && created == 2, "reentry has one creation callback")) return 1;
    third.beginJoin(0, "invalid", "avatar");
    if (!check(waitFor([&] { return !third.isOnline(); }) && third.pendingRoomId() == 0, "join failure cleans pending state")) return 1;
    server.stop();
    if (!check(waitFor([&] { return !host.isOnline(); }), "disconnect cleans state")) return 1;
    std::cout << "Online room controller checks passed\n";
    return 0;
}
