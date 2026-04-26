FROM ubuntu:24.04 AS toolchain

ENV DEBIAN_FRONTEND=noninteractive

# python3 + python3-jsonschema satisfy the AP M5-1.2 Tranche A validator gate
# (tests/validate_json_schema.py). graphviz satisfies the AP M5-1.3 Tranche
# A DOT syntax gate (`dot -Tsvg`); the Python fallback in
# tests/validate_dot_reports.py uses only stdlib and runs alongside the
# Graphviz path. The test and coverage stages run ctest, which invokes both
# validators; the toolchain layer keeps the dependencies centrally installed
# for every derived stage that runs ctest. The runtime stage uses a separate
# base image and is not affected.
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        git \
        graphviz \
        libcap-dev \
        python3 \
        python3-jsonschema \
        time \
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

FROM toolchain AS quality-base

ARG XRAY_CLANG_TIDY_MAX_FINDINGS=0
ARG XRAY_LIZARD_MAX_CCN=10
ARG XRAY_LIZARD_MAX_LENGTH=50
ARG XRAY_LIZARD_MAX_PARAMETERS=5

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        clang \
        clang-tidy \
        python3 \
        python3-pip \
    && python3 -m pip install --no-cache-dir --break-system-packages lizard==1.22.0 \
    && rm -rf /var/lib/apt/lists/*

COPY . .

RUN chmod +x /workspace/scripts/run-quality-report.sh /workspace/scripts/show-quality.sh \
    && XRAY_CLANG_TIDY_MAX_FINDINGS="${XRAY_CLANG_TIDY_MAX_FINDINGS}" \
       XRAY_LIZARD_MAX_CCN="${XRAY_LIZARD_MAX_CCN}" \
       XRAY_LIZARD_MAX_LENGTH="${XRAY_LIZARD_MAX_LENGTH}" \
       XRAY_LIZARD_MAX_PARAMETERS="${XRAY_LIZARD_MAX_PARAMETERS}" \
       /workspace/scripts/run-quality-report.sh /workspace

FROM quality-base AS quality

ENTRYPOINT ["/workspace/scripts/show-quality.sh"]

FROM quality-base AS quality-check

ARG XRAY_CLANG_TIDY_MAX_FINDINGS=0

RUN clang_tidy_findings="$(awk -F= '$1 == "clang_tidy_findings" { print $2 }' /workspace/build-quality/quality/summary.txt)" \
    && lizard_warning_count="$(awk -F= '$1 == "lizard_warning_count" { print $2 }' /workspace/build-quality/quality/summary.txt)" \
    && test -n "$clang_tidy_findings" \
    && test -n "$lizard_warning_count" \
    && echo "clang-tidy findings: ${clang_tidy_findings} (threshold: ${XRAY_CLANG_TIDY_MAX_FINDINGS})" >&2 \
    && echo "lizard warnings: ${lizard_warning_count} (threshold: 0)" >&2 \
    && if [ "$clang_tidy_findings" -gt "${XRAY_CLANG_TIDY_MAX_FINDINGS}" ]; then \
        echo "error: clang-tidy findings exceed threshold: ${clang_tidy_findings} > ${XRAY_CLANG_TIDY_MAX_FINDINGS}" >&2; \
        exit 1; \
    fi \
    && if [ "$lizard_warning_count" -gt 0 ]; then \
        echo "error: lizard warnings exceed threshold: ${lizard_warning_count} > 0" >&2; \
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
