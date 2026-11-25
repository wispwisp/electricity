#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

cd "$(dirname "$0")/.."

echo "== 1/6 build"
cmake --preset dev
cmake --build build

echo "== 2/6 tests"
ctest --test-dir build --output-on-failure

echo "== 3/6 clang-format"
git ls-files -co --exclude-standard '*.cpp' '*.h' | xargs clang-format --dry-run --Werror

echo "== 4/6 clang-tidy"
# GitHub Actions sets CI=true, and there every file is checked. Locally only
# the files a push would bring to CI are: changed since origin/main, plus the
# .cpp beside each changed header.
if [ "${CI:-}" != true ] && git rev-parse --verify --quiet origin/main >/dev/null; then
    changed=$( (git diff --name-only --diff-filter=d "$(git merge-base HEAD origin/main)"
                git ls-files --others --exclude-standard) | sort -u)
    for header in $(echo "$changed" | grep -E '\.h$' || true); do
        if [ -f "${header%.h}.cpp" ]; then
            changed="$changed"$'\n'"${header%.h}.cpp"
        fi
    done
    changed=$(echo "$changed" | sort -u)
else
    changed=$(git ls-files)
fi
files=$(echo "$changed" | grep -E '^(src|tests)/.*\.cpp$' || true)
if [ -z "$files" ]; then
    echo "no changed .cpp files, skipping"
else
    echo "$files"
    echo "$files" | xargs clang-tidy -p build --quiet
fi

echo "== 5/6 cppcheck"
cppcheck --enable=warning,performance,portability --error-exitcode=1 \
    --library=qt --inline-suppr --quiet src/

echo "== 6/6 asan tests"
cmake --preset asan
cmake --build build-asan
ctest --test-dir build-asan -L core --output-on-failure

echo "All quality gates passed"
