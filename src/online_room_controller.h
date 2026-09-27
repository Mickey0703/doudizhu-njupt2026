#ifndef ONLINE_ROOM_CONTROLLER_H
#define ONLINE_ROOM_CONTROLLER_H

#include "relay_client.h"
#include "game_core/game_command.h"
#include "game_core/game_event.h"
#include "game_core/game_state.h"
#include <QMap>

class RemoteGameSession;

// Owns the entire online room lifetime; consumers render its read-only state.
class OnlineRoomController : public QObject
{
    Q_OBJECT
public:
    explicit OnlineRoomController(QObject *parent = nullptr);
    OnlineRoomController(const QString &host, quint16 port, QObject *parent = nullptr);
    void beginHost(const QString &name, const QString &avatarId);
    void beginJoin(quint32 roomId, const QString &name, const QString &avatarId);
    void reset();
    bool isOnline() const { return m_online; }
    bool isHost() const { return m_host; }
    bool isConnected() const;
    int playerCount() const { return m_players.size(); }
    int localPlayerId() const { return m_localPlayerId; }
    int localSeat() const;
    quint32 pendingRoomId() const { return m_pendingRoomId; }
    quint32 roomId() const { return m_roomId; }
    const GameState &viewState() const;
    void sendStartGame();
    void sendChat(const QString &text);
    bool submit(const GameCommand &command);
    bool callLord(int bid);
    bool playCards(const Cards &cards);
    bool pass();
signals:
    void connected();
    void disconnected();
    void roomCreated(quint32 roomId);
    void roomJoined(quint32 roomId, int playerId, const QList<RemotePlayer> &players);
    void roomJoinFailed(QString reason);
    void playerJoined(int playerId, QString name, QString avatarId);
    void playerLeft(int playerId);
    void roomDissolved(QString reason);
    void gameStarted();
    void errorOccurred(QString error);
    void eventApplied(const GameEvent &event);
    void commandRejected(GameError error);
    void chatReceived(int playerId, const QString &text);
    void roomStateChanged();
private:
    void begin(bool host, quint32 roomId, const QString &name, const QString &avatarId);
    void createSession();
    RelayClient *m_client = nullptr;
    RemoteGameSession *m_session = nullptr;
    bool m_explicitEndpoint = false;
    QString m_endpointHost;
    quint16 m_endpointPort = 0;
    bool m_online = false;
    bool m_host = false;
    quint32 m_pendingRoomId = 0;
    quint32 m_roomId = 0;
    int m_localPlayerId = -1;
    QString m_name;
    QString m_avatarId;
    QMap<int, RemotePlayer> m_players;
    const GameState m_emptyState;
};
#endif
