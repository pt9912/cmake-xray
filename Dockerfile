FROM ubuntu:24.04 AS toolchain

ENV DEBIAN_FRONTEND=noninteractive

# Single consolidated apt-install for every CI tool the derived stages need:
#   - build-essential / cmake: build all three pipelines (test, coverage,
#     quality) on the same toolchain layer.
#   - graphviz / python3-jsonschema: AP M5-1.2 / 1.3 / 1.4 validator gates
#     (validate_json_schema.py, validate_dot_reports.py via dot -Tsvg/-Tplain
#     /-Tjson, validate_html_reports.py).
#   - gcovr: AP M5 coverage gate. Previously installed only in the coverage
#     stage; consolidating here lets every derived stage hit the same cached
#     toolchain layer.
#   - clang / clang-tidy / python3-pip + lizard: AP M5 quality gate.
#     Previously installed in quality-base; same caching argument applies.
# pip-installed lizard pin matches the previous quality-base contract; the
# --break-system-packages flag is required on Ubuntu 24.04 because system
# Python is marked PEP 668 externally-managed.
# The runtime stage uses a separate base image and is not affected by any
# of these tools.
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        clang-tidy \
        cmake \
        gcovr \
        git \
        graphviz \
        libcap-dev \
        python3 \
        python3-jsonschema \
        python3-pip \
        time \
    && python3 -m pip install --no-cache-dir --break-system-packages lizard==1.22.0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

FROM toolchain AS build

COPY . .

# Parallel cmake build keeps the build stage from serialising compilation;
# previously each stage spent ~2 min on sequential cc1plus invocations.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel

FROM build AS test

# CTest runs the doctest suite plus the validator gates; -j parallelises the
# entries so e2e_binary (~3 s) overlaps with the Python validators.
RUN cd build && ctest --output-on-failure --parallel

FROM toolchain AS coverage

COPY . .

RUN cmake -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DXRAY_ENABLE_COVERAGE=ON \
    && cmake --build build-coverage --parallel \
    && ctest --test-dir build-coverage --output-on-failure --parallel \
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

FROM toolchain AS release-archive

COPY . .

# AP M5-1.6 Tranche B: build the reproducible Linux release archive in a
# controlled container so the host environment cannot leak into the
# artifact. The stage produces the archive and its SHA-256 sidecar in
# /workspace/release-out and exposes them via an entrypoint that copies
# them into a host-mounted output directory:
#   docker build --target release-archive --build-arg XRAY_APP_VERSION=1.2.3 -t cmake-xray:release-archive .
#   mkdir -p ./release-assets
#   docker run --rm -v "$PWD/release-assets:/output" cmake-xray:release-archive
# Alternatively, callers can use docker create / docker cp directly if
# they prefer not to mount /output.
# XRAY_APP_VERSION is the published app version without leading 'v' (the
# canonical source is scripts/validate-release-tag.sh against the release
# tag). SOURCE_DATE_EPOCH defaults to 1 so the archive stays byte-stable
# without a real Tag-Commit-Zeitstempel; CI overrides it to
# `git log -1 --pretty=%ct $TAG` per plan-M5-1-6.md Entscheidungen.
ARG XRAY_APP_VERSION
ARG SOURCE_DATE_EPOCH=1
RUN if [ -z "$XRAY_APP_VERSION" ]; then \
        echo "error: --build-arg XRAY_APP_VERSION is required for release-archive stage" >&2; \
        echo "hint: pass the canonical app version without a leading 'v', e.g. 1.2.3 or 1.2.3-rc.1" >&2; \
        exit 2; \
    fi \
    && SOURCE_DATE_EPOCH="$SOURCE_DATE_EPOCH" \
       bash scripts/build-release-archive.sh "$XRAY_APP_VERSION" /workspace/release-out

ENTRYPOINT ["bash", "/workspace/scripts/release-archive-entrypoint.sh"]

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
