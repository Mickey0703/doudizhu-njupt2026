# ===================================================================
# 斗地主（DouDiZhu）—— Qt/C++ 卡牌游戏
# ===================================================================
# 这是一个使用 CMake 构建的 Qt6 Widgets 项目，基于 C++17。
#
# 依赖：
#   - Qt6 (或 Qt5) 模块：Widgets, Core, Network, Multimedia
#   - 编译器支持 C++17
#
# 构建：
#   mkdir build && cd build
#   cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt6
#   cmake --build .
#
# 文件结构：
#   card.h/cpp          — 单张扑克牌（花色+点数）
#   cards.h/cpp         — 牌集合（QSet 封装，支持增删查改）
#   player.h/cpp        — 玩家基类（角色、方向、链表、虚函数）
#   robot.h/cpp         — AI 机器人玩家（使用 QTimer 异步决策）
#   userplayer.h/cpp    — 真人用户玩家
#   playhand.h/cpp      — 牌型识别（16种牌型）与大小比较
#   strategy.h/cpp      — AI 出牌策略（贪心算法）
#   game_core/          — 游戏核心：GameEngine/LocalSession/Deck/序列化（纯逻辑，不依赖 UI）
#   gamecontrol_adapter — 桥接 GamePanel ↔ LocalSession（保留旧信号接口）
#   gamepanel.h/cpp/ui  — 主窗口 GUI（卡牌渲染、按钮、动画）
#   cardpanel.h/cpp     — 单张牌的 GUI 控件
#   chatpanel.h/cpp     — 联机聊天面板
#   musicplayer.h/cpp   — 背景音乐/音效播放
#   relay_client + relay/  — Relay 转发联机（客户端 + 独立中转服务器）
#   networkdata.h       — 网络协议定义
#
# Author: DouDiZhu Team
# ===================================================================

cmake_minimum_required(VERSION 3.16)

project(DouDiZhu VERSION 0.1 LANGUAGES CXX)

# ---- Relay 服务器地址（编译期注入）----
# 默认 127.0.0.1，本地开发直接连本机 relay。打发行客户端时用：
#   cmake -DDOUDIZHU_RELAY_HOST=YOUR.RELAY.SERVER.IP ..
# 注入公网 IP。运行时仍可用环境变量 DOUDIZHU_RELAY_HOST 覆盖此默认值。
set(DOUDIZHU_RELAY_HOST "127.0.0.1" CACHE STRING "默认 relay 服务器地址（编译期注入到客户端）")

# ---- Qt 自动化：MOC（元对象编译器）、UIC（UI 编译器）、RCC（资源编译器） ----
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---- C++ 标准 ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 编译告警 & Sanitizer（M-cmake）----
# project_warnings 是一个 INTERFACE 目标，只把告警/sanitizer 选项加到本项目自有目标上，
# 不波及 Qt 等第三方目标。用法：给自有目标 target_link_libraries(<tgt> PRIVATE project_warnings)。
# 打开 sanitizer：cmake -DENABLE_SANITIZERS=ON ..（GCC/Clang，调试用；MSVC 忽略）。
option(ENABLE_SANITIZERS "Build with AddressSanitizer + UBSan (GCC/Clang)" OFF)
add_library(project_warnings INTERFACE)
if(MSVC)
    target_compile_options(project_warnings INTERFACE /W4)
else()
    target_compile_options(project_warnings INTERFACE -Wall -Wextra)
    if(ENABLE_SANITIZERS)
        target_compile_options(project_warnings INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(project_warnings INTERFACE
            -fsanitize=address,undefined)
    endif()
endif()

# ---- 查找 Qt 模块 ----
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Multimedia
)

# ---- 源文件列表 ----
set(PROJECT_SOURCES
        src/gamepanel.cpp
        src/gamepanel_render.cpp
        src/gamepanel_net.cpp
        src/gamepanel_game.cpp
        src/gamepanel_menu.cpp
        src/gamepanel.h
        src/gamepanel.ui
        src/cardpanel.h
        src/cardpanel.cpp
        src/networkdata.h
        src/relay_game_mapping.h
        src/relay_game_mapping.cpp
        src/musicplayer.h
        src/musicplayer.cpp
        src/chatpanel.h
        src/chatpanel.cpp
        src/relay_client.h src/relay_client.cpp
        src/theme_manager.h src/theme_manager.cpp
        src/chaos_engine.h src/chaos_engine.cpp
        src/sticker_store.h src/sticker_store.cpp
        src/sticker_widget.h src/sticker_widget.cpp
        src/avatar_store.h src/avatar_store.cpp
        src/avatar_picker.h src/avatar_picker.cpp
        src/popup_widget.h src/popup_widget.cpp
)

# ---- 资源文件 ----
set(PROJECT_RESOURCES
        resources.qrc
)

add_library(DouDiZhuPresentation STATIC
    ${PROJECT_SOURCES}
    src/robot.h src/robot.cpp
    src/userplayer.h src/userplayer.cpp
    src/gamecontrol_adapter.h src/gamecontrol_adapter.cpp
    game_core/remote_game_session.h game_core/remote_game_session.cpp
    src/card_table_view.h src/card_table_view.cpp
    src/online_room_controller.h src/online_room_controller.cpp
    src/game_action_controller.h src/game_action_controller.cpp
    src/game_record_model.h src/game_record_model.cpp
    src/scoreboard_dialog.h src/scoreboard_dialog.cpp
)
target_include_directories(DouDiZhuPresentation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(DouDiZhuPresentation PUBLIC Qt6::Widgets Qt6::Core Qt6::Network Qt6::Multimedia DouDiZhuGameCore PRIVATE project_warnings)
target_compile_definitions(DouDiZhuPresentation PRIVATE DOUDIZHU_RELAY_HOST="${DOUDIZHU_RELAY_HOST}")

qt_add_executable(DouDiZhu MANUAL_FINALIZATION src/main.cpp ${PROJECT_RESOURCES})

# ---- 链接 Qt 库 ----
target_link_libraries(DouDiZhu PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Network
    Qt6::Multimedia
    DouDiZhuGameCore
    DouDiZhuPresentation
    project_warnings
)

# 把 relay 主机默认值以字符串字面量注入 relay_client.cpp（见该文件顶部的 #ifndef）
target_compile_definitions(DouDiZhu PRIVATE
    DOUDIZHU_RELAY_HOST="${DOUDIZHU_RELAY_HOST}"
)

# ---- 目标属性（平台相关） ----
set_target_properties(DouDiZhu PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE FALSE   # 临时关闭：显示控制台窗口以捕获崩溃信息（调试完后改回TRUE）
)

# ---- 安装规则 ----
include(GNUInstallDirs)
install(TARGETS DouDiZhu
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(DouDiZhu)

# ---- Shared Objects Library (避免重复链接) ----
add_library(DouDiZhu_objects OBJECT
    src/card.h src/card.cpp
    src/cards.h src/cards.cpp
    src/playhand.h src/playhand.cpp
    src/player.h src/player.cpp
    src/strategy.h src/strategy.cpp
)
target_include_directories(DouDiZhu_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DouDiZhu_objects PUBLIC Qt6::Core Qt6::Widgets)
target_link_libraries(DouDiZhu_objects PRIVATE project_warnings)

# ---- Game Core Library ----
# 注意：不要在此 embed $<TARGET_OBJECTS:DouDiZhu_objects>。这些对象已通过下方
# target_link_libraries(... PUBLIC DouDiZhu_objects) 传递进来，恰好一次。两处都写
# 会把同一批对象链接两遍（GNU ld 容忍，但 MSVC/其它链接器下多重定义风险）。
add_library(DouDiZhuGameCore STATIC
    game_core/game_types.h
    game_core/game_state.h
    game_core/game_command.h
    game_core/game_event.h
    game_core/deck.h game_core/deck.cpp
    game_core/game_engine.h game_core/game_engine.cpp
    game_core/game_serialization.h game_core/game_serialization.cpp
    game_core/ai_policy.h game_core/ai_policy.cpp
    game_core/state_strategy.h game_core/state_strategy.cpp
    game_core/local_session.h game_core/local_session.cpp
)
target_include_directories(DouDiZhuGameCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DouDiZhuGameCore PUBLIC Qt6::Core DouDiZhu_objects)
target_link_libraries(DouDiZhuGameCore PRIVATE project_warnings)

# ===================================================================
# 引擎层端到端回归测试（阶段 5 / C2 恢复）
# ===================================================================
# 纯 Qt Core（+ 网络测试用 Qt Network），不依赖 UI/Widgets/Multimedia。
# 复用 DouDiZhuGameCore 目标，避免重复编译核心源码。
# 用法：cmake -DBUILD_TESTING=ON .. && cmake --build . && ctest
# 关闭：默认关（BUILD_TESTING 由 CTest 模块引入，默认 ON；如需彻底跳过测试构建可 -DBUILD_TESTING=OFF）
include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    # ---- 纯引擎层：只链 DouDiZhuGameCore（含 card/cards/playhand/player/strategy）----
    foreach(pair
        "GameCoreDeckTests:game_core_deck_tests.cpp:game_core_deck"
        "PlayHandTests:playhand_tests.cpp:playhand"
        "GameCoreEngineTests:game_core_engine_tests.cpp:game_core_engine"
        "GameSerializationTests:game_serialization_tests.cpp:game_serialization"
        "LocalSessionTests:local_session_tests.cpp:local_session"
    )
        string(REPLACE ":" ";" parts "${pair}")
        list(GET parts 0 tgt)
        list(GET parts 1 src)
        list(GET parts 2 testname)
        add_executable(${tgt} tests/${src})
        target_include_directories(${tgt} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )
        target_link_libraries(${tgt} PRIVATE DouDiZhuGameCore)
        add_test(NAME ${testname} COMMAND ${tgt})
    endforeach()

    # ---- 服务器权威端到端：编译 relay + 权威核心，用真实 TCP socket ----
    add_executable(ServerAuthoritativeTests
        tests/server_authoritative_tests.cpp
        relay/room.h relay/room.cpp
        relay/relay_server.h relay/relay_server.cpp
        relay/server_room_game.h relay/server_room_game.cpp
    )
    target_include_directories(ServerAuthoritativeTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/relay
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    target_link_libraries(ServerAuthoritativeTests PRIVATE
        Qt6::Core Qt6::Network DouDiZhuGameCore)
    add_test(NAME server_authoritative COMMAND ServerAuthoritativeTests)

    # ---- 客户端远程会话视图（无 socket，喂序列化事件）----
    add_executable(RemoteSessionTests
        tests/remote_session_tests.cpp
        game_core/remote_game_session.h game_core/remote_game_session.cpp
        src/relay_client.h src/relay_client.cpp
    )
    target_include_directories(RemoteSessionTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    target_link_libraries(RemoteSessionTests PRIVATE
        Qt6::Core Qt6::Network DouDiZhuGameCore)
    add_test(NAME remote_session COMMAND RemoteSessionTests)

    # Pure-state AI: deliberately does not link Player, Strategy or Qt Widgets.
    add_executable(StateStrategyChecks tools/state_strategy_checks.cpp
        src/card.cpp src/cards.cpp src/playhand.cpp game_core/deck.cpp
        game_core/game_engine.cpp game_core/ai_policy.cpp game_core/state_strategy.cpp)
    target_include_directories(StateStrategyChecks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/game_core)
    target_link_libraries(StateStrategyChecks PRIVATE Qt6::Core project_warnings)
    add_test(NAME state_strategy COMMAND StateStrategyChecks)

    add_executable(OnlineRoomControllerChecks tools/online_room_controller_checks.cpp
        src/online_room_controller.h src/online_room_controller.cpp
        src/relay_client.h src/relay_client.cpp
        game_core/remote_game_session.h game_core/remote_game_session.cpp
        relay/room.h relay/room.cpp relay/relay_server.h relay/relay_server.cpp
        relay/server_room_game.h relay/server_room_game.cpp)
    target_include_directories(OnlineRoomControllerChecks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(OnlineRoomControllerChecks PRIVATE Qt6::Core Qt6::Network DouDiZhuGameCore project_warnings)
    add_test(NAME online_room_controller COMMAND OnlineRoomControllerChecks)

    add_executable(OptionalUiChecks tools/optional_ui_checks.cpp ${PROJECT_RESOURCES})
    target_link_libraries(OptionalUiChecks PRIVATE DouDiZhuPresentation Qt6::Test project_warnings)
    add_test(NAME optional_ui COMMAND OptionalUiChecks)
    set_tests_properties(optional_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
