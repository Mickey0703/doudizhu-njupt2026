#ifndef RELAY_SERVER_H
#define RELAY_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QTimer>
#include <QDataStream>
#include <QByteArray>
#include "room.h"
#include "server_room_game.h"

class RelayServer : public QObject
{
    Q_OBJECT
public:
    explicit RelayServer(QObject *parent = nullptr);
    ~RelayServer() override;

    bool start(quint16 port = 9527);
    void stop();
    bool isRunning() const;

    /// 实际监听端口。以 start(0) 让 OS 选空闲端口后，回读真实端口（供测试消除端口冲突假失败）。
    quint16 serverPort() const;

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onRoomEmpty();

    
    void checkIdleRooms();

private:
    void processMessage(QTcpSocket *socket, quint8 type, quint16 payloadLen, QDataStream &in);
    void dissolveRoom(quint32 roomId, const QString &reason);
    quint32 generateRoomId();
    static QByteArray makePacket(quint8 type, const QByteArray &data = {});

    // ---- 服务器权威游戏 ----
    /// 开始一局：服务器发牌，私有下发每座位手牌，广播公开事件
    void startAuthoritativeGame(quint32 roomId);
    /// 处理客户端命令：会话推导座位→引擎验证→路由事件
    void handleGameCommand(QTcpSocket *socket, QDataStream &in);
    /// 把引擎事件按私有/公开路由给房间成员
    void routeGameEvents(Room *room, const QVector<GameEvent> &events);
    /// 路由事件后统一善后：全员不叫流局→自动重开；否则若当前座位被 AI 托管则驱动其行动。
    void afterEvents(quint32 roomId, const QVector<GameEvent> &events);
    /// 若当前座位处于 AI 托管，延迟一步后驱动其行动并递归驱动（掉线托管的推进循环）。
    void driveAISeatIfNeeded(quint32 roomId);
    /// 打包单个事件为 GameRelay{SvrEvent} 包
    static QByteArray makeEventPacket(const GameEvent &event);

    QTcpServer *m_server;
    QMap<quint32, Room *> m_rooms;
    QMap<quint32, ServerRoomGame *> m_games;  ///< 每房间的权威游戏
    QMap<QTcpSocket *, quint32> m_socketRoom;
    QMap<QTcpSocket *, int> m_socketPid;
    QMap<QTcpSocket *, QByteArray> m_buffers;
    QMap<QTcpSocket *, QTimer *> m_heartbeats;
    QTimer *m_idleCheckTimer;

    // N6 限流：单 IP 当前并发连接数；socket→IP 便于断开时回退计数（断开后 peerAddress 不可靠）。
    QMap<QString, int> m_ipConnCount;
    QMap<QTcpSocket *, QString> m_socketIp;
};

#endif // RELAY_SERVER_H
