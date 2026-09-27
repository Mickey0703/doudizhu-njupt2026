#ifndef GAMECONTROL_ADAPTER_H
#define GAMECONTROL_ADAPTER_H

#include <QObject>
#include "game_core/local_session.h"
#include "player.h"
#include "robot.h"
#include "userplayer.h"

class ChaosEngine;

/**
 * Compatibility presentation bridge around LocalSession. Player/Robot objects
 * are read-only presentation caches; rules and AI use authoritative GameState.
 * New UI input submits commands explicitly via executeCommand(). Existing
 * signals and public compatibility methods remain available.
 * Chaos decisions stay in the session; ChaosEngine supplies visual effects.
 */
class GameControlAdapter : public QObject
{
    Q_OBJECT

public:
    // ============ GameControl兼容的公共接口 ============

    enum GameStatus {
        DispatchCard,
        CallingLord,
        PlayingHand
    };

    enum PlayerStatus {
        ThinkingForCallLord,
        PickingCard,
        Playing,
        ThinkingForPlayHand,  // Add this for Relay compatibility
        Winning
    };

    explicit GameControlAdapter(QObject *parent = nullptr);
    ~GameControlAdapter();

    // ============ 游戏控制接口 ============

    void playerInit();
    void resetCardData();
    void startLordCard();
    void becomeLord(Player *player, int bet);
    void disable();
    /// Read-only authority and explicit human command boundary for controllers.
    const GameState &state() const { return m_session->fullState(); }
    CommandResult executeCommand(const GameCommand &command);

    // ============ 信号处理 ============

    void onGrabBet(Player *player, int bet);
    void onPlayHand(Player *player, const Cards &card);

    /// 单机模式下用户输入直接驱动 LocalSession 引擎；联机模式（host 权威转发 /
    /// joiner 服务器转发）走各自网络路径，须关闭以免与引擎双路径冲突。默认开启。
    void setUserInputDrivesEngine(bool enabled) { m_userInputDrivesEngine = enabled; }

    // ============ Player访问器 ============

    Robot *getLeftRobot() const { return m_robotLeft; }
    Robot *getRightRobot() const { return m_robotRight; }
    UserPlayer *getUserPlayer() const { return m_user; }
    Player *getCurrentPlayer() const { return m_currPlayer; }
    Player *getPendPlayer() const { return m_pendPlayer; }
    Cards getPendCards() const { return m_pendCards; }

    // ============ 混沌模式支持 ============

    void setChaosEngine(ChaosEngine *engine) { m_chaosEngine = engine; }
    ChaosEngine *chaosEngine() const { return m_chaosEngine; }

    /// 把混沌改牌参数下发给底层 LocalSession（决策留会话、执行走引擎、权威状态唯一真相）。
    void setChaosParams(bool active, double disappearChance, double transformChance,
                        bool disappearOnDraw, bool transformOnPlay, bool disappearOnIdle);
    /// 玩家出牌变形入口：与 AI 共用会话层 maybeTransformPlay，命中发 chaosCardsTransformed。
    Cards maybeTransformPlayerCards(const Cards &sel);
    /// 发呆超时消牌（掷骰+执行都在会话）：真的消掉一张返回 true，供 UI 显示横幅。
    bool tryIdleVanishFor(Player *player);

    // ============ 牌堆操作 ============

    Cards getSurplusCards() const;  // For Relay mode: returns lord cards

    // ============ Relay模式支持 ============

    void setCurrentPlayer(Player *player);
    void syncPendingInfo(Player *player, const Cards &cards);

signals:
    void gameEventObserved(const GameEvent &event);
    void playerStatusChanged(Player *player, PlayerStatus status);
    void notifyGrabLordBet(Player *player, int bet, bool flag);
    void gameStatusChanged(GameStatus status);
    void notifyPlayHand(Player *player, const Cards &card);
    void pendingInfo(Player *player, const Cards &card);
    void chaosCardDisappeared(Player *player, const Card &card);
    /// 混沌出牌变形已发生（AI 座位；玩家变形在 GamePanel 本地处理）。转发自 LocalSession。
    void chaosCardsTransformed(Player *player, const Card &from, const Card &to);

private slots:
    void onSessionEvent(const GameEvent &event);
    void onSessionRoundFinished(int winnerSeat);

private:
    void setupConnections();
    void emitEventAsSignals(const GameEvent &event);
    void syncStateToPlayers();
    Player *seatToPlayer(int seat) const;
    int playerToSeat(Player *player) const;

    LocalSession *m_session;
    Robot *m_robotLeft;
    Robot *m_robotRight;
    UserPlayer *m_user;
    Player *m_currPlayer;
    Player *m_pendPlayer;
    Cards m_pendCards;
    ChaosEngine *m_chaosEngine;
    bool m_sessionActive;
    bool m_userInputDrivesEngine = true;
};

#endif // GAMECONTROL_ADAPTER_H
