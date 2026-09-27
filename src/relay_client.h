#ifndef RELAY_CLIENT_H
#define RELAY_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QList>
#include "networkdata.h"

struct RemotePlayer
{
    int playerId;
    QString name;
    QString avatarId;   ///< 头像 ID（各端本地 AvatarStore 据此加载同一张图）
};

class RelayClient : public QObject
{
    Q_OBJECT
public:
    explicit RelayClient(QObject *parent = nullptr);
    RelayClient(const QString &host, quint16 port, QObject *parent = nullptr);
    ~RelayClient() override;

    // Connection (hardcoded address)
    void connectToRelay();
    void disconnectFromRelay();
    bool isConnected() const;

    // Room ops
    void createRoom(const QString &playerName, const QString &avatarId = {});
    void joinRoom(quint32 roomId, const QString &playerName, const QString &avatarId = {});
    void leaveRoom();
    void sendStartGame();

    // Game message relay — constructs and sends a GameRelay message
    void sendGameRelay(quint8 subType, const QByteArray &payload);
    void sendGameMessage(quint8 subType, const QByteArray &gamePayload);

    // Chat (sent as GameRelay{ChatBroadcast})
    void sendChat(const QString &msg);

    // Accessors
    quint32 currentRoomId() const { return m_roomId; }
    int myPlayerId() const { return m_playerId; }
    bool isHost() const { return m_isHost; }

signals:
    void connected();
    void disconnected();
    void roomCreated(quint32 roomId);
    void roomJoined(quint32 roomId, int myPid, const QList<RemotePlayer> &players);
    void roomJoinFailed(const QString &reason);
    void playerJoined(int playerId, const QString &name, const QString &avatarId);
    void playerLeft(int playerId);
    void roomDissolved(const QString &reason);
    void gameStarted();
    void gameMessageReceived(quint8 subType, const QByteArray &gamePayload);
    void errorOccurred(const QString &error);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void sendPing();

private:
    void processMessage(quint8 type, QDataStream &in);
    void sendMessage(quint8 type, const QByteArray &data = {});

    QTcpSocket *m_socket;
    QTimer *m_pingTimer;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port = RELAY_PORT;
    quint32 m_roomId = 0;
    int m_playerId = -1;
    bool m_isHost = false;

    // Hardcoded server address — MODIFY THIS to your server's public IP
    static const char *RELAY_HOST;
    static const quint16 RELAY_PORT = 9527;
};

#endif // RELAY_CLIENT_H
