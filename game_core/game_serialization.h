#ifndef GAME_CORE_GAME_SERIALIZATION_H
#define GAME_CORE_GAME_SERIALIZATION_H

#include "game_command.h"
#include "game_event.h"
#include "game_state.h"

#include <QDataStream>

/**
 * @brief 游戏对象的网络序列化 —— 客户端与服务器共享
 *
 * 所有函数使用调用方设置好版本的 QDataStream。
 * 卡牌编码复用 point*10+suit 方案（与 NetMsg::encodeCard 一致），
 * 但此处不依赖 networkdata.h，保持 game_core 对 Qt Core 的纯依赖。
 *
 * 线路格式（均为大端 QDataStream Qt_6_0）：
 *   Cards:   quint8 count, 后跟 count 个 quint8 卡牌码
 *   Command: quint8 type, qint32 seat, qint32 bid, Cards cards
 *   Event:   quint8 type, qint32 seat, qint32 value, Cards cards
 */
namespace GameSerialization
{

// ---- Cards ----
void writeCards(QDataStream &out, const Cards &cards);
Cards readCards(QDataStream &in);

// ---- GameCommand ----
void writeCommand(QDataStream &out, const GameCommand &command);
GameCommand readCommand(QDataStream &in);

// ---- GameEvent ----
void writeEvent(QDataStream &out, const GameEvent &event);
GameEvent readEvent(QDataStream &in);

} // namespace GameSerialization

#endif // GAME_CORE_GAME_SERIALIZATION_H
