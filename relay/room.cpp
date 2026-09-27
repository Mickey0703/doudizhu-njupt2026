#include "room.h"
#include <algorithm>
#include <QDebug>

Room::Room(quint32 roomId, QObject *parent)
    : QObject(parent), m_id(roomId)
{
    m_lastActivity = QDateTime::currentSecsSinceEpoch();
    qDebug() << "[Room" << m_id << "] created";
}

int Room::addMember(QTcpSocket *socket, const QString &name, const QString &avatarId)
{
    if (isFull()) return -1;
    int playerId = -1;
    for (int candidate = 1; candidate <= 3; ++candidate)
    {
        const auto it = std::find_if(m_members.begin(), m_members.end(),
            [candidate](const RoomMember &m) { return m.playerId == candidate; });
        if (it == m_members.end())
        {
            playerId = candidate;
            break;
        }
    }
    if (playerId < 0) return -1;

    RoomMember m;
    m.socket = socket;
    m.playerId = playerId;
    m.name = name;
    m.avatarId = avatarId;
    m_members.append(m);
    updateActivity();
    qDebug() << "[Room" << m_id << "] player" << m.playerId << name << "joined, count=" << m_members.size();
    return m.playerId;
}

void Room::removeMember(int playerId)
{
    auto it = std::find_if(m_members.begin(), m_members.end(),
        [playerId](const RoomMember &m) { return m.playerId == playerId; });
    if (it == m_members.end()) return;
    qDebug() << "[Room" << m_id << "] player" << it->playerId << it->name << "left, count=" << m_members.size() - 1;
    m_members.erase(it);
    updateActivity();
    if (isEmpty()) emit empty();
}

QTcpSocket *Room::socketForPlayer(int playerId) const
{
    for (const auto &m : m_members)
        if (m.playerId == playerId && m.socket && m.socket->state() == QTcpSocket::ConnectedState)
            return m.socket;
    return nullptr;
}

int Room::playerIdForSocket(QTcpSocket *socket) const
{
    for (const auto &m : m_members)
        if (m.socket == socket) return m.playerId;
    return -1;
}

void Room::broadcastExcept(QTcpSocket *exclude, const QByteArray &packet)
{
    for (const auto &m : m_members)
        if (m.socket != exclude && m.socket && m.socket->state() == QTcpSocket::ConnectedState)
            m.socket->write(packet);
}

void Room::broadcastToAll(const QByteArray &packet)
{
    for (const auto &m : m_members)
        if (m.socket && m.socket->state() == QTcpSocket::ConnectedState)
            m.socket->write(packet);
}

void Room::sendToPlayer(int playerId, const QByteArray &packet)
{
    QTcpSocket *s = socketForPlayer(playerId);
    if (s) s->write(packet);
}
