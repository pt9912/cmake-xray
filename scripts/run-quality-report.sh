#!/usr/bin/env sh
set -eu

workspace="${1:-/workspace}"
build_dir="${workspace}/build-quality"
report_dir="${build_dir}/quality"

: "${XRAY_CLANG_TIDY_MAX_FINDINGS:=0}"
: "${XRAY_LIZARD_MAX_CCN:=10}"
: "${XRAY_LIZARD_MAX_LENGTH:=50}"
: "${XRAY_LIZARD_MAX_PARAMETERS:=5}"

mkdir -p "${report_dir}"

cmake -S "${workspace}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

find "${workspace}/src" -type f -name '*.cpp' | sort > "${report_dir}/clang-tidy-inputs.txt"

if [ -s "${report_dir}/clang-tidy-inputs.txt" ]; then
    # Parallelise clang-tidy across every src/ .cpp file: one invocation per
    # file, up to nproc in flight. The summary-counting grep below relies on
    # `:line:col: (warning|error):` which is robust to xargs' inter-process
    # output interleaving, so we do not need GNU parallel or run-clang-tidy
    # for stable counting.
    parallel_jobs="$(nproc 2>/dev/null || echo 1)"
    xargs -r -P "${parallel_jobs}" -n 1 -a "${report_dir}/clang-tidy-inputs.txt" \
        clang-tidy \
        -p "${build_dir}" \
        --config-file="${workspace}/.clang-tidy" \
        --quiet \
        > "${report_dir}/clang-tidy.raw.txt" 2>&1 || true
    sed '/ warnings generated\.$/d' "${report_dir}/clang-tidy.raw.txt" \
        > "${report_dir}/clang-tidy.txt"
    rm -f "${report_dir}/clang-tidy.raw.txt"
else
    : > "${report_dir}/clang-tidy.txt"
fi

clang_tidy_findings="$(grep -Ec ':[0-9]+:[0-9]+: (warning|error):' "${report_dir}/clang-tidy.txt" || true)"

lizard \
    -l cpp \
    -C 999 \
    -L 9999 \
    -a 99 \
    "${workspace}/src" \
    > "${report_dir}/lizard.txt"
lizard \
    -l cpp \
    -C "${XRAY_LIZARD_MAX_CCN}" \
    -L "${XRAY_LIZARD_MAX_LENGTH}" \
    -a "${XRAY_LIZARD_MAX_PARAMETERS}" \
    -w \
    "${workspace}/src" \
    > "${report_dir}/lizard-warnings.txt" 2>&1 || true

lizard_warning_count="$(grep -c 'warning:' "${report_dir}/lizard-warnings.txt" || true)"

cat > "${report_dir}/summary.txt" <<EOF
clang_tidy_findings=${clang_tidy_findings}
clang_tidy_max_findings=${XRAY_CLANG_TIDY_MAX_FINDINGS}
lizard_warning_count=${lizard_warning_count}
lizard_max_ccn=${XRAY_LIZARD_MAX_CCN}
lizard_max_length=${XRAY_LIZARD_MAX_LENGTH}
lizard_max_parameters=${XRAY_LIZARD_MAX_PARAMETERS}
EOF
