cmake_minimum_required(VERSION 3.16)
project(DouDiZhuRelay VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# relay 只用 Qt6 基础功能（QTcpServer/QTimer/QDataStream/QRandomGenerator），
# 无需 6.8 特性。放宽到 6.2 以便在发行版自带 Qt6 的 Linux 服务器上编译
# （如 TencentOS/RHEL 8 的 Qt 6.2/6.6）。Windows 本地仍可用 6.8。
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Network)

set(RELAY_SOURCES
    main.cpp
    room.h room.cpp
    relay_server.h relay_server.cpp
    server_room_game.h server_room_game.cpp
    ../src/networkdata.h
)

# game_core 中仅依赖 Qt Core 的部分。player/strategy 亦仅依赖 QObject+cards（无 UI），
# 掉线 AI 托管(ai_policy)需要它们在服务器端运行同一套 AI 决策。
set(GAME_CORE_SOURCES
    ../src/card.h ../src/card.cpp
    ../src/cards.h ../src/cards.cpp
    ../src/playhand.h ../src/playhand.cpp
    ../src/player.h ../src/player.cpp
    ../src/strategy.h ../src/strategy.cpp
    ../game_core/deck.h ../game_core/deck.cpp
    ../game_core/game_engine.h ../game_core/game_engine.cpp
    ../game_core/game_serialization.h ../game_core/game_serialization.cpp
    ../game_core/ai_policy.h ../game_core/ai_policy.cpp
)

qt_add_executable(DouDiZhuRelay ${RELAY_SOURCES} ${GAME_CORE_SOURCES})

target_include_directories(DouDiZhuRelay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(DouDiZhuRelay PRIVATE Qt6::Core Qt6::Network)

# Console app (not GUI) — do NOT set WIN32_EXECUTABLE
