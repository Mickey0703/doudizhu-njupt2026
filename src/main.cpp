/**
 * @file main.cpp
 * @brief 斗地主游戏入口
 *
 * 斗地主（DouDiZhu）—— Qt/C++ 实现的本地+联机斗地主卡牌游戏
 *
 * 核心模块：
 * - Card / Cards：扑克牌数据层
 * - PlayHand：牌型识别与比较
 * - Strategy：AI 贪心策略（首出、跟牌、叫地主）
 * - Player / Robot / UserPlayer：玩家抽象与多态
 * - GameEngine / LocalSession：游戏流程核心（发牌、叫地主、出牌轮转、计分）
 * - GameControlAdapter：桥接 GamePanel 与 LocalSession
 * - GamePanel：主窗口 GUI（卡牌渲染、动画、布局）
 * - MusicPlayer：背景音乐/音效播放
 * - RelayClient / RelayServer：Relay 转发联机对战
 * - ChatPanel：联机聊天界面
 *
 * 启动后进入主菜单，可选择：
 * - 人机对战：本地与两个 AI 机器人对战
 * - 创建房间：作为服务器等待其他玩家连接
 * - 加入房间：作为客户端连接到服务器
 */

#include "gamepanel.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    GamePanel w;
    w.show();
    return a.exec();
}
