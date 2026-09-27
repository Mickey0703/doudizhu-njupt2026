# 斗地主游戏引擎层招新题 Starter Kit

本工程用于 **2026 C++ 组招新免试题**。完整规则、函数清单、评分标准与提交方式以题目 PDF / 招新文档为准。

本压缩包保留了完整程序运行所需的 GUI、头像、牌面、主题、音频、网络等配套文件；**必做题仍然只要求实现游戏引擎层指定的 `TODO(candidate)`**。除题目明确允许的选做内容外，不要修改其他评分依赖文件。

## 环境

- C++17
- CMake
- Qt 6.8 或兼容版本
- Qt 模块：Core、Widgets、Network、Multimedia

## 必做任务

只实现以下 5 个文件中带有 `// TODO(candidate)` 标记的函数：

1. `src/playhand.cpp`：牌型识别与大小比较。
2. `game_core/deck.cpp`：标准牌堆、确定性洗牌、牌堆合法性校验。
3. `game_core/game_engine.cpp`：叫地主、出牌、过牌、结算与状态投影。
4. `game_core/local_session.cpp`：单机会话编排、事件转发与 AI 调度。
5. `game_core/game_serialization.cpp`：命令、事件与牌集合的网络序列化。

## 必做题中不要改动

除题目明确允许的 `TODO(candidate)` 外，请保持评分依赖部分原样，尤其包括：

- 全部 `.h` 文件；
- `src/card.*`、`src/cards.*`；
- AI 相关代码：`ai_policy.*`、`strategy.*`、`player.*` 及其配套代码；
- `tests/` 下全部测试；
- UI / 网络 / 音频等其他模块；
- 已给出的 chaos 相关函数，以及 `execute()` 派发器。

核心设计约束包括：命令 → 事件模型、`GameEngine` 维护唯一权威状态、非法命令无副作用、引擎核心确定性、无 UI / 网络 / I/O 依赖，以及按座位进行隐藏信息投影。具体以题目文档为准。

> 若进行题目明确列出的选做项（例如 GamePanel 解耦、AI 重写等），可按选做项要求修改对应文件。

## 完整工程内容

除引擎层和测试外，压缩包同时包含完整运行所需的配套内容，包括：

- `src/gamepanel.ui` 与 GamePanel / CardPanel 等 GUI 源码；
- `avatars/` 人物头像；
- `images/` 扑克牌、背景和界面图片；
- `themes/` 主题、卡背与贴图；
- `resources/` 音频与资源文件；
- `resources.qrc` Qt 资源清单；
- 网络会话及 Relay 相关配套代码。

这些文件主要用于保证工程能够保持完整界面与运行环境，并不改变必做题的修改范围。

## 构建

```sh
cmake -B build
cmake --build build
```

若 CMake 找不到 Qt，可显式指定 Qt 安装目录：

```sh
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/compiler_kit
cmake --build build
```

主程序目标为 `DouDiZhu`。

## 测试

补全必做实现后运行：

```sh
ctest --test-dir build --output-on-failure
```

建议先关注以下引擎层测试：

```text
playhand_tests
→ game_core_deck_tests
→ game_core_engine_tests
→ local_session_tests
```

`game_serialization_tests` 相对独立，可以穿插完成。

## 选做与自由发挥

题目另设 AI 策略重写、GamePanel 解耦、计分板 / 新玩法 / 对局回放、设计说明文档等加分项。选择相关加分项时，以题目文档中的边界和验收要求为准。

## 提交

完成后将整个工程推送到自己的 GitHub 公开仓库，提交仓库链接，并保留完整提交历史。
