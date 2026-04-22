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

FROM toolchain AS coverage

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        gcovr \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN cmake -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DXRAY_ENABLE_COVERAGE=ON \
    && cmake --build build-coverage \
    && ctest --test-dir build-coverage --output-on-failure \
    && mkdir -p build-coverage/coverage \
    && gcovr \
        --root /workspace \
        --filter /workspace/src \
        --object-directory /workspace/build-coverage \
        --exclude /workspace/build-coverage/_deps \
        > /workspace/build-coverage/coverage/coverage.txt \
    && grep '^TOTAL' /workspace/build-coverage/coverage/coverage.txt \
        > /workspace/build-coverage/coverage/summary.txt

RUN printf '%s\n' \
    '#!/usr/bin/env sh' \
    'set -eu' \
    'printf "==> /workspace/build-coverage/coverage/summary.txt <==\n"' \
    'cat /workspace/build-coverage/coverage/summary.txt' \
    'printf "\n==> /workspace/build-coverage/coverage/coverage.txt <==\n"' \
    'cat /workspace/build-coverage/coverage/coverage.txt' \
    > /usr/local/bin/show-coverage \
    && chmod +x /usr/local/bin/show-coverage

ENTRYPOINT ["/usr/local/bin/show-coverage"]

FROM coverage AS coverage-check

ARG XRAY_COVERAGE_THRESHOLD=100

RUN coverage_percent="$(awk '$1 == "TOTAL" { gsub("%", "", $4); print $4 }' /workspace/build-coverage/coverage/summary.txt)" \
    && test -n "$coverage_percent" \
    && echo "coverage: ${coverage_percent}% (threshold: ${XRAY_COVERAGE_THRESHOLD}%)" >&2 \
    && if [ "$coverage_percent" -lt "$XRAY_COVERAGE_THRESHOLD" ]; then \
        echo "error: coverage threshold not met: ${coverage_percent}% < ${XRAY_COVERAGE_THRESHOLD}%" >&2; \
        exit 1; \
    fi

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
