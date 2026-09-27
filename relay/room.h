#ifndef RELAY_ROOM_H
#define RELAY_ROOM_H

#include <QObject>
#include <QTcpSocket>
#include <QList>
#include <QByteArray>
#include <QDateTime>

struct RoomMember
{
    QTcpSocket *socket = nullptr;
    int playerId = -1;
    QString name;
    QString avatarId;   ///< 头像 ID，仅透传给其它客户端（服务器不解析）
};

class Room : public QObject
{
    Q_OBJECT
public:
    explicit Room(quint32 roomId, QObject *parent = nullptr);

    quint32 id() const { return m_id; }

    /// 添加成员，返回分配的 playerId（1/2/3），房间满返回 -1
    int addMember(QTcpSocket *socket, const QString &name, const QString &avatarId = {});

    /// 移除成员，若人数归零则发射 empty()
    void removeMember(int playerId);

    /// playerId → socket（未找到返回 nullptr）
    QTcpSocket *socketForPlayer(int playerId) const;

    /// socket → playerId（未找到返回 -1）
    int playerIdForSocket(QTcpSocket *socket) const;

    QList<RoomMember> members() const { return m_members; }
    int memberCount() const { return m_members.size(); }
    bool isFull() const { return m_members.size() >= 3; }
    bool isEmpty() const { return m_members.isEmpty(); }

    /// 向除 exclude 外的所有成员广播
    void broadcastExcept(QTcpSocket *exclude, const QByteArray &packet);

    /// 向所有成员广播
    void broadcastToAll(const QByteArray &packet);

    /// 向指定 playerId 发送
    void sendToPlayer(int playerId, const QByteArray &packet);

    void updateActivity() { m_lastActivity = QDateTime::currentSecsSinceEpoch(); }
    qint64 lastActivity() const { return m_lastActivity; }

    /// 房主 playerId（第一个加入的成员）
    int hostId() const { return m_members.isEmpty() ? -1 : m_members.first().playerId; }

signals:
    void empty();  ///< 人数归零，通知 RelayServer 销毁

private:
    quint32 m_id;
    QList<RoomMember> m_members;
    qint64 m_lastActivity = 0;
};

#endif // RELAY_ROOM_H
