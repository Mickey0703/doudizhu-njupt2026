# 选做实现计划

目标：保留必做引擎契约和所有原测试，完成纯 GameState AI 与 GamePanel 职责解耦，添加计分/事件记录和设计说明。

- [x] AI：`StateStrategy::decideCommand(const GameState &, int)`，枚举合法候选、只看自己的手牌及公开信息；`AIPolicy` 只读取状态并通过命令执行。新增纯 Core 测试验证独立于旧 Player/Strategy。
- [x] 卡牌视图：`CardTableView` 独占牌控件、选牌索引和布局/动画，不接触引擎或网络；GamePanel 只传具体手牌和余牌数。
- [x] 联机会话：`OnlineRoomController` 独占客户端、远程视图和房间状态；主窗口只消费事件、显示房间/聊天信息。
- [x] 用户回合：`GameActionController` 统一命令路由、纯状态提示、合法超时行动和计时器；UI 不再依赖按钮显隐推断阶段。
- [x] 计分与记录：只观察本地/远程事件，显示累计分、倍数与对局记录，不成为第二个游戏状态源。
- [x] 集成：原七组测试不变；新增 AI、真实 TCP 房间和 Qt Widgets 离屏测试；审阅后全量构建与回归。

重点边界：不同长度飞机/顺子；农民队友让牌；提示的投影信息；首出超时不能过；切换菜单/房间后旧计时器与旧网络信号不能推进新对局；联机等待确认不乐观删除手牌；动画控件销毁；计分历史不可改变引擎。

验收：Debug 全量构建成功；10/10 CTest 测试组通过，原 84 个用例保持不变；新增 AI 1000 seed、真实 TCP 与离屏 UI 检查。只读审阅指出的最高叫分投影、动画缩放终点问题均已复现失败并修复转绿。最终日志：build/optional-final-tests.log；修改范围：build/optional-scope-audit.json。
