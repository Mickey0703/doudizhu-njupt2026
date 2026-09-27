#include "relay_server.h"
#include "networkdata.h"
#include "game_core/game_serialization.h"
#include <QDebug>
#include <QDataStream>
#include <QRandomGenerator>
#include <QDateTime>

namespace {
// 掉线 AI 托管每步行动前的延迟（毫秒），给真人玩家看清 AI 动作，与单机节奏接近。
constexpr int kAITakeoverDelayMs = 1000;
// N6 限流（公网防刷，精简版）：单 IP 并发连接上限。同机多客户端 + 掉线重连留足余量，
// 正常游玩碰不到；只挡脚本对单一 IP 猛刷连接。阈值放宽为 10。
constexpr int kMaxConnPerIp = 10;
}

RelayServer::RelayServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_idleCheckTimer(new QTimer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &RelayServer::onNewConnection);
    m_idleCheckTimer->setInterval(5 * 60 * 1000);
    connect(m_idleCheckTimer, &QTimer::timeout, this, &RelayServer::checkIdleRooms);
}

RelayServer::~RelayServer() { stop(); }

bool RelayServer::start(quint16 port)
{
    if (!m_server->listen(QHostAddress::Any, port))
    {
        qWarning() << "Relay server failed:" << m_server->errorString();
        return false;
    }
    // N6：限制待接受连接队列，缓解 SYN/连接洪泛把队列撑爆。
    m_server->setMaxPendingConnections(30);
    m_idleCheckTimer->start();
    qInfo() << "DouDiZhuRelay listening on port" << port;
    return true;
}

void RelayServer::stop()
{
    m_idleCheckTimer->stop();
    for (auto it = m_socketRoom.begin(); it != m_socketRoom.end(); ++it)
        it.key()->disconnectFromHost();
    qDeleteAll(m_rooms);
    m_rooms.clear();
    qDeleteAll(m_games);
    m_games.clear();
    m_socketRoom.clear();
    m_socketPid.clear();
    m_buffers.clear();
    for (auto *t : m_heartbeats) { t->stop(); t->deleteLater(); }
    m_heartbeats.clear();
    m_ipConnCount.clear();
    m_socketIp.clear();
    m_server->close();
}

bool RelayServer::isRunning() const { return m_server->isListening(); }

quint16 RelayServer::serverPort() const { return m_server->serverPort(); }

// ============ makePacket utility ============

QByteArray RelayServer::makePacket(quint8 type, const QByteArray &data)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint8(type) << quint16(data.size());
    if (!data.isEmpty())
        out.writeRawData(data.constData(), data.size());
    return packet;
}

// ============ Connection handling ============

void RelayServer::onNewConnection()
{
    while (m_server->hasPendingConnections())
    {
        QTcpSocket *socket = m_server->nextPendingConnection();

        // N6：单 IP 并发连接超限直接拒绝（防脚本对单一 IP 猛刷连接耗尽资源）。
        const QString ip = socket->peerAddress().toString();
        if (m_ipConnCount.value(ip, 0) >= kMaxConnPerIp)
        {
            qWarning() << "Rejecting connection from" << ip << ": per-IP limit reached";
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }
        m_ipConnCount[ip] = m_ipConnCount.value(ip, 0) + 1;
        m_socketIp[socket] = ip;

        connect(socket, &QTcpSocket::readyRead, this, &RelayServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &RelayServer::onDisconnected);

        auto *hb = new QTimer(this);
        hb->setSingleShot(true);
        hb->setInterval(45000); // 45s timeout, client sends every 30s
        connect(hb, &QTimer::timeout, this, [socket]() {
            qInfo() << "Heartbeat timeout";
            socket->disconnectFromHost();
        });
        m_heartbeats[socket] = hb;
        hb->start();

        qInfo() << "Client connected:" << socket->peerAddress().toString();
    }
}

void RelayServer::onReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    m_buffers[socket].append(socket->readAll());

    while (true)
    {
        // Re-fetch the buffer every iteration instead of holding a long-lived
        // reference: processMessage() can remove this socket from m_buffers
        // (e.g. LeaveRoom), which would leave a held reference dangling and
        // cause a use-after-free on the next loop turn.
        auto it = m_buffers.find(socket);
        if (it == m_buffers.end())
            return; // socket was cleaned up during processing
        QByteArray &buffer = it.value();

        if (buffer.size() < 3)
            return;

        quint8 type;
        quint16 len;

        QDataStream header(buffer);
        header.setVersion(QDataStream::Qt_6_0);
        header >> type >> len;

        if (buffer.size() < 3 + len)
            return; // incomplete, wait for more data

        QByteArray payload = buffer.mid(3, len);
        QDataStream in(&payload, QIODevice::ReadOnly);
        in.setVersion(QDataStream::Qt_6_0);
        processMessage(socket, type, len, in);

        // processMessage() may have removed the socket; re-fetch before mutating.
        it = m_buffers.find(socket);
        if (it == m_buffers.end())
            return;
        it.value().remove(0, 3 + len);
    }
}

void RelayServer::onDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) return;

    // N6：回退该 IP 的并发连接计数（用建连时记录的 IP，断开后 peerAddress 可能已失效）。
    if (m_socketIp.contains(socket))
    {
        const QString ip = m_socketIp.take(socket);
        const int n = m_ipConnCount.value(ip, 0) - 1;
        if (n > 0) m_ipConnCount[ip] = n;
        else m_ipConnCount.remove(ip);
    }

    // Clean up heartbeat timer
    if (m_heartbeats.contains(socket))
    {
        m_heartbeats[socket]->stop();
        m_heartbeats[socket]->deleteLater();
        m_heartbeats.remove(socket);
    }

    quint32 roomId = m_socketRoom.value(socket, 0);
    int pid = m_socketPid.value(socket, -1);
    Room *room = m_rooms.value(roomId, nullptr);

    if (room && pid > 0)
    {
        bool wasHost = (pid == room->hostId());

        ServerRoomGame *game = m_games.value(roomId, nullptr);
        if (wasHost)
        {
            // 房主掉线：房主持有房间身份，无人可替代，仍解散房间。
            dissolveRoom(roomId, QStringLiteral("房主断开连接"));
        }
        else if (game && game->isActive())
        {
            // 对局进行中非房主掉线：AI 托管该座位本局，牌局继续（B2 由"解散"升级为"托管"）。
            // 标记座位为 AI，广播 PlayerLeft 让其余客户端显示"托管中"，移除成员（socket 已死），
            // 然后驱动 AI——若此刻正轮到该座位，立即接管；否则等轮到时由 afterEvents 驱动。
            const int seat = pid - 1;
            game->setSeatAI(seat, true);

            QByteArray data;
            QDataStream out(&data, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << qint32(pid);
            room->broadcastExcept(socket, makePacket(NetMsg::PlayerLeft, data));

            room->removeMember(pid);
            driveAISeatIfNeeded(roomId);
        }
        else
        {
            // 未开局（大厅等待中）掉线：仅移除成员，房间继续等待补位。
            QByteArray data;
            QDataStream out(&data, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << qint32(pid);
            room->broadcastExcept(socket, makePacket(NetMsg::PlayerLeft, data));

            room->removeMember(pid);
        }
    }

    m_socketRoom.remove(socket);
    m_socketPid.remove(socket);
    m_buffers.remove(socket);
    socket->deleteLater();
}

void RelayServer::onRoomEmpty()
{
    auto *room = qobject_cast<Room *>(sender());
    if (!room) return;
    qInfo() << "[Room" << room->id() << "] empty, destroying";
    const quint32 roomId = room->id();
    m_rooms.remove(roomId);
    delete m_games.take(roomId);
    room->deleteLater();
}

void RelayServer::checkIdleRooms()
{
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QList<quint32> toRemove;
    for (auto it = m_rooms.begin(); it != m_rooms.end(); ++it)
        if (now - it.value()->lastActivity() > 600)
            toRemove.append(it.key());

    for (quint32 roomId : toRemove)
    {
        dissolveRoom(roomId, QStringLiteral("房间超时空闲，已自动关闭"));
    }
}

void RelayServer::dissolveRoom(quint32 roomId, const QString &reason)
{
    Room *room = m_rooms.value(roomId, nullptr);
    if (!room)
        return;

    if (!room->isEmpty())
    {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << reason;
        room->broadcastToAll(makePacket(NetMsg::RoomDissolved, data));
    }

    for (const auto &member : room->members())
    {
        m_socketRoom.remove(member.socket);
        m_socketPid.remove(member.socket);
    }

    m_rooms.remove(roomId);
    delete m_games.take(roomId);
    room->deleteLater();
}

// ============ Room ID generation ============

quint32 RelayServer::generateRoomId()
{
    quint32 id;
    for (int i = 0; i < 100; ++i)
    {
        id = 100000 + QRandomGenerator::global()->bounded(900000);
        if (!m_rooms.contains(id)) return id;
    }
    // Fallback linear scan
    for (id = 100000; id <= 999999; ++id)
        if (!m_rooms.contains(id)) return id;
    return 100000;
}

// ============ Message dispatching ============

void RelayServer::processMessage(QTcpSocket *socket, quint8 type, quint16 payloadLen, QDataStream &in)
{
    switch (type)
    {
    case NetMsg::CreateRoom:
    {
        QString name;
        QString avatarId;
        in >> name >> avatarId;

        // A socket may only belong to one room at a time. Creating/joining
        // again would leave the old room holding a stale socket entry, which
        // later dereferences a deleted socket on broadcast (cross-room crash).
        if (m_socketRoom.contains(socket))
        {
            QByteArray d;
            QDataStream o(&d, QIODevice::WriteOnly);
            o.setVersion(QDataStream::Qt_6_0);
            o << QStringLiteral("已在房间中，无法创建新房间");
            socket->write(makePacket(NetMsg::JoinFailed, d));
            break;
        }

        quint32 roomId = generateRoomId();
        auto *room = new Room(roomId, this);
        connect(room, &Room::empty, this, &RelayServer::onRoomEmpty);
        m_rooms[roomId] = room;

        int pid = room->addMember(socket, name, avatarId);
        m_socketRoom[socket] = roomId;
        m_socketPid[socket] = pid;

        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_0);
        out << quint32(roomId);
        socket->write(makePacket(NetMsg::RoomCreated, data));

        qInfo() << "[Room" << roomId << "] created by" << name;
        break;
    }

    case NetMsg::RelayJoinRoom:
    {
        quint32 roomId;
        QString name;
        QString avatarId;
        in >> roomId >> name >> avatarId;

        // Reject if already in a room (see CreateRoom note): prevents a socket
        // from being tracked by two rooms and dangling in the first.
        if (m_socketRoom.contains(socket))
        {
            QByteArray d;
            QDataStream o(&d, QIODevice::WriteOnly);
            o.setVersion(QDataStream::Qt_6_0);
            o << QStringLiteral("已在房间中，无法加入其它房间");
            socket->write(makePacket(NetMsg::JoinFailed, d));
            break;
        }

        Room *room = m_rooms.value(roomId, nullptr);
        if (!room)
        {
            QByteArray d;
            QDataStream o(&d, QIODevice::WriteOnly);
            o.setVersion(QDataStream::Qt_6_0);
            o << QStringLiteral("房间不存在");
            socket->write(makePacket(NetMsg::JoinFailed, d));
            break;
        }
        if (room->isFull())
        {
            QByteArray d;
            QDataStream o(&d, QIODevice::WriteOnly);
            o.setVersion(QDataStream::Qt_6_0);
            o << QStringLiteral("房间已满");
            socket->write(makePacket(NetMsg::JoinFailed, d));
            break;
        }
        // 拒绝加入进行中的对局：否则新客户端会占用空出的 pid/座位，
        // 却拿不到私有手牌，并被迫替掉线者操作权威手牌，导致视图错乱/卡局。
        if (ServerRoomGame *game = m_games.value(roomId, nullptr))
        {
            if (game->isActive())
            {
                QByteArray d;
                QDataStream o(&d, QIODevice::WriteOnly);
                o.setVersion(QDataStream::Qt_6_0);
                o << QStringLiteral("对局进行中，无法加入");
                socket->write(makePacket(NetMsg::JoinFailed, d));
                break;
            }
        }

        int pid = room->addMember(socket, name, avatarId);
        m_socketRoom[socket] = roomId;
        m_socketPid[socket] = pid;

        // JoinOk: {playerId, playerCount, [playerId, name, avatarId]...}
        QByteArray ok;
        QDataStream okOut(&ok, QIODevice::WriteOnly);
        okOut.setVersion(QDataStream::Qt_6_0);
        okOut << qint32(pid) << quint8(room->memberCount());
        for (const auto &m : room->members())
            okOut << qint32(m.playerId) << m.name << m.avatarId;
        socket->write(makePacket(NetMsg::JoinOk, ok));

        // PlayerJoined to others: {playerId, name, avatarId}
        QByteArray pj;
        QDataStream pjOut(&pj, QIODevice::WriteOnly);
        pjOut.setVersion(QDataStream::Qt_6_0);
        pjOut << qint32(pid) << name << avatarId;
        room->broadcastExcept(socket, makePacket(NetMsg::PlayerJoined, pj));

        qInfo() << "[Room" << roomId << "] player" << pid << name << "joined";
        break;
    }

    case NetMsg::LeaveRoom:
    {
        quint32 roomId = m_socketRoom.value(socket, 0);
        int pid = m_socketPid.value(socket, -1);
        Room *room = m_rooms.value(roomId, nullptr);
        if (!room || pid < 0) break;

        if (pid == room->hostId())
        {
            dissolveRoom(roomId, QStringLiteral("房主离开房间"));
            break;
        }

        ServerRoomGame *game = m_games.value(roomId, nullptr);
        bool takenOver = false;
        if (game && game->isActive())
        {
            // 对局进行中非房主主动离开：同掉线处理，AI 托管该座位本局，牌局继续。
            game->setSeatAI(pid - 1, true);
            takenOver = true;
        }

        QByteArray d;
        QDataStream o(&d, QIODevice::WriteOnly);
        o.setVersion(QDataStream::Qt_6_0);
        o << qint32(pid);
        room->broadcastExcept(socket, makePacket(NetMsg::PlayerLeft, d));

        room->removeMember(pid);
        m_socketRoom.remove(socket);
        m_socketPid.remove(socket);
        if (takenOver)
            driveAISeatIfNeeded(roomId);
        // B3：不删活 socket 的接收缓冲。socket 仍连接，删掉会丢弃同一 TCP 段里
        // LeaveRoom 之后管线化的后续消息（如立即 CreateRoom），且触发 onReadyRead
        // 的 re-fetch 守卫提前 return。缓冲由 onDisconnected/stop 在 socket 真正关闭时清理。
        break;
    }

    case NetMsg::StartGameReq:
    {
        quint32 roomId = m_socketRoom.value(socket, 0);
        int pid = m_socketPid.value(socket, -1);
        Room *room = m_rooms.value(roomId, nullptr);
        if (!room || pid != room->hostId() || room->memberCount() != 3) break;

        room->updateActivity();
        room->broadcastToAll(makePacket(NetMsg::StartGameNotify));
        qInfo() << "[Room" << roomId << "] game started";

        // 服务器权威发牌
        startAuthoritativeGame(roomId);
        break;
    }

    case NetMsg::GameRelay:
    {
        // 服务器权威模式：仅接受意图命令（SvrCommand）与聊天广播（ChatBroadcast）。
        // 所有游戏裁决在服务器 GameEngine 完成，旧的 host 权威转发路径已移除。
        quint32 roomId = m_socketRoom.value(socket, 0);
        Room *room = m_rooms.value(roomId, nullptr);
        if (!room) break;
        if (payloadLen < 1) break;
        room->updateActivity();

        quint8 subType = 0;
        in >> subType;

        if (subType == NetMsg::GameSub::SvrCommand)
        {
            handleGameCommand(socket, in);
            break;
        }

        if (subType == NetMsg::GameSub::ChatBroadcast)
        {
            // 聊天不经引擎，原样转发给同房其他成员（校验发送者 playerId 与会话一致）。
            QByteArray gamePayload;
            quint16 remaining = payloadLen - 1;
            if (remaining > 0)
            {
                gamePayload.resize(remaining);
                in.readRawData(gamePayload.data(), remaining);
            }

            int claimedId = -1;
            if (gamePayload.size() >= static_cast<int>(sizeof(qint32)))
            {
                QDataStream payloadIn(gamePayload);
                payloadIn.setVersion(QDataStream::Qt_6_0);
                qint32 pid = -1;
                payloadIn >> pid;
                claimedId = static_cast<int>(pid);
            }
            if (claimedId != m_socketPid.value(socket, -1))
                break;

            QByteArray full;
            QDataStream fOut(&full, QIODevice::WriteOnly);
            fOut.setVersion(QDataStream::Qt_6_0);
            fOut << subType;
            if (!gamePayload.isEmpty())
                fOut.writeRawData(gamePayload.constData(), gamePayload.size());
            room->broadcastExcept(socket, makePacket(NetMsg::GameRelay, full));
            break;
        }

        // 其它子类型（旧 host 权威 Notify/Req）在服务器权威下无效，忽略。
        break;
    }

    case NetMsg::Heartbeat:
    {
        quint32 roomId = m_socketRoom.value(socket, 0);
        if (Room *room = m_rooms.value(roomId, nullptr))
            room->updateActivity();
        if (m_heartbeats.contains(socket))
        {
            m_heartbeats[socket]->stop();
            m_heartbeats[socket]->start();
        }
        socket->write(makePacket(NetMsg::HeartbeatAck));
        break;
    }

    default:
        qDebug() << "Unknown relay msg type:" << type;
        break;
    }
}

// ============ 服务器权威游戏 ============

QByteArray RelayServer::makeEventPacket(const GameEvent &event)
{
    // GameRelay 外层帧内：{ subType=SvrEvent, 序列化的 GameEvent }
    QByteArray inner;
    QDataStream innerOut(&inner, QIODevice::WriteOnly);
    innerOut.setVersion(QDataStream::Qt_6_0);
    innerOut << quint8(NetMsg::GameSub::SvrEvent);
    GameSerialization::writeEvent(innerOut, event);
    return makePacket(NetMsg::GameRelay, inner);
}

void RelayServer::routeGameEvents(Room *room, const QVector<GameEvent> &events)
{
    if (!room)
        return;

    for (const GameEvent &event : events)
    {
        const QByteArray packet = makeEventPacket(event);

        // 唯一的私有事件：PrivateHandDealt（cards 是该座位手牌）
        // 只发给对应座位；其余事件均为公开信息，广播给全部成员。
        if (event.type == GameEventType::PrivateHandDealt)
        {
            const int playerId = event.seat + 1;  // seat 0/1/2 → playerId 1/2/3
            room->sendToPlayer(playerId, packet);
        }
        else
        {
            room->broadcastToAll(packet);
        }
    }
}

void RelayServer::afterEvents(quint32 roomId, const QVector<GameEvent> &events)
{
    // 全员不叫流局：引擎发 RoundVoided 并置回 Waiting（isActive()=false）。
    // 服务器随即用新种子自动重开一局，避免房间卡死。
    for (const GameEvent &e : events)
    {
        if (e.type == GameEventType::RoundVoided)
        {
            startAuthoritativeGame(roomId);
            return;   // 重开内部会再驱动 AI，无需重复
        }
    }
    // 未流局：若下一个该行动的座位处于 AI 托管（有玩家掉线），驱动它。
    driveAISeatIfNeeded(roomId);
}

void RelayServer::driveAISeatIfNeeded(quint32 roomId)
{
    ServerRoomGame *game = m_games.value(roomId, nullptr);
    Room *room = m_rooms.value(roomId, nullptr);
    if (!game || !room || !game->isActive())
        return;

    const int seat = game->state().currentSeat;
    if (!isValidSeat(seat) || !game->isSeatAI(seat))
        return;

    // 延迟一步再出，让真人玩家看清 AI 的动作（与单机 AI 节奏一致）。
    // QTimer 挂在 this 上；用 roomId 而非裸指针，回调里重新查表，避免房间已被销毁的悬垂访问。
    QTimer::singleShot(kAITakeoverDelayMs, this, [this, roomId]() {
        ServerRoomGame *g = m_games.value(roomId, nullptr);
        Room *r = m_rooms.value(roomId, nullptr);
        if (!g || !r || !g->isActive())
            return;
        const int s = g->state().currentSeat;
        if (!isValidSeat(s) || !g->isSeatAI(s))
            return;

        CommandResult result = g->stepAI();
        if (result.accepted)
        {
            routeGameEvents(r, result.events);
            afterEvents(roomId, result.events);  // 递归：链到下一个 AI 座位或自动重开
        }
    });
}

void RelayServer::startAuthoritativeGame(quint32 roomId)
{
    Room *room = m_rooms.value(roomId, nullptr);
    if (!room)
        return;

    // 拒绝对进行中的对局重新发牌：否则房主可反复 StartGameReq 重摇自己手牌作弊，
    // 或恶意刷新打断牌局。只有无局或上一局已结束才允许开新局。
    if (ServerRoomGame *existing = m_games.value(roomId, nullptr)) {
        if (existing->isActive()) {
            qWarning() << "[Room" << roomId << "] StartGameReq ignored: game already active";
            return;
        }
    }

    // 区分两种开局：
    // - 续局（continuation）：托管中(hasAISeat)的房间因全员不叫流局而自动重开。此时
    //   可能已有玩家掉线（memberCount<3），必须保留 AI 托管标记继续代打，且不能被
    //   memberCount 守卫挡住，否则牌局卡死。
    // - 全新开局：房主发起(memberCount==3，全是真人)。校验人数并清空任何残留的 AI 标记
    //   （被托管座位空出后可能有新玩家补位占用同一 pid，须清掉旧托管）。
    ServerRoomGame *game = m_games.value(roomId, nullptr);
    const bool continuation = (game && game->hasAISeat());
    if (!continuation)
    {
        if (room->memberCount() != 3)
            return;
    }

    // 复用该房间已有的（非活跃）游戏实例，而不是每次重建(B1)：GameEngine 设计为
    // 跨局累计比分——startRound 接受 RoundFinished 相位并把上局 players[].score 带入
    // 新局。若每次 delete+new，累计比分被清零。仅在房间尚无游戏时才创建新实例。
    if (!game)
    {
        game = new ServerRoomGame();
        m_games[roomId] = game;
    }
    if (!continuation)
        game->clearAISeats();

    // 洗牌种子必须不可预测(M-seed)：用时间戳会让任何知道大致开局时刻的人算出
    // 全部手牌。改用系统 CSPRNG。
    const quint32 seed = QRandomGenerator::system()->generate();
    CommandResult result = game->startRound(seed, 0);
    if (!result.accepted)
    {
        qWarning() << "[Room" << roomId << "] startRound rejected, error"
                   << static_cast<int>(result.error);
        return;
    }

    routeGameEvents(room, result.events);
    qInfo() << "[Room" << roomId << "] authoritative deal complete"
            << (continuation ? "(AI-takeover continuation)" : "");

    // 若开局后首个座位就处于 AI 托管，立即驱动。
    driveAISeatIfNeeded(roomId);
}

void RelayServer::handleGameCommand(QTcpSocket *socket, QDataStream &in)
{
    const quint32 roomId = m_socketRoom.value(socket, 0);
    const int pid = m_socketPid.value(socket, -1);
    Room *room = m_rooms.value(roomId, nullptr);
    ServerRoomGame *game = m_games.value(roomId, nullptr);
    if (!room || !game || pid < 1 || pid > 3)
        return;

    // 反序列化命令；座位由会话推导，忽略载荷 seat
    GameCommand command = GameSerialization::readCommand(in);
    const int seat = pid - 1;

    CommandResult result = game->applyCommand(seat, command);

    if (result.accepted)
    {
        routeGameEvents(room, result.events);
        // 善后：全员不叫流局→自动重开；否则若下一座位被 AI 托管（有玩家掉线）则驱动它。
        afterEvents(roomId, result.events);
    }
    else
    {
        // 仅回发送者一个拒绝
        QByteArray inner;
        QDataStream innerOut(&inner, QIODevice::WriteOnly);
        innerOut.setVersion(QDataStream::Qt_6_0);
        innerOut << quint8(NetMsg::GameSub::SvrRejected)
                 << quint8(result.error);
        socket->write(makePacket(NetMsg::GameRelay, inner));
    }
}
