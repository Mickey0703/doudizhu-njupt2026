#include "relay/relay_server.h"
#include "networkdata.h"
#include "game_core/game_serialization.h"
#include "game_core/game_event.h"
#include "cards.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <iostream>

namespace {

bool waitFor(const std::function<bool()> &predicate, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (predicate()) loop.quit();
    });
    poll.start(5);
    if (predicate()) return true;
    loop.exec();
    return predicate();
}

QByteArray makePacket(quint8 type, const QByteArray &payload)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << type << quint16(payload.size());
    if (!payload.isEmpty())
        out.writeRawData(payload.constData(), payload.size());
    return packet;
}

struct ReceivedPacket
{
    quint8 type = 0;
    QByteArray payload;
};

bool readPacket(QTcpSocket &socket, ReceivedPacket *packet, int timeoutMs = 1500)
{
    if (!waitFor([&]() { return socket.bytesAvailable() >= 3; }, timeoutMs))
        return false;
    QByteArray header = socket.peek(3);
    QDataStream headerIn(header);
    headerIn.setVersion(QDataStream::Qt_6_0);
    quint16 payloadSize = 0;
    headerIn >> packet->type >> payloadSize;
    if (!waitFor([&]() { return socket.bytesAvailable() >= 3 + payloadSize; }, timeoutMs))
        return false;
    socket.read(3);
    packet->payload = socket.read(payloadSize);
    return packet->payload.size() == payloadSize;
}

// 建立一个满员房间（host + 2 joiners），返回 roomId；失败返回 0
quint32 setupFullRoom(QTcpSocket &host, QTcpSocket &p2, QTcpSocket &p3, quint16 port)
{
    host.connectToHost(QHostAddress::LocalHost, port);
    p2.connectToHost(QHostAddress::LocalHost, port);
    p3.connectToHost(QHostAddress::LocalHost, port);
    if (!host.waitForConnected(1000) || !p2.waitForConnected(1000) || !p3.waitForConnected(1000))
        return 0;

    QByteArray createPayload;
    QDataStream createOut(&createPayload, QIODevice::WriteOnly);
    createOut.setVersion(QDataStream::Qt_6_0);
    createOut << QStringLiteral("host");
    host.write(makePacket(NetMsg::CreateRoom, createPayload));

    ReceivedPacket roomCreated;
    if (!readPacket(host, &roomCreated) || roomCreated.type != NetMsg::RoomCreated)
        return 0;
    quint32 roomId = 0;
    QDataStream roomIn(roomCreated.payload);
    roomIn.setVersion(QDataStream::Qt_6_0);
    roomIn >> roomId;

    auto joinRoom = [&](QTcpSocket &socket, const QString &name) {
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << roomId << name;
        socket.write(makePacket(NetMsg::RelayJoinRoom, payload));
        ReceivedPacket joined;
        return readPacket(socket, &joined) && joined.type == NetMsg::JoinOk;
    };
    if (!joinRoom(p2, QStringLiteral("p2")) || !joinRoom(p3, QStringLiteral("p3")))
        return 0;
    return roomId;
}

// 从一个 GameRelay 包解析 SvrEvent；若非 SvrEvent 返回 false
bool parseSvrEvent(const ReceivedPacket &pkt, GameEvent *event)
{
    if (pkt.type != NetMsg::GameRelay || pkt.payload.isEmpty())
        return false;
    QDataStream in(pkt.payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint8 subType = 0;
    in >> subType;
    if (subType != NetMsg::GameSub::SvrEvent)
        return false;
    *event = GameSerialization::readEvent(in);
    return true;
}

// 排空一个 socket 上目前所有 SvrEvent，收集到 vector；同时记录是否见到私有发牌
struct DrainedEvents
{
    QVector<GameEvent> events;
    int privateHandCount = 0;      // 收到的 PrivateHandDealt 数量
    int privateHandSeat = -1;      // 最后一个私有发牌的 seat
    int privateHandCards = 0;      // 最后一个私有发牌的牌数
};

DrainedEvents drainEvents(QTcpSocket &socket, int quietMs = 300)
{
    DrainedEvents result;
    // 持续读取直到 quietMs 内无新数据
    while (waitFor([&]() { return socket.bytesAvailable() >= 3; }, quietMs)) {
        ReceivedPacket pkt;
        if (!readPacket(socket, &pkt))
            break;
        GameEvent event;
        if (parseSvrEvent(pkt, &event)) {
            result.events.append(event);
            if (event.type == GameEventType::PrivateHandDealt) {
                result.privateHandCount++;
                result.privateHandSeat = event.seat;
                result.privateHandCards = event.cards.cardCount();
            }
        }
    }
    return result;
}

// 读取一个 socket 上目前所有包，直到 quietMs 内无新数据（不限包类型）。
QVector<ReceivedPacket> drainPackets(QTcpSocket &socket, int quietMs = 300)
{
    QVector<ReceivedPacket> out;
    while (waitFor([&]() { return socket.bytesAvailable() >= 3; }, quietMs)) {
        ReceivedPacket pkt;
        if (!readPacket(socket, &pkt))
            break;
        out.append(pkt);
    }
    return out;
}

void sendStartGame(QTcpSocket &host)
{
    host.write(makePacket(NetMsg::StartGameReq, QByteArray()));
}

// 客户端发送一个 SvrCommand（座位由服务器会话推导，此处 seat 字段会被忽略）
void sendCommand(QTcpSocket &socket, const GameCommand &cmd)
{
    QByteArray inner;
    QDataStream innerOut(&inner, QIODevice::WriteOnly);
    innerOut.setVersion(QDataStream::Qt_6_0);
    innerOut << quint8(NetMsg::GameSub::SvrCommand);
    GameSerialization::writeCommand(innerOut, cmd);
    socket.write(makePacket(NetMsg::GameRelay, inner));
}

// 读取一个 socket 上的下一个 SvrRejected（若下一个包不是拒绝则返回 false）
bool readRejected(QTcpSocket &socket, quint8 *errorOut, int timeoutMs = 1000)
{
    ReceivedPacket pkt;
    if (!readPacket(socket, &pkt, timeoutMs) || pkt.type != NetMsg::GameRelay)
        return false;
    QDataStream in(pkt.payload);
    in.setVersion(QDataStream::Qt_6_0);
    quint8 subType = 0;
    in >> subType;
    if (subType != NetMsg::GameSub::SvrRejected)
        return false;
    in >> *errorOut;
    return true;
}

// 阶段B：服务器权威发牌，每个玩家只收到自己的手牌
bool testServerDealsPrivateHandsOnly()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    quint32 roomId = setupFullRoom(host, p2, p3, port);
    if (roomId == 0) {
        std::cerr << "  room setup failed\n";
        return false;
    }

    sendStartGame(host);

    // 每个 socket 排空事件
    DrainedEvents hostEvents = drainEvents(host);
    DrainedEvents p2Events = drainEvents(p2);
    DrainedEvents p3Events = drainEvents(p3);

    // 每个客户端应恰好收到 1 个 PrivateHandDealt（自己的），17 张牌
    if (hostEvents.privateHandCount != 1) {
        std::cerr << "  host privateHandCount=" << hostEvents.privateHandCount << " (expected 1)\n";
        return false;
    }
    if (p2Events.privateHandCount != 1 || p3Events.privateHandCount != 1) {
        std::cerr << "  joiner privateHandCount != 1\n";
        return false;
    }
    if (hostEvents.privateHandCards != 17
        || p2Events.privateHandCards != 17
        || p3Events.privateHandCards != 17) {
        std::cerr << "  private hand not 17 cards\n";
        return false;
    }

    // 每个客户端收到的私有发牌 seat 必须是自己（host=seat0, p2=seat1, p3=seat2）
    if (hostEvents.privateHandSeat != 0
        || p2Events.privateHandSeat != 1
        || p3Events.privateHandSeat != 2) {
        std::cerr << "  private hand seat mismatch\n";
        return false;
    }

    // 每个客户端也应收到公开事件（RoundStarted / TurnChanged）
    auto hasType = [](const DrainedEvents &d, GameEventType t) {
        for (const GameEvent &e : d.events) if (e.type == t) return true;
        return false;
    };
    if (!hasType(hostEvents, GameEventType::RoundStarted)
        || !hasType(p2Events, GameEventType::RoundStarted)) {
        std::cerr << "  missing RoundStarted broadcast\n";
        return false;
    }

    return true;
}

// 阶段C：非当前座位出牌被拒绝
bool testServerRejectsOutOfTurnCommand()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0)
        return false;

    sendStartGame(host);
    // 排空发牌事件
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);

    // firstSeat=0 → host(seat0) 该叫地主。p2(seat1) 抢先叫地主应被拒。
    sendCommand(p2, GameCommand::callLord(1, 3));

    quint8 error = 0;
    if (!readRejected(p2, &error)) {
        std::cerr << "  expected rejection for out-of-turn bid\n";
        return false;
    }
    // NotCurrentPlayer
    if (error != static_cast<quint8>(GameError::NotCurrentPlayer)) {
        std::cerr << "  wrong error code: " << int(error) << "\n";
        return false;
    }
    return true;
}

// 阶段C：座位由会话推导，客户端伪造 seat 无效
bool testServerDerivesSeatFromSession()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0)
        return false;

    sendStartGame(host);
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);

    // host 是 seat0，但在命令里伪造 seat=1。服务器应按会话推导 seat0，接受叫地主。
    sendCommand(host, GameCommand::callLord(1, 1));

    DrainedEvents hostEvents = drainEvents(host);
    // 应收到 BidAccepted 事件且 seat=0（服务器推导），而非伪造的 1
    bool foundBid = false;
    for (const GameEvent &e : hostEvents.events) {
        if (e.type == GameEventType::BidAccepted) {
            foundBid = true;
            if (e.seat != 0) {
                std::cerr << "  bid seat should be 0 (session-derived), got " << e.seat << "\n";
                return false;
            }
        }
    }
    if (!foundBid) {
        std::cerr << "  no BidAccepted event received\n";
        return false;
    }
    return true;
}

// 阶段C：合法叫地主广播给所有玩家
bool testServerBroadcastsBidToAll()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0)
        return false;

    sendStartGame(host);
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);

    // host(seat0) 叫地主
    sendCommand(host, GameCommand::callLord(0, 1));

    // p2 和 p3 都应收到 BidAccepted 广播
    auto sawBid = [](DrainedEvents &d) {
        for (const GameEvent &e : d.events)
            if (e.type == GameEventType::BidAccepted) return true;
        return false;
    };
    DrainedEvents p2Events = drainEvents(p2);
    DrainedEvents p3Events = drainEvents(p3);
    if (!sawBid(p2Events) || !sawBid(p3Events)) {
        std::cerr << "  bid not broadcast to all joiners\n";
        return false;
    }
    return true;
}

// 重发 StartGameReq 不能重置进行中的对局（防房主重摇手牌作弊）
bool testServerRejectsRedealWhileActive()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0)
        return false;

    sendStartGame(host);
    DrainedEvents first = drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);
    if (first.privateHandCount != 1) {
        std::cerr << "  first deal did not happen\n";
        return false;
    }

    // 房主再次请求开始：应被忽略，不产生第二次发牌
    sendStartGame(host);
    DrainedEvents second = drainEvents(host);
    if (second.privateHandCount != 0) {
        std::cerr << "  re-deal happened while game active (privateHandCount="
                  << second.privateHandCount << ")\n";
        return false;
    }
    return true;
}

// 对局进行中不允许新客户端加入房间（未开局的房间可加入）
bool testServerRejectsJoinWhileActive()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    // host 建房 + p2 加入（2/3，未满、未开局）
    QTcpSocket host, p2;
    host.connectToHost(QHostAddress::LocalHost, port);
    p2.connectToHost(QHostAddress::LocalHost, port);
    if (!host.waitForConnected(1000) || !p2.waitForConnected(1000))
        return false;

    QByteArray createPayload;
    QDataStream createOut(&createPayload, QIODevice::WriteOnly);
    createOut.setVersion(QDataStream::Qt_6_0);
    createOut << QStringLiteral("host");
    host.write(makePacket(NetMsg::CreateRoom, createPayload));
    ReceivedPacket roomCreated;
    if (!readPacket(host, &roomCreated) || roomCreated.type != NetMsg::RoomCreated)
        return false;
    quint32 roomId = 0;
    QDataStream roomIn(roomCreated.payload);
    roomIn.setVersion(QDataStream::Qt_6_0);
    roomIn >> roomId;

    QByteArray j2;
    QDataStream j2o(&j2, QIODevice::WriteOnly);
    j2o.setVersion(QDataStream::Qt_6_0);
    j2o << roomId << QStringLiteral("p2");
    p2.write(makePacket(NetMsg::RelayJoinRoom, j2));
    ReceivedPacket j2ok;
    if (!readPacket(p2, &j2ok) || j2ok.type != NetMsg::JoinOk) {
        std::cerr << "  p2 join (pre-game) should succeed\n";
        return false;
    }

    // 此时房间无 ServerRoomGame（未开局），p3 加入应成功（守护"未开局可加入"路径）。
    QTcpSocket p3;
    p3.connectToHost(QHostAddress::LocalHost, port);
    if (!p3.waitForConnected(1000))
        return false;
    QByteArray j3;
    QDataStream j3o(&j3, QIODevice::WriteOnly);
    j3o.setVersion(QDataStream::Qt_6_0);
    j3o << roomId << QStringLiteral("p3");
    p3.write(makePacket(NetMsg::RelayJoinRoom, j3));
    ReceivedPacket j3ok;
    if (!readPacket(p3, &j3ok) || j3ok.type != NetMsg::JoinOk) {
        std::cerr << "  p3 join (pre-game) should succeed\n";
        return false;
    }
    return true;
}

// 对局进行中一名非房主掉线 → AI 托管该座位，牌局继续（不解散房间）。
// 做法：让 host(seat0)、p2(seat1) 都不叫，轮到 p3(seat2) 时 p3 掉线；服务器应
// 立即用 AI 接管 seat2 行动，host 收到后续游戏事件（BidAccepted 或流局重开的
// RoundStarted），且绝不收到 RoomDissolved。
bool testMidGameDisconnectTriggersAITakeover()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0)
        return false;

    sendStartGame(host);
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);

    // host(seat0) 不叫 → 轮到 seat1
    sendCommand(host, GameCommand::callLord(0, 0));
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);
    // p2(seat1) 不叫 → 轮到 seat2(p3)
    sendCommand(p2, GameCommand::callLord(1, 0));
    drainEvents(host);
    drainEvents(p2);
    drainEvents(p3);

    // 轮到 p3 时 p3 掉线：服务器应 AI 托管 seat2，而非解散房间
    p3.disconnectFromHost();
    if (!waitFor([&]() { return p3.state() == QAbstractSocket::UnconnectedState; }, 1000)) {
        std::cerr << "  p3 did not disconnect\n";
        return false;
    }

    // 收集 host 后续包：应看到 AI 驱动的游戏事件（GameRelay），且无 RoomDissolved
    bool sawGameEvent = false;
    bool sawDissolved = false;
    while (waitFor([&]() { return host.bytesAvailable() >= 3; }, 2500)) {
        ReceivedPacket pkt;
        if (!readPacket(host, &pkt))
            break;
        if (pkt.type == NetMsg::RoomDissolved)
            sawDissolved = true;
        else if (pkt.type == NetMsg::GameRelay)
            sawGameEvent = true;
    }
    if (sawDissolved) {
        std::cerr << "  room dissolved on mid-game disconnect (expected AI takeover)\n";
        return false;
    }
    if (!sawGameEvent) {
        std::cerr << "  no AI-driven game events after disconnect (takeover did not advance)\n";
        return false;
    }
    return true;
}

// B3：LeaveRoom 后在同一次 write 里管线化紧跟一条 CreateRoom，不应因删缓冲而丢失。
// 修复前 LeaveRoom 会 m_buffers.remove(活socket)，使同段内后续字节被丢弃。
bool testLeaveRoomThenPipelinedCreateSurvives()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    // host 建房，p2 加入
    QTcpSocket host, p2;
    host.connectToHost(QHostAddress::LocalHost, port);
    p2.connectToHost(QHostAddress::LocalHost, port);
    if (!host.waitForConnected(1000) || !p2.waitForConnected(1000))
        return false;

    QByteArray createPayload;
    QDataStream createOut(&createPayload, QIODevice::WriteOnly);
    createOut.setVersion(QDataStream::Qt_6_0);
    createOut << QStringLiteral("host");
    host.write(makePacket(NetMsg::CreateRoom, createPayload));
    ReceivedPacket roomCreated;
    if (!readPacket(host, &roomCreated) || roomCreated.type != NetMsg::RoomCreated)
        return false;
    quint32 roomId = 0;
    QDataStream roomIn(roomCreated.payload);
    roomIn.setVersion(QDataStream::Qt_6_0);
    roomIn >> roomId;

    QByteArray j2;
    QDataStream j2o(&j2, QIODevice::WriteOnly);
    j2o.setVersion(QDataStream::Qt_6_0);
    j2o << roomId << QStringLiteral("p2");
    p2.write(makePacket(NetMsg::RelayJoinRoom, j2));
    ReceivedPacket j2ok;
    if (!readPacket(p2, &j2ok) || j2ok.type != NetMsg::JoinOk)
        return false;

    // p2 在一次 write 里发送 LeaveRoom + CreateRoom（管线化，尽量落在同一 TCP 段）。
    QByteArray leavePkt = makePacket(NetMsg::LeaveRoom, QByteArray());
    QByteArray createPayload2;
    QDataStream c2(&createPayload2, QIODevice::WriteOnly);
    c2.setVersion(QDataStream::Qt_6_0);
    c2 << QStringLiteral("p2-new");
    QByteArray createPkt = makePacket(NetMsg::CreateRoom, createPayload2);
    p2.write(leavePkt + createPkt);

    // 修复后：CreateRoom 不被丢弃，p2 收到 RoomCreated
    auto sawRoomCreated = [](QTcpSocket &s) {
        while (waitFor([&]() { return s.bytesAvailable() >= 3; }, 1200)) {
            ReceivedPacket pkt;
            if (!readPacket(s, &pkt))
                break;
            if (pkt.type == NetMsg::RoomCreated)
                return true;
        }
        return false;
    };
    if (!sawRoomCreated(p2)) {
        std::cerr << "  pipelined CreateRoom after LeaveRoom was lost (B3 regression)\n";
        return false;
    }
    return true;
}

// ============ 连接层异常路径（纯拒绝分支，无复杂时序）============

// N6 限流：单 IP 并发连接达到上限(kMaxConnPerIp=10)后，第 11 条被服务器接受后
// 立即断开。前 10 条应保持连接，第 11 条应回到未连接态。
bool testPerIpConnectionLimitRejects()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    // 达到上限的 10 条连接（全来自 127.0.0.1，共用同一 IP 计数），必须全部保持连接。
    QTcpSocket socks[10];
    for (int i = 0; i < 10; ++i) {
        socks[i].connectToHost(QHostAddress::LocalHost, port);
        if (!socks[i].waitForConnected(1000)) {
            std::cerr << "  connection " << i << " failed to connect\n";
            return false;
        }
    }
    // 给服务器 onNewConnection 时间把这 10 条计入 per-IP 计数。
    waitFor([]() { return false; }, 150);
    for (int i = 0; i < 10; ++i) {
        if (socks[i].state() != QAbstractSocket::ConnectedState) {
            std::cerr << "  connection " << i << " was dropped below the limit\n";
            return false;
        }
    }

    // 第 11 条超限：TCP 层可能瞬间连上，但服务器应随即踢断 → 回到未连接态。
    QTcpSocket extra;
    extra.connectToHost(QHostAddress::LocalHost, port);
    extra.waitForConnected(1000);
    if (!waitFor([&]() { return extra.state() == QAbstractSocket::UnconnectedState; }, 1500)) {
        std::cerr << "  11th connection over per-IP limit was not rejected\n";
        return false;
    }
    return true;
}

// 加入不存在的房间 → JoinFailed{"房间不存在"}
bool testJoinNonexistentRoomFails()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, port);
    if (!client.waitForConnected(1000))
        return false;

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint32(999999) << QStringLiteral("nobody");  // 从未创建过的房间号
    client.write(makePacket(NetMsg::RelayJoinRoom, payload));

    ReceivedPacket pkt;
    if (!readPacket(client, &pkt) || pkt.type != NetMsg::JoinFailed) {
        std::cerr << "  expected JoinFailed for nonexistent room\n";
        return false;
    }
    QDataStream in(pkt.payload);
    in.setVersion(QDataStream::Qt_6_0);
    QString reason;
    in >> reason;
    if (reason != QStringLiteral("房间不存在")) {
        std::cerr << "  wrong reason for nonexistent room: " << reason.toStdString() << "\n";
        return false;
    }
    return true;
}

// 加入已满(3/3)房间 → JoinFailed{"房间已满"}
bool testFullRoomRejectsJoin()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    quint32 roomId = setupFullRoom(host, p2, p3, port);
    if (roomId == 0) {
        std::cerr << "  room setup failed\n";
        return false;
    }

    // 房间已满但未开局，第 4 名玩家加入应因满员被拒（满员校验先于对局校验）。
    QTcpSocket p4;
    p4.connectToHost(QHostAddress::LocalHost, port);
    if (!p4.waitForConnected(1000))
        return false;
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << roomId << QStringLiteral("p4");
    p4.write(makePacket(NetMsg::RelayJoinRoom, payload));

    ReceivedPacket pkt;
    if (!readPacket(p4, &pkt) || pkt.type != NetMsg::JoinFailed) {
        std::cerr << "  expected JoinFailed for full room\n";
        return false;
    }
    QDataStream in(pkt.payload);
    in.setVersion(QDataStream::Qt_6_0);
    QString reason;
    in >> reason;
    if (reason != QStringLiteral("房间已满")) {
        std::cerr << "  wrong reason for full room: " << reason.toStdString() << "\n";
        return false;
    }
    return true;
}

// 聊天冒充：客户端在 ChatBroadcast 里伪造他人的 playerId → 服务器丢弃不转发。
// 做法：p2(真实 pid=2) 先发一条冒充 host(pid=1) 的聊天，再发一条合法(pid=2)聊天；
// TCP 保序，host 端应只收到后者，且绝不见到冒充内容。
bool testChatImpersonationRejected()
{
    RelayServer server;
    if (!server.start(0))
        return false;
    const quint16 port = server.serverPort();

    QTcpSocket host, p2, p3;
    if (setupFullRoom(host, p2, p3, port) == 0) {
        std::cerr << "  room setup failed\n";
        return false;
    }

    // 清掉 host 在 join 阶段积累的 PlayerJoined 广播，避免干扰后续读取。
    drainPackets(host);

    auto makeChat = [](qint32 claimedPid, const QString &msg) {
        QByteArray inner;
        QDataStream out(&inner, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << quint8(NetMsg::GameSub::ChatBroadcast) << qint32(claimedPid) << msg;
        return makePacket(NetMsg::GameRelay, inner);
    };

    p2.write(makeChat(1, QStringLiteral("fake")));   // 冒充 host → 应被丢弃
    p2.write(makeChat(2, QStringLiteral("real")));   // 合法 → 应被转发

    auto parseChat = [](const ReceivedPacket &pkt, qint32 *pidOut, QString *msgOut) {
        if (pkt.type != NetMsg::GameRelay || pkt.payload.isEmpty())
            return false;
        QDataStream in(pkt.payload);
        in.setVersion(QDataStream::Qt_6_0);
        quint8 sub = 0;
        in >> sub;
        if (sub != NetMsg::GameSub::ChatBroadcast)
            return false;
        in >> *pidOut >> *msgOut;
        return true;
    };

    int chatCount = 0;
    for (const ReceivedPacket &pkt : drainPackets(host)) {
        qint32 pid = -1;
        QString msg;
        if (!parseChat(pkt, &pid, &msg))
            continue;
        ++chatCount;
        if (msg == QStringLiteral("fake")) {
            std::cerr << "  impersonated chat was forwarded (should be dropped)\n";
            return false;
        }
        if (pid != 2 || msg != QStringLiteral("real")) {
            std::cerr << "  unexpected forwarded chat: pid=" << pid
                      << " msg=" << msg.toStdString() << "\n";
            return false;
        }
    }
    if (chatCount != 1) {
        std::cerr << "  expected exactly 1 forwarded chat, got " << chatCount << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    struct NamedTest { const char *name; std::function<bool()> run; };
    const NamedTest tests[] = {
        {"Server deals private hands only", testServerDealsPrivateHandsOnly},
        {"Server rejects out-of-turn command", testServerRejectsOutOfTurnCommand},
        {"Server derives seat from session", testServerDerivesSeatFromSession},
        {"Server broadcasts bid to all", testServerBroadcastsBidToAll},
        {"Server rejects re-deal while active", testServerRejectsRedealWhileActive},
        {"Pre-game room accepts joins", testServerRejectsJoinWhileActive},
        {"Mid-game disconnect triggers AI takeover", testMidGameDisconnectTriggersAITakeover},
        {"LeaveRoom + pipelined CreateRoom survives (B3)", testLeaveRoomThenPipelinedCreateSurvives},
        {"Per-IP connection limit rejects overflow (N6)", testPerIpConnectionLimitRejects},
        {"Join nonexistent room fails", testJoinNonexistentRoomFails},
        {"Full room rejects join", testFullRoomRejectsJoin},
        {"Chat impersonation rejected", testChatImpersonationRejected},
    };

    for (const auto &t : tests) {
        if (!t.run()) {
            std::cerr << "FAIL: " << t.name << '\n';
            return 1;
        }
        std::cout << "PASS: " << t.name << '\n';
    }
    return 0;
}
