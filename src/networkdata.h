#ifndef NETWORKDATA_H
#define NETWORKDATA_H

#include <QString>
#include <QDataStream>
#include "card.h"

/**
 * @brief 网络协议定义 —— 消息类型枚举 + 数据结构 + 编码解码工具
 *
 * 协议格式（所有消息统一）：
 *   [1B type] [2B payload length] [N bytes payload]
 *
 * 卡牌编码：encodeCard(Card) → quint8 = point*10 + suit
 *          decodeCard(quint8) → Card(point=code/10, suit=code%10)
 */
namespace NetMsg
{

    /// 消息类型枚举
    enum Type : quint8
    {
        // ============ 连接管理 ============
        Login = 1,    ///< 登录请求（C→S, 附带 name）
        LoginOk,      ///< 登录成功（S→C, 附带 playerId）
        LoginFailed,  ///< 登录失败（S→C, 附带原因）
        JoinRoom,     ///< 加入房间（C→S）
        RoomJoined,   ///< 已加入房间（S→C）
        RoomLeft,     ///< 离开房间通知（S→C）
        PlayerList,   ///< 玩家列表同步（S→C）
        Ready,        ///< 准备就绪（C→S）

        // ============ 游戏流程 ============
        GameStart,        ///< 游戏开始（S→C, 附带 lordId）
        CallLordReq,      ///< 叫地主请求（C→S, 附带 bet）
        CallLordNotify,   ///< 叫地主通知（S→C）
        PlayHandReq,      ///< 出牌请求（C→S, 附带牌编码列表）
        PlayHandNotify,   ///< 出牌通知（S→C）
        PassNotify,       ///< 不出通知（S→C）
        GameOverNotify,   ///< 游戏结束通知（S→C）

        // ============ 聊天 ============
        Chat,          ///< 聊天消息（C→S）
        ChatBroadcast, ///< 聊天广播（S→C）

        // ============ 心跳 ============
        Ping,   ///< 心跳请求（C→S, 每30秒）
        Pong,   ///< 心跳应答（S→C）

        // ============ 贴图 & 系统同步 ============
        Sticker       = 48,   ///< 贴图消息
        System        = 49,   ///< 系统通知（主题/混沌同步）

        // ============ 错误 ============
        Error, ///< 错误消息

        // ============ Relay 房间管理（编号 60-69）============
        CreateRoom    = 60,  ///< C→R: {name}
        RoomCreated   = 61,  ///< R→C: {roomId(quint32)}
        RelayJoinRoom = 62,  ///< C→R: {roomId(quint32), name}
        JoinOk        = 63,  ///< R→C: {playerId(qint32), playerCount(quint8), [players]}
        JoinFailed    = 64,  ///< R→C: {reason}
        PlayerJoined  = 65,  ///< R→C(广播): {playerId(qint32), name}
        PlayerLeft    = 66,  ///< R→C(广播): {playerId(qint32)}
        RoomDissolved = 67,  ///< R→C(广播): {reason}
        LeaveRoom     = 68,  ///< C→R: {}

        // ============ Relay 游戏转发（编号 70-79）============
        GameRelay       = 70,  ///< C→R / R→C: {subType(quint8) + gamePayload}
        StartGameReq    = 71,  ///< C(房主)→R: {}
        StartGameNotify = 72,  ///< R→C(广播): {}

        // ============ Relay 心跳 ============
        Heartbeat    = 90,  ///< C→R: {}
        HeartbeatAck = 91,  ///< R→C: {}
    };

    /// 玩家信息结构体（支持 QDataStream 序列化）
    struct PlayerInfo
    {
        qint32 id;       ///< 玩家 ID
        QString name;    ///< 昵称
        qint32 score;    ///< 累计分数
        qint8 role;      ///< 角色：0=未定, 1=地主(Lord), 2=农民(Farmer)
        qint8 gender;    ///< 性别：0=男(Man), 1=女(Woman)
        qint8 isReady;   ///< 是否准备就绪
        QString avatarId;   ///< 头像 ID（独立于主题，玩家自由选择）

        friend QDataStream &operator<<(QDataStream &s, const PlayerInfo &p)
        {
            return s << p.id << p.name << p.score << p.role << p.gender
                     << p.isReady << p.avatarId;
        }
        friend QDataStream &operator>>(QDataStream &s, PlayerInfo &p)
        {
            return s >> p.id >> p.name >> p.score >> p.role >> p.gender
                     >> p.isReady >> p.avatarId;
        }
    };

    /// 贴图消息载荷
    struct StickerMsg
    {
        qint32 senderId;
        quint8 stickerId;
        QString themeName;

        friend QDataStream &operator<<(QDataStream &s, const StickerMsg &m)
        {
            return s << m.senderId << m.stickerId << m.themeName;
        }
        friend QDataStream &operator>>(QDataStream &s, StickerMsg &m)
        {
            return s >> m.senderId >> m.stickerId >> m.themeName;
        }
    };

    // ============ 卡牌编码/解码 ============

    /// 将 Card 编码为 quint8：code = point × 10 + suit
    /// 例：♠A (point=Card_A=12, suit=Spade=4) → code = 120 + 4 = 124
    inline quint8 encodeCard(const Card &c)
    {
        return static_cast<quint8>(c.getpoint() * 10 + c.getsuit());
    }

    /// 将 quint8 解码为 Card
    /// 越界校验：损坏/恶意包若解出非法枚举，会令渲染层 suitBase[] 越界崩溃。
    /// 大小王花色为 0(Suit_Begin)，普通牌花色 1..4。非法码返回默认 Card。
    inline Card decodeCard(quint8 code)
    {
        int pt = code / 10;    // 点数 = code ÷ 10
        int suit = code % 10;  // 花色 = code mod 10

        const bool joker = (pt == Card::Card_SJ || pt == Card::Card_BJ)
                           && suit == Card::Suit_Begin;
        const bool normal = (pt >= Card::Card_3 && pt <= Card::Card_2)
                            && (suit >= Card::Diamond && suit <= Card::Spade);
        if (!joker && !normal)
            return Card();

        return Card(static_cast<Card::CardPoint>(pt),
                     static_cast<Card::CardSuit>(suit));
    }

    /// GameRelay 内嵌子类型
    namespace GameSub
    {
        enum Type : quint8
        {
            DealCards        = 1,  ///< 发牌：{playerCount(quint8), [playerId(qint32), cardCount(quint8), codes[quint8]...]}
            CallLordNotify   = 2,  ///< 叫地主通知：{playerId(qint32), bet(quint8)}
            CallLordReq      = 3,  ///< 叫地主请求：{playerId(qint32), bet(quint8)}
            PlayHandNotify   = 4,  ///< 出牌通知：{playerId(qint32), count(quint8), codes[quint8]...}
            PlayHandReq      = 5,  ///< 出牌请求：{playerId(qint32), count(quint8), codes[quint8]...}
            PassNotify       = 6,  ///< 不出通知：{playerId(qint32)}
            GameOverNotify   = 7,  ///< 游戏结束：{winnerId(qint32), isLordWin(quint8)}
            LordSelectedNotify = 8, ///< 地主确认：{lordId(qint32), bottomCount(quint8), codes[quint8]...}
            PlayHandRejected = 9,  ///< 出牌被拒：{playerId(qint32), reason}
            ChatBroadcast    = 10, ///< 聊天：{playerId(qint32), msg}
            TurnChangedNotify = 11, ///< 权威轮次：{playerId(qint32), phase(quint8: 1 call, 2 play)}

            // ============ 服务器权威消息族（编号 20+）============
            // 客户端→服务器：意图命令（座位由服务器会话推导，不信任载荷）
            SvrCommand   = 20, ///< C→S: GameSerialization::writeCommand 序列化的 GameCommand
            // 服务器→客户端：权威事件
            SvrEvent     = 21, ///< S→C: {viewerSeat 相关} GameSerialization::writeEvent 序列化的 GameEvent
            SvrRejected  = 22, ///< S→C(仅发送者): {quint8 error}
        };
    }

} // namespace NetMsg

#endif // NETWORKDATA_H
