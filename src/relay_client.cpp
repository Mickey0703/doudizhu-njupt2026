#include "relay_client.h"
#include <QDebug>
#include <QHostAddress>
#include <QtGlobal>

// ============ Relay server address ============
// 默认连本地 127.0.0.1（本地开发）。发行客户端在 configure 时用
//   cmake -DDOUDIZHU_RELAY_HOST=<公网IP> ..
// 注入编译期默认值（见 CMakeLists.txt）；运行时仍可用环境变量
// DOUDIZHU_RELAY_HOST / DOUDIZHU_RELAY_PORT 覆盖（见构造函数）。
#ifndef DOUDIZHU_RELAY_HOST
#define DOUDIZHU_RELAY_HOST "127.0.0.1"
#endif
const char *RelayClient::RELAY_HOST = DOUDIZHU_RELAY_HOST;

// ============ Constructor / Destructor ============

RelayClient::RelayClient(QObject *parent)
    : RelayClient(qEnvironmentVariable("DOUDIZHU_RELAY_HOST", RELAY_HOST),
                  static_cast<quint16>(qEnvironmentVariableIntValue("DOUDIZHU_RELAY_PORT") > 0
                                           ? qEnvironmentVariableIntValue("DOUDIZHU_RELAY_PORT")
                                           : RELAY_PORT),
                  parent)
{
}

RelayClient::RelayClient(const QString &host, quint16 port, QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_pingTimer(new QTimer(this))
    , m_host(host)
    , m_port(port)
{
    connect(m_socket, &QTcpSocket::connected,       this, &RelayClient::onConnected);
    connect(m_socket, &QTcpSocket::readyRead,        this, &RelayClient::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,     this, &RelayClient::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,    this, &RelayClient::onError);

    m_pingTimer->setInterval(30000); // 30s heartbeat
    connect(m_pingTimer, &QTimer::timeout, this, &RelayClient::sendPing);
}

RelayClient::~RelayClient()
{
    disconnectFromRelay();
}

// ============ Connection ============

void RelayClient::connectToRelay()
{
    qDebug() << "RelayClient connecting to" << m_host << m_port;
    m_socket->connectToHost(m_host, m_port);
}

void RelayClient::disconnectFromRelay()
{
    m_pingTimer->stop();
    if (m_socket->state() == QTcpSocket::ConnectedState)
    {
        if (m_roomId != 0)
            leaveRoom();
        m_socket->disconnectFromHost();
    }
}

bool RelayClient::isConnected() const
{
    return m_socket->state() == QTcpSocket::ConnectedState;
}

// ============ Slots ============

void RelayClient::onConnected()
{
    m_pingTimer->start();
    emit connected();
}

void RelayClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (m_buffer.size() >= 3)
    {
        quint8 type;
        quint16 len;

        QByteArray headerBytes = m_buffer.left(3);
        QDataStream header(headerBytes);
        header.setVersion(QDataStream::Qt_6_0);
        header >> type >> len;

        if (m_buffer.size() < 3 + len)
            return; // incomplete packet

        QByteArray payload = m_buffer.mid(3, len);
        QDataStream in(payload);
        in.setVersion(QDataStream::Qt_6_0);
        processMessage(type, in);
        m_buffer.remove(0, 3 + len);
    }
}

void RelayClient::onDisconnected()
{
    m_pingTimer->stop();
    m_roomId = 0;
    m_playerId = -1;
    m_isHost = false;
    emit disconnected();
}

void RelayClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QString err = m_socket->errorString();
    qWarning() << "RelayClient error:" << err;
    emit errorOccurred(err);
}

void RelayClient::sendPing()
{
    sendMessage(NetMsg::Heartbeat);
}

// ============ Room operations ============

void RelayClient::createRoom(const QString &playerName, const QString &avatarId)
{
    m_isHost = true;
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << playerName << avatarId;
    sendMessage(NetMsg::CreateRoom, data);
}

void RelayClient::joinRoom(quint32 roomId, const QString &playerName, const QString &avatarId)
{
    m_isHost = false;
    m_roomId = roomId;
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << roomId << playerName << avatarId;
    sendMessage(NetMsg::RelayJoinRoom, data);
}

void RelayClient::leaveRoom()
{
    sendMessage(NetMsg::LeaveRoom);
    m_roomId = 0;
    m_playerId = -1;
    m_isHost = false;
}

void RelayClient::sendStartGame()
{
    sendMessage(NetMsg::StartGameReq);
}

void RelayClient::sendChat(const QString &msg)
{
    // Chat uses GameRelay{ChatBroadcast}
    QByteArray payload;
    QDataStream pOut(&payload, QIODevice::WriteOnly);
    pOut.setVersion(QDataStream::Qt_6_0);
    pOut << qint32(m_playerId) << msg;
    sendGameRelay(NetMsg::GameSub::ChatBroadcast, payload);
}

// ============ Message construction ============

void RelayClient::sendMessage(quint8 type, const QByteArray &data)
{
    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out << quint8(type) << quint16(data.size());
    if (!data.isEmpty())
        out.writeRawData(data.constData(), data.size());
    m_socket->write(packet);
}

void RelayClient::sendGameRelay(quint8 subType, const QByteArray &payload)
{
    QByteArray full;
    QDataStream fOut(&full, QIODevice::WriteOnly);
    fOut.setVersion(QDataStream::Qt_6_0);
    fOut << subType;
    if (!payload.isEmpty())
        fOut.writeRawData(payload.constData(), payload.size());
    sendMessage(NetMsg::GameRelay, full);
}

void RelayClient::sendGameMessage(quint8 subType, const QByteArray &gamePayload)
{
    sendGameRelay(subType, gamePayload);
}

// ============ Message processing ============

void RelayClient::processMessage(quint8 type, QDataStream &in)
{
    switch (type)
    {
    case NetMsg::RoomCreated:
    {
        quint32 roomId;
        in >> roomId;
        m_roomId = roomId;
        m_playerId = 1; // host is always player 1
        emit roomCreated(roomId);
        break;
    }

    case NetMsg::JoinOk:
    {
        qint32 myPid;
        quint8 count;
        in >> myPid >> count;
        m_playerId = myPid;

        QList<RemotePlayer> players;
        for (int i = 0; i < count; ++i)
        {
            RemotePlayer rp;
            in >> rp.playerId >> rp.name >> rp.avatarId;
            players.append(rp);
        }
        emit roomJoined(m_roomId, myPid, players);
        break;
    }

    case NetMsg::JoinFailed:
    {
        QString reason;
        in >> reason;
        m_roomId = 0;
        emit roomJoinFailed(reason);
        break;
    }

    case NetMsg::PlayerJoined:
    {
        qint32 pid;
        QString name;
        QString avatarId;
        in >> pid >> name >> avatarId;
        emit playerJoined(pid, name, avatarId);
        break;
    }

    case NetMsg::PlayerLeft:
    {
        qint32 pid;
        in >> pid;
        emit playerLeft(pid);
        break;
    }

    case NetMsg::RoomDissolved:
    {
        QString reason;
        in >> reason;
        m_roomId = 0;
        m_playerId = -1;
        m_isHost = false;
        emit roomDissolved(reason);
        break;
    }

    case NetMsg::StartGameNotify:
    {
        emit gameStarted();
        break;
    }

    case NetMsg::GameRelay:
    {
        // Guard against a zero-length GameRelay body: reading subType from an
        // empty stream yields an indeterminate value that would then be
        // dispatched. Require at least the subType byte.
        if (in.device()->bytesAvailable() < 1)
            break;

        quint8 subType;
        in >> subType;

        // Read remaining payload bytes (everything after subType in this message)
        QByteArray gamePayload;
        QIODevice *dev = in.device();
        qint64 remaining = dev->bytesAvailable();
        if (remaining > 0)
        {
            gamePayload.resize(remaining);
            in.readRawData(gamePayload.data(), remaining);
        }

        emit gameMessageReceived(subType, gamePayload);
        break;
    }

    case NetMsg::HeartbeatAck:
        // Nothing to do
        break;

    default:
        qDebug() << "RelayClient unknown msg type:" << type;
        break;
    }
}
