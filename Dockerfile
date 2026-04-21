FROM ubuntu:24.04 AS toolchain

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

FROM toolchain AS build

COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

FROM build AS test

RUN cd build && ctest --output-on-failure

FROM ubuntu:24.04 AS runtime

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        libstdc++6 \
    && useradd \
        --system \
        --create-home \
        --home-dir /app \
        --shell /usr/sbin/nologin \
        cmake-xray \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build --chown=cmake-xray:cmake-xray /workspace/build/cmake-xray /app/cmake-xray

USER cmake-xray

ENTRYPOINT ["/app/cmake-xray"]
