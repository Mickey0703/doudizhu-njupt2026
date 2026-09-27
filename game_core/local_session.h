#ifndef GAME_CORE_LOCAL_SESSION_H
#define GAME_CORE_LOCAL_SESSION_H

#include "game_engine.h"
#include "game_types.h"

#include <QObject>
#include <array>

/**
 * @enum PlayerType
 * @brief 玩家类型：用户控制或AI控制
 */
enum class PlayerType {
    User,  ///< 用户控制，需要等待输入
    AI     ///< AI控制，自动决策
};

/**
 * @class LocalSession
 * @brief 本地游戏会话管理器 - 协调GameEngine和AI/用户输入
 *
 * ## 职责
 *
 * LocalSession是单机游戏的核心协调者，负责：
 * 1. **玩家管理**: 区分用户玩家和AI玩家
 * 2. **AI调度**: 自动触发AI决策，模拟真实游戏节奏
 * 3. **事件转发**: 将GameEngine的事件转换为Qt信号
 * 4. **状态投影**: 为用户提供合适的状态视图
 *
 * ## 架构位置
 *
 * ```
 * GamePanel (UI)
 *     ↓ executeCommand()
 * LocalSession (会话管理)
 *     ↓ execute()
 * GameEngine (核心逻辑)
 *     ↓ events
 * LocalSession
 *     ↓ emit eventEmitted()
 * GamePanel (更新UI)
 * ```
 *
 * ## AI策略
 *
 * LocalSession集成了完整的Strategy类：
 * - **叫地主**: 基于手牌强度权重计算
 * - **首出**: 最优顺子拆分，优先消耗单张
 * - **跟牌**: 最小牌型打过对手
 * - **队友**: whetherToBeat()考虑队友情况
 *
 * ## 使用示例
 *
 * ```cpp
 * LocalSession session;
 *
 * // 配置玩家类型
 * session.setPlayerType(0, PlayerType::User);
 * session.setPlayerType(1, PlayerType::AI);
 * session.setPlayerType(2, PlayerType::AI);
 *
 * // 开始游戏
 * session.startRound();
 *
 * // 用户出牌
 * Cards cards = ...;
 * session.executeCommand(GameCommand::playCards(0, cards));
 *
 * // AI会自动执行（通过QTimer）
 * ```
 *
 * ## 信号说明
 *
 * - **eventEmitted**: 每个GameEvent都会触发，UI据此更新
 * - **roundFinished**: 游戏结束，winnerSeat是胜者座位号
 *
 * @see GameEngine, Strategy, PlayerType
 */
class LocalSession : public QObject
{
    Q_OBJECT

public:
    explicit LocalSession(QObject *parent = nullptr);
    ~LocalSession();

    // ============ Configuration ============

    /**
     * @brief 设置座位的玩家类型
     * @param seat 座位号 (0-2)
     * @param type User或AI
     *
     * 必须在startRound()之前调用
     */
    void setPlayerType(int seat, PlayerType type);

    /**
     * @brief 查询座位的玩家类型
     * @param seat 座位号 (0-2)
     * @return 该座位的PlayerType
     */
    PlayerType playerType(int seat) const;

    // ============ Session control ============

    /**
     * @brief 开始新一轮游戏
     * @param seed 随机数种子（0表示使用当前时间）
     * @return 成功返回true
     *
     * 效果：
     * - 洗牌并发牌
     * - 进入CallingLord阶段
     * - 如果第一个玩家是AI，自动触发AI决策
     */
    bool startRound(quint32 seed = 0);

    /**
     * @brief 执行游戏命令（用户或外部调用）
     * @param command 要执行的命令
     * @return 命令结果
     *
     * 如果命令被接受：
     * - 触发eventEmitted信号
     * - 如果轮到AI，自动调度AI决策
     *
     * 如果命令被拒绝：
     * - 返回错误码
     * - 状态不变
     */
    CommandResult executeCommand(const GameCommand &command);

    // ============ State queries ============

    /**
     * @brief 当前游戏阶段
     * @return CallingLord, Playing, 或 RoundFinished
     */
    GamePhase currentPhase() const;

    /**
     * @brief 当前轮到的座位号
     * @return 0, 1, 或 2
     */
    int currentSeat() const;

    /**
     * @brief 当前玩家的类型
     * @return User或AI
     */
    PlayerType currentPlayerType() const;

    /**
     * @brief 获取特定座位的投影状态（隐藏其他玩家手牌）
     * @param seat 座位号
     * @return 投影后的GameState
     *
     * 用于防止作弊 - UI只应显示用户自己的手牌
     */
    GameState projectedStateFor(int seat) const;

    /**
     * @brief 获取完整状态（调试用，生产代码慎用）
     * @return 完整的GameState，包含所有玩家手牌
     */
    const GameState &fullState() const;

    // ============ AI control ============

    /**
     * @brief 启用或禁用AI自动执行
     * @param enabled true=AI自动执行，false=需要手动triggerAIMove()
     *
     * 默认启用。禁用时用于单步调试AI逻辑。
     */
    void setAIEnabled(bool enabled);

    /**
     * @brief 手动触发一次AI决策（无论AI是否启用）
     *
     * 用于测试，或实现"AI提示"功能
     */
    void triggerAIMove();

    // ============ Chaos (耄耋) mode ============

    /**
     * @brief 配置混沌模式改牌参数（耄耋模式专用）。
     *
     * 决策（掷骰、选牌）留在会话层，执行走引擎命令，权威 GameState 是唯一真相
     * （消牌不再只改 UI 缓存，避免下次同步"复活"；出牌变形同时覆盖玩家与 AI）。
     * 表现（贴图/震屏/横幅）仍由 UI 层 ChaosEngine 负责。
     *
     * @param active         是否启用改牌
     * @param disappearChance 单张消失概率 (0..1)
     * @param transformChance 出牌变形概率 (0..1)
     * @param disappearOnDraw 发牌时消牌
     * @param transformOnPlay 出牌时变形
     * @param disappearOnIdle 发呆超时消牌（倒计时驱动，掷 disappearChance）
     */
    void setChaosParams(bool active, double disappearChance, double transformChance,
                        bool disappearOnDraw, bool transformOnPlay, bool disappearOnIdle);

    /**
     * @brief 玩家出牌变形入口（耄耋模式）。与 AI 共用同一份 maybeTransformPlay，
     *        基于权威手牌做变形，命中时发 chaosCardsTransformed（驱动 UI 横幅）。
     * @param intended 玩家原本要出的牌
     * @return 变形后的牌（未命中则原样返回）
     *
     * 由 UI（onBtnPlay）在发出牌命令前调用；变形已在此完成，随后的 PlayCards
     * 命令按结果原样执行，executeCommand 不再二次变形（见其注释）。
     */
    Cards transformPlayerPlay(const Cards &intended);

    /**
     * @brief 让某座位随机消失一张手牌（发呆超时用），走引擎权威命令。
     * @param seat 座位号
     * @return 成功消掉返回 true
     *
     * 由 UI 倒计时驱动（仅人类座位有倒计时）。与 onDraw/onPlay 不同，此路径
     * 不掷 chance——调用方（倒计时）已决定要触发。
     */
    bool vanishRandomCard(int seat);

    /**
     * @brief 发呆超时消牌的完整决策+执行（耄耋模式）。检查 active && onIdle，
     *        掷 disappearChance，命中则调 vanishRandomCard 走权威命令消掉一张。
     * @param seat 座位号
     * @return 真的消掉一张返回 true（UI 据此显示"发呆太久"横幅）
     *
     * 把"是否触发"的 RNG 从 UI 收回会话，与 onDraw/onPlay 统一走 rollChaos。
     */
    bool tryIdleVanish(int seat);

signals:
    /**
     * @brief 游戏事件发生（每个GameEngine的事件都会转发）
     * @param event 事件详情
     *
     * UI应监听此信号并更新显示
     */
    void eventEmitted(const GameEvent &event);

    /**
     * @brief 游戏结束
     * @param winnerSeat 获胜者座位号 (0-2)
     */
    void roundFinished(int winnerSeat);

    /**
     * @brief 混沌出牌变形已发生（耄耋模式）。UI 据此显示"变异"横幅。
     * @param seat 发生变形的座位
     * @param from 原本要出的一张牌
     * @param to   变成的牌
     *
     * 实际出的是变形后的牌（已作为普通 CardsPlayed 事件走完引擎）。若变形导致
     * 牌型非法被引擎拒，玩家路径会收到 rejected（重新出牌），AI 走兜底重新决策。
     */
    void chaosCardsTransformed(int seat, const Card &from, const Card &to);

private:
    void processEvents(const QVector<GameEvent> &events);
    void scheduleAIMove();
    void executeAIMove(int seat);
    // Roll per-card disappearance for a seat's freshly dealt hand and, if any
    // hit, issue a ChaosVanish command. Called for every seat right after deal.
    void maybeVanishOnDeal(int seat);
    // If chaos transform is active and rolls, swap one card of `intended` for a
    // random other card from `seat`'s hand. Emits chaosCardsTransformed. Returns
    // the (possibly unchanged) cards to actually play.
    Cards maybeTransformPlay(int seat, const Cards &intended);
    // Single source of truth for chaos probability rolls (deal / play / idle).
    bool rollChaos(double chance) const;

    GameEngine m_engine;                                     ///< 游戏核心引擎
    std::array<PlayerType, kPlayerCount> m_playerTypes;     ///< 每个座位的玩家类型
    bool m_aiEnabled;                                        ///< AI自动执行开关

    // Chaos (耄耋) params — decision (RNG) lives here; execution goes through the
    // engine so the authoritative state stays the single source of truth.
    bool m_chaosActive = false;
    double m_chaosDisappearChance = 0.0;
    double m_chaosTransformChance = 0.0;
    bool m_chaosDisappearOnDraw = false;
    bool m_chaosTransformOnPlay = false;
    bool m_chaosDisappearOnIdle = false;
};

#endif // GAME_CORE_LOCAL_SESSION_H
