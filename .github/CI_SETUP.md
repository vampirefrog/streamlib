# GitHub Actions CI Setup

This project uses GitHub Actions for continuous integration.

## Workflows

### 1. Main CI (`ci.yml`)

Runs on every push and pull request to master/main/develop branches.

**Jobs:**

- **Linux Build Matrix**
  - Compilers: GCC, Clang
  - Configurations: Full (all deps), Minimal (no optional deps)
  - Runs valgrind memory leak checks on full configuration

- **macOS Build Matrix**
  - Configurations: Full, Minimal
  - Tests on latest macOS

- **Code Coverage**
  - Generates coverage report using lcov
  - Linux only (GCC)

- **Static Analysis**
  - Runs cppcheck
  - Exports compile commands
  - Captures build warnings

- **Documentation Check**
  - Verifies README and examples exist
  - Checks header documentation

### 2. Release Build (`release.yml`)

Triggers on version tags (v*).

**Jobs:**

- **Create Release**
  - Creates GitHub release from tag

- **Build Release Artifacts**
  - Builds on Ubuntu and macOS
  - Runs full test suite
  - Creates distributable packages
  - Uploads to GitHub release

## Test Execution

All workflows run the test suite:

```bash
./tests/test_basic        # Core functionality
./tests/test_compression  # Compression support (if available)
./tests/test_walker       # Path walker
./tests/test_archive      # Archive support (if available)
```

## Memory Leak Detection

On Linux with full configuration, valgrind checks for memory leaks:

```bash
valgrind --leak-check=full --error-exitcode=1 ./tests/test_*
```

CI fails if any leaks are detected.

## Local Testing

To replicate CI environment locally:

### Linux (Ubuntu):

```bash
# Install dependencies
sudo apt-get install -y \
  zlib1g-dev libbz2-dev liblzma-dev libzstd-dev libarchive-dev valgrind

# Build and test
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
ctest --output-on-failure

# Memory leak check
valgrind --leak-check=full ./tests/test_basic
```

### macOS:

```bash
# Install dependencies
brew install zlib bzip2 xz zstd libarchive

# Build and test
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
ctest --output-on-failure
```

### Minimal build (no optional dependencies):

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
./tests/test_basic  # Only basic tests will run
```

## CI Status

Check CI status at:
- https://github.com/YOUR_USERNAME/streamlib/actions

Add status badge to README.md:
```markdown
[![CI](https://github.com/YOUR_USERNAME/streamlib/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/streamlib/actions/workflows/ci.yml)
```

## Troubleshooting

### Tests fail on minimal build
- Expected if tests require optional dependencies
- Use `|| true` to allow failure (already configured)

### Valgrind errors
- Fix memory leaks in source code
- Verify all `malloc()` have corresponding `free()`
- Check stream cleanup order

### macOS-specific issues
- Ensure dependencies installed via Homebrew
- May need to set `ZLIB_ROOT` and `BZIP2_ROOT` CMake variables
- Check library paths with `otool -L`

## Future Improvements

- [ ] Add Windows CI (using MSVC)
- [ ] Increase test timeout for slower runners
- [ ] Add performance benchmarks
- [ ] Collect and publish coverage reports
- [ ] Add sanitizer builds (ASAN, UBSAN)
