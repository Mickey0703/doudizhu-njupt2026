#ifndef GAME_CORE_REMOTE_GAME_SESSION_H
#define GAME_CORE_REMOTE_GAME_SESSION_H

#include "game_command.h"
#include "game_event.h"
#include "game_state.h"

#include <QObject>

class RelayClient;

/**
 * @brief 客户端远程游戏会话 —— 服务器权威模式下的纯视图端
 *
 * 与 LocalSession 对称，但**不含 GameEngine**：所有规则由服务器裁决。
 * 职责：
 *   - 把本地玩家意图（叫地主/出牌/过）序列化为 SvrCommand 发给服务器。
 *   - 接收服务器 SvrEvent，更新本地只读视图 GameState，并发 Qt 信号供 UI 刷新。
 *   - 接收 SvrRejected，发 commandRejected 信号（UI 提示）。
 *
 * 本地视图 GameState：
 *   - 只有本座位手牌是完整的（来自 PrivateHandDealt）。
 *   - 其他座位只有手牌数量（remainingCards 语义），具体牌面为空。
 *   - 公开信息（当前座位、pending、地主、倍数、分数）随事件更新。
 *
 * 客户端**不做**规则验证——发命令后等服务器确认的事件才更新权威部分。
 * （本地可做乐观 UI，但视图状态以服务器事件为准。）
 */
class RemoteGameSession : public QObject
{
    Q_OBJECT

public:
    explicit RemoteGameSession(RelayClient *client, QObject *parent = nullptr);

    /// 设置本地玩家座位（0/1/2），由 relay playerId-1 得到
    void setLocalSeat(int seat);
    int localSeat() const { return m_localSeat; }

    /// 发送命令到服务器（座位会被服务器按会话推导，这里仅填本地 seat 供本地参考）
    void sendCommand(const GameCommand &command);

    /// 便捷方法
    void callLord(int bid);
    void playCards(const Cards &cards);
    void pass();

    /// 只读本地视图
    const GameState &viewState() const { return m_view; }

    /// 处理来自 RelayClient 的游戏消息（subType + payload）
    void handleServerMessage(quint8 subType, const QByteArray &payload);

signals:
    /// 每个服务器事件应用后发出，供 UI 刷新
    void eventApplied(const GameEvent &event);
    /// 命令被服务器拒绝
    void commandRejected(GameError error);
    /// 一局结束
    void roundFinished(int winnerSeat);

private:
    void applyEvent(const GameEvent &event);

    RelayClient *m_client;
    GameState m_view;
    int m_localSeat = kInvalidSeat;
};

#endif // GAME_CORE_REMOTE_GAME_SESSION_H
