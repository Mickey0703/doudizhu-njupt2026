#ifndef GAME_ACTION_CONTROLLER_H
#define GAME_ACTION_CONTROLLER_H

#include "game_core/game_command.h"
#include "game_core/game_state.h"
#include <QObject>
#include <QTimer>

class GameControlAdapter;
class OnlineRoomController;

struct GameActionResult {
    bool submitted = false;
    bool waitingForServer = false;
    GameError error = GameError::None;
};

// One command boundary for local/online input, hints and timeout actions.
class GameActionController : public QObject
{
    Q_OBJECT
public:
    GameActionController(GameControlAdapter *local, OnlineRoomController *online,
                         QObject *parent = nullptr);
    const GameState &state() const;
    int localSeat() const;
    bool isOurTurn() const;
    GameActionResult submit(GameCommand command);
    GameCommand hint() const;
    void startCountdown(int seconds = 25);
    void stopCountdown();
    int remainingSeconds() const { return m_seconds; }
signals:
    void secondsChanged(int seconds);
    void idleWarning();
    void timedOut(const GameActionResult &result);
private:
    void tick();
    GameControlAdapter *m_local;
    OnlineRoomController *m_online;
    QTimer m_timer;
    int m_seconds = 0;
    int m_turnSeat = kInvalidSeat;
    GamePhase m_turnPhase = GamePhase::Waiting;
    int m_turnRound = 0;
};

#endif
