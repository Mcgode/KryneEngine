#!/usr/bin/env bash
# Runs the unit tests and builds an LLVM source-based coverage report.
# macOS and Linux only (Clang/AppleClang, non-cross-compiled) — see RunCoverage.ps1 for Windows.
# Requires configuring CMake with -DKRYNE_ENGINE_ENABLE_COVERAGE=ON and building the Tests target.
set -euo pipefail

BUILD_DIR="${1:?Usage: RunCoverage.sh <build-dir> <cxx-compiler>}"
CXX_COMPILER="${2:?Usage: RunCoverage.sh <build-dir> <cxx-compiler>}"
COVERAGE_DIR="${BUILD_DIR}/coverage"
RAW_DIR="${COVERAGE_DIR}/raw"

# Resolve llvm-cov/llvm-profdata through the compiler driver rather than searching PATH: this
# guarantees the tool matches the Clang version that produced the instrumentation, which the
# .profraw/.profdata formats are sensitive to.
LLVM_PROFDATA=("$("${CXX_COMPILER}" -print-prog-name=llvm-profdata)")
LLVM_COV=("$("${CXX_COMPILER}" -print-prog-name=llvm-cov)")

rm -rf "${COVERAGE_DIR}"
mkdir -p "${RAW_DIR}"

echo "Running tests with coverage instrumentation..."
LLVM_PROFILE_FILE="${RAW_DIR}/%p.profraw" ctest --test-dir "${BUILD_DIR}" --output-on-failure

TEST_BINARIES=()
while IFS= read -r bin; do
    TEST_BINARIES+=("${bin}")
done < <(find "${BUILD_DIR}/Tests" -type f -perm -111 -name '*_UnitTests')
if [ ${#TEST_BINARIES[@]} -eq 0 ]; then
    echo "No test binaries found under ${BUILD_DIR}/Tests." >&2
    exit 1
fi

"${LLVM_PROFDATA[@]}" merge -sparse "${RAW_DIR}"/*.profraw -o "${COVERAGE_DIR}/coverage.profdata"

OBJECT_ARGS=("${TEST_BINARIES[0]}")
for bin in "${TEST_BINARIES[@]:1}"; do
    OBJECT_ARGS+=(-object "${bin}")
done

# Only report on KryneEngine's own sources, not tests, third-party code, or generated files.
IGNORE_REGEX='(^|/)(External|Tests|_deps)/'

"${LLVM_COV[@]}" report "${OBJECT_ARGS[@]}" \
    -instr-profile="${COVERAGE_DIR}/coverage.profdata" \
    -ignore-filename-regex="${IGNORE_REGEX}"

"${LLVM_COV[@]}" show "${OBJECT_ARGS[@]}" \
    -instr-profile="${COVERAGE_DIR}/coverage.profdata" \
    -ignore-filename-regex="${IGNORE_REGEX}" \
    -format=html -output-dir="${COVERAGE_DIR}/html"

echo "HTML report: ${COVERAGE_DIR}/html/index.html"
