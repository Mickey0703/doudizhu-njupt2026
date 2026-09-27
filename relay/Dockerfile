# 斗地主 relay 服务器 —— 自带 Qt6 的容器，不依赖宿主机(TencentOS)系统源。
# 构建上下文应为 doudizhu-relay/ 目录（含 relay/、game_core/、card.* 等）。
# 用法（在 doudizhu-relay/ 目录内）：
#   docker build -t doudizhu-relay .
#   docker run -d --name relay --restart unless-stopped -p 9527:9527 doudizhu-relay
FROM ubuntu:24.04

# Qt6 基础 + 网络模块 + 编译工具（relay 无界面，只需 Core/Network）
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# relay/CMakeLists.txt 用 ../ 引用父目录源码，故在 relay/ 内构建
RUN cd relay \
    && cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j"$(nproc)"

EXPOSE 9527
CMD ["/src/relay/build/DouDiZhuRelay", "--port", "9527"]
