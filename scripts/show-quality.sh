#!/usr/bin/env sh
set -eu

report_dir="${1:-/workspace/build-quality/quality}"

printf '==> %s/summary.txt <==\n' "${report_dir}"
cat "${report_dir}/summary.txt"
printf '\n==> %s/clang-tidy.txt <==\n' "${report_dir}"
cat "${report_dir}/clang-tidy.txt"
printf '\n==> %s/lizard.txt <==\n' "${report_dir}"
cat "${report_dir}/lizard.txt"
printf '\n==> %s/lizard-warnings.txt <==\n' "${report_dir}"
cat "${report_dir}/lizard-warnings.txt"
