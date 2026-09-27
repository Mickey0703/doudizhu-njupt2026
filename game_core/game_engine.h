#ifndef GAME_CORE_GAME_ENGINE_H
#define GAME_CORE_GAME_ENGINE_H

#include "game_command.h"
#include "game_event.h"
#include "game_state.h"

/**
 * @class GameEngine
 * @brief 斗地主游戏核心引擎 - 纯函数式、确定性的游戏逻辑
 *
 * ## 设计原则
 *
 * 1. **不可变状态**: GameState是只读的，每次命令执行产生新状态
 * 2. **确定性**: 给定相同的初始状态和命令序列，结果总是相同
 * 3. **事件驱动**: 所有状态变化通过GameEvent表达，支持回放和日志
 * 4. **无副作用**: 不依赖外部状态，不产生I/O，纯计算逻辑
 *
 * ## 游戏流程
 *
 * ```
 * startRound() → CallingLord阶段
 *   ↓ (3轮叫地主)
 * selectLord() → Playing阶段
 *   ↓ (出牌直到某人手牌为空)
 * finishRound() → RoundFinished阶段
 * ```
 *
 * ## 使用示例
 *
 * ```cpp
 * GameEngine engine;
 *
 * // 开始游戏
 * CardList deck = Deck::shuffled(12345);
 * CommandResult result = engine.startRound(deck, 0);
 *
 * // 执行命令
 * result = engine.execute(GameCommand::callLord(0, 3));
 * for (const GameEvent &event : result.events) {
 *     // 处理事件：更新UI、播放音效等
 * }
 *
 * // 查询状态
 * if (engine.state().phase == GamePhase::Playing) {
 *     int currentSeat = engine.state().currentSeat;
 *     Cards hand = engine.state().players[currentSeat].hand;
 * }
 *
 * // 获取投影状态（隐藏其他玩家手牌）
 * GameState projected = engine.projectedStateFor(0);
 * ```
 *
 * ## 线程安全
 *
 * GameEngine不是线程安全的。多线程环境下需要外部同步。
 * 推荐在单一线程（通常是主线程）中使用。
 *
 * @see GameState, GameCommand, GameEvent
 */
class GameEngine
{
public:
    /**
     * @brief 获取当前游戏状态（只读）
     * @return 当前完整的GameState引用
     *
     * 注意：返回的引用在下次execute()调用后可能失效
     */
    const GameState &state() const;

    /**
     * @brief 开始新一轮游戏
     * @param deck 洗好的54张牌（必须包含完整的一副牌）
     * @param firstSeat 第一个叫地主的玩家座位号 (0-2)
     * @return 命令结果，包含发牌事件
     *
     * 前置条件：
     * - deck.size() == 54
     * - deck包含标准斗地主牌型（52张普通牌+2张王）
     * - 0 <= firstSeat < 3
     *
     * 后置条件：
     * - phase == CallingLord
     * - 每位玩家17张手牌
     * - 3张底牌待定
     * - currentSeat == firstSeat
     */
    CommandResult startRound(const CardList &deck, int firstSeat);

    /**
     * @brief 执行一个游戏命令
     * @param command 要执行的命令（叫地主/出牌/过）
     * @return 命令结果
     *
     * 如果命令合法：
     * - result.accepted == true
     * - result.events包含状态变化事件
     * - 内部状态已更新
     *
     * 如果命令非法：
     * - result.accepted == false
     * - result.error说明原因
     * - 内部状态不变
     *
     * 常见错误：
     * - NotYourTurn: 不是该玩家的回合
     * - InvalidCards: 牌型不合法或不在手中
     * - CannotBeat: 无法打过上家
     * - InvalidBid: 叫分不合法
     */
    CommandResult execute(const GameCommand &command);

    /**
     * @brief 验证当前状态的一致性（调试用）
     * @return 错误码，NoError表示状态正常
     *
     * 检查项：
     * - 手牌数量合法
     * - 不存在重复的牌
     * - 阶段与状态一致
     * - 座位号有效
     *
     * 仅用于单元测试和调试，生产代码中不应失败
     */
    GameError validateState() const;

    /**
     * @brief 获取特定座位的投影状态（隐藏信息）
     * @param seat 座位号 (0-2)
     * @return 投影后的GameState
     *
     * 投影规则：
     * - 保留该座位的完整手牌
     * - 其他座位只保留手牌数量，清空具体牌面
     * - 底牌在CallingLord阶段隐藏
     * - 其他信息（pending、lord、phase等）完全保留
     *
     * 用于网络游戏，防止作弊
     */
    GameState projectedStateFor(int seat) const;

private:
    friend class GameEngineTestAccess;

    // 命令执行器
    CommandResult executeCallLord(const GameCommand &command);
    CommandResult executePlayCards(const GameCommand &command);
    CommandResult executePass(const GameCommand &command);
    CommandResult executeChaosVanish(const GameCommand &command);

    // 游戏流程控制
    void selectLord(int seat, int bid, QVector<GameEvent> *events);
    void finishRound(int winnerSeat, QVector<GameEvent> *events);

    GameState m_state;  ///< 当前游戏状态
};

#endif // GAME_CORE_GAME_ENGINE_H
