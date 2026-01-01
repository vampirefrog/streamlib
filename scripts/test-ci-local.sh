#!/bin/bash
# Local CI testing script - mimics GitHub Actions workflow

set -e

echo "=========================================="
echo "StreamLib Local CI Test"
echo "=========================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    NPROC=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    NPROC=$(sysctl -n hw.ncpu)
else
    echo -e "${RED}Unsupported OS: $OSTYPE${NC}"
    exit 1
fi

echo "Detected OS: $OS"
echo "CPU cores: $NPROC"
echo ""

# Check for required tools
echo "Checking required tools..."
command -v cmake >/dev/null 2>&1 || { echo -e "${RED}cmake not found${NC}"; exit 1; }
command -v gcc >/dev/null 2>&1 || { echo -e "${RED}gcc not found${NC}"; exit 1; }

if [[ "$OS" == "linux" ]]; then
    command -v valgrind >/dev/null 2>&1 || echo -e "${YELLOW}Warning: valgrind not found (optional)${NC}"
fi

echo -e "${GREEN}✓ Required tools found${NC}"
echo ""

# Check for optional dependencies
echo "Checking optional dependencies..."
DEPS_FOUND=0

if pkg-config --exists zlib 2>/dev/null; then
    echo -e "${GREEN}✓ zlib found${NC}"
    ((DEPS_FOUND++))
else
    echo -e "${YELLOW}✗ zlib not found${NC}"
fi

if pkg-config --exists bzip2 2>/dev/null || [[ -f /usr/include/bzlib.h ]]; then
    echo -e "${GREEN}✓ bzip2 found${NC}"
    ((DEPS_FOUND++))
else
    echo -e "${YELLOW}✗ bzip2 not found${NC}"
fi

if pkg-config --exists liblzma 2>/dev/null; then
    echo -e "${GREEN}✓ lzma found${NC}"
    ((DEPS_FOUND++))
else
    echo -e "${YELLOW}✗ lzma not found${NC}"
fi

if pkg-config --exists libzstd 2>/dev/null; then
    echo -e "${GREEN}✓ zstd found${NC}"
    ((DEPS_FOUND++))
else
    echo -e "${YELLOW}✗ zstd not found${NC}"
fi

if pkg-config --exists libarchive 2>/dev/null; then
    echo -e "${GREEN}✓ libarchive found${NC}"
    ((DEPS_FOUND++))
else
    echo -e "${YELLOW}✗ libarchive not found${NC}"
fi

echo ""
echo "Optional dependencies found: $DEPS_FOUND/5"
echo ""

# Clean previous build
if [ -d build ]; then
    echo "Removing old build directory..."
    rm -rf build
fi

# Configure
echo "=========================================="
echo "Configuring with CMake..."
echo "=========================================="
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
echo ""

# Build
echo "=========================================="
echo "Building..."
echo "=========================================="
cmake --build . --parallel $NPROC
echo ""

# Run tests
echo "=========================================="
echo "Running tests..."
echo "=========================================="

TESTS_PASSED=0
TESTS_FAILED=0

run_test() {
    local test_name=$1
    local test_bin=$2

    if [ -f "$test_bin" ]; then
        echo -n "Running $test_name... "
        if $test_bin > /tmp/${test_name}.log 2>&1; then
            echo -e "${GREEN}PASSED${NC}"
            ((TESTS_PASSED++))
        else
            echo -e "${RED}FAILED${NC}"
            ((TESTS_FAILED++))
            echo "See /tmp/${test_name}.log for details"
        fi
    else
        echo -e "${YELLOW}Skipping $test_name (not built)${NC}"
    fi
}

run_test "test_basic" "./tests/test_basic"
run_test "test_compression" "./tests/test_compression"
run_test "test_walker" "./tests/test_walker"
run_test "test_archive" "./tests/test_archive"

echo ""
echo "Tests passed: $TESTS_PASSED"
echo "Tests failed: $TESTS_FAILED"
echo ""

# Memory leak check (Linux only)
if [[ "$OS" == "linux" ]] && command -v valgrind >/dev/null 2>&1; then
    echo "=========================================="
    echo "Running valgrind memory leak checks..."
    echo "=========================================="

    VALGRIND_OPTS="--leak-check=full --error-exitcode=1 --quiet"

    if [ -f ./tests/test_basic ]; then
        echo -n "Checking test_basic... "
        if valgrind $VALGRIND_OPTS ./tests/test_basic > /dev/null 2>&1; then
            echo -e "${GREEN}NO LEAKS${NC}"
        else
            echo -e "${RED}LEAKS DETECTED${NC}"
            exit 1
        fi
    fi

    if [ -f ./tests/test_compression ]; then
        echo -n "Checking test_compression... "
        if valgrind $VALGRIND_OPTS ./tests/test_compression > /dev/null 2>&1; then
            echo -e "${GREEN}NO LEAKS${NC}"
        else
            echo -e "${RED}LEAKS DETECTED${NC}"
            exit 1
        fi
    fi

    echo ""
fi

# Summary
echo "=========================================="
echo "Summary"
echo "=========================================="
echo -e "OS: $OS"
echo -e "Dependencies: $DEPS_FOUND/5"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    exit 1
else
    echo -e "Tests failed: $TESTS_FAILED"
fi

if [[ "$OS" == "linux" ]] && command -v valgrind >/dev/null 2>&1; then
    echo -e "Memory leaks: ${GREEN}None${NC}"
fi

echo ""
echo -e "${GREEN}✓ All checks passed!${NC}"
