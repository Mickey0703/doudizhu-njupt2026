#include "online_room_controller.h"
#include "game_core/remote_game_session.h"
#include <QDataStream>

OnlineRoomController::OnlineRoomController(QObject *parent) : QObject(parent) {}
OnlineRoomController::OnlineRoomController(const QString &host, quint16 port, QObject *parent)
    : QObject(parent), m_explicitEndpoint(true), m_endpointHost(host), m_endpointPort(port) {}

bool OnlineRoomController::isConnected() const { return m_client && m_client->isConnected(); }
int OnlineRoomController::localSeat() const { return m_session ? m_session->localSeat() : kInvalidSeat; }
const GameState &OnlineRoomController::viewState() const { return m_session ? m_session->viewState() : m_emptyState; }

void OnlineRoomController::reset()
{
    auto *session = m_session;
    auto *client = m_client;
    m_session = nullptr;
    m_client = nullptr;
    m_online = false;
    m_host = false;
    m_roomId = m_pendingRoomId = 0;
    m_localPlayerId = -1;
    m_name.clear();
    m_avatarId.clear();
    m_players.clear();
    if (session) {
        session->disconnect();
        session->deleteLater();
    }
    if (client) {
        client->disconnect();
        client->disconnectFromRelay();
        client->deleteLater();
    }
    emit roomStateChanged();
}

void OnlineRoomController::beginHost(const QString &name, const QString &avatarId) { begin(true, 0, name, avatarId); }
void OnlineRoomController::beginJoin(quint32 id, const QString &name, const QString &avatarId) { begin(false, id, name, avatarId); }

void OnlineRoomController::begin(bool host, quint32 id, const QString &name, const QString &avatarId)
{
    reset();
    m_online = true;
    m_host = host;
    m_pendingRoomId = id;
    m_name = name;
    m_avatarId = avatarId;
    if (host) m_players.insert(1, {1, name, avatarId});
    auto *client = m_explicitEndpoint ? new RelayClient(m_endpointHost, m_endpointPort, this) : new RelayClient(this);
    m_client = client;
    connect(client, &RelayClient::connected, this, [this, client] {
        if (m_client != client) return;
        if (m_host) client->createRoom(m_name, m_avatarId);
        else client->joinRoom(m_pendingRoomId, m_name, m_avatarId);
        emit connected();
    });
    connect(client, &RelayClient::disconnected, this, [this, client] {
        if (m_client != client) return;
        reset();
        emit disconnected();
    });
    connect(client, &RelayClient::roomCreated, this, [this, client](quint32 room) {
        if (m_client != client) return;
        m_roomId = room;
        m_pendingRoomId = 0;
        m_localPlayerId = client->myPlayerId();
        m_players.insert(m_localPlayerId, {m_localPlayerId, m_name, m_avatarId});
        createSession();
        emit roomStateChanged();
        if (m_client == client) emit roomCreated(room);
    });
    connect(client, &RelayClient::roomJoined, this, [this, client](quint32 room, int pid, const QList<RemotePlayer> &players) {
        if (m_client != client) return;
        m_roomId = room;
        m_pendingRoomId = 0;
        m_localPlayerId = pid;
        m_players.clear();
        for (const auto &player : players) m_players.insert(player.playerId, player);
        m_players.insert(pid, {pid, m_name, m_avatarId});
        createSession();
        emit roomStateChanged();
        if (m_client == client) emit roomJoined(room, pid, players);
    });
    connect(client, &RelayClient::roomJoinFailed, this, [this, client](const QString &reason) {
        if (m_client != client) return;
        reset();
        emit roomJoinFailed(reason);
    });
    connect(client, &RelayClient::roomDissolved, this, [this, client](const QString &reason) {
        if (m_client != client) return;
        reset();
        emit roomDissolved(reason);
    });
    connect(client, &RelayClient::playerJoined, this, [this, client](int pid, const QString &name, const QString &avatar) {
        if (m_client != client || !m_roomId) return;
        const bool added = !m_players.contains(pid);
        m_players.insert(pid, {pid, name, avatar});
        emit roomStateChanged();
        if (added && m_client == client) emit playerJoined(pid, name, avatar);
    });
    connect(client, &RelayClient::playerLeft, this, [this, client](int pid) {
        if (m_client != client || pid == m_localPlayerId || !m_players.remove(pid)) return;
        emit roomStateChanged();
        if (m_client == client) emit playerLeft(pid);
    });
    connect(client, &RelayClient::gameStarted, this, [this, client] {
        if (m_client == client && m_roomId) emit gameStarted();
    });
    connect(client, &RelayClient::errorOccurred, this, [this, client](const QString &error) {
        if (m_client != client) return;
        reset();
        emit errorOccurred(error);
    });
    connect(client, &RelayClient::gameMessageReceived, this, [this, client](quint8 type, const QByteArray &payload) {
        if (m_client != client || !m_roomId) return;
        if (type == NetMsg::GameSub::SvrEvent || type == NetMsg::GameSub::SvrRejected) {
            if (m_session) m_session->handleServerMessage(type, payload);
        } else if (type == NetMsg::GameSub::ChatBroadcast) {
            QDataStream in(payload);
            in.setVersion(QDataStream::Qt_6_0);
            qint32 pid = -1;
            QString text;
            in >> pid >> text;
            if (in.status() == QDataStream::Ok) emit chatReceived(pid, text);
        }
    });
    client->connectToRelay();
    emit roomStateChanged();
}

void OnlineRoomController::createSession()
{
    if (m_session || !m_client) return;
    m_session = new RemoteGameSession(m_client, this);
    m_session->setLocalSeat(m_localPlayerId - 1);
    auto *session = m_session;
    connect(session, &RemoteGameSession::eventApplied, this, [this, session](const GameEvent &event) {
        if (m_session == session) emit eventApplied(event);
    });
    connect(session, &RemoteGameSession::commandRejected, this, [this, session](GameError error) {
        if (m_session == session) emit commandRejected(error);
    });
}

void OnlineRoomController::sendStartGame()
{
    if (m_host && m_roomId && isConnected() && playerCount() == kPlayerCount) m_client->sendStartGame();
}
void OnlineRoomController::sendChat(const QString &text)
{
    if (m_roomId && isConnected()) m_client->sendChat(text);
}
bool OnlineRoomController::submit(const GameCommand &command)
{
    GameError error = GameError::None;
    if (!m_online || !m_roomId || !m_session || !isConnected()) error = GameError::InvalidPhase;
    else if (!isValidSeat(localSeat())) error = GameError::InvalidSeat;
    else if ((command.type == GameCommandType::CallLord && viewState().phase != GamePhase::CallingLord)
             || ((command.type == GameCommandType::PlayCards || command.type == GameCommandType::Pass) && viewState().phase != GamePhase::Playing)
             || (command.type != GameCommandType::CallLord && command.type != GameCommandType::PlayCards && command.type != GameCommandType::Pass)) error = GameError::InvalidPhase;
    else if (viewState().currentSeat != localSeat()) error = GameError::NotCurrentPlayer;
    if (error != GameError::None) { emit commandRejected(error); return false; }
    auto localCommand = command;
    localCommand.seat = localSeat();
    m_session->sendCommand(localCommand);
    return true;
}
bool OnlineRoomController::callLord(int bid) { return submit(GameCommand::callLord(localSeat(), bid)); }
bool OnlineRoomController::playCards(const Cards &cards) { return submit(GameCommand::playCards(localSeat(), cards)); }
bool OnlineRoomController::pass() { return submit(GameCommand::pass(localSeat())); }
