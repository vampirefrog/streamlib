# Development Guide

## Working with Claude CLI

This project is designed to be developed using Claude CLI (claude-code).

### Getting Started
```bash
# Navigate to project root
cd streamio

# Start Claude CLI session
claude-code
```

### Development Workflow

1. **Check current status**: Review TODO.md for next tasks
2. **Implement feature**: Work on one component at a time
3. **Test**: Build and run tests after each component
4. **Commit**: Commit working code frequently

### Building
```bash
# Configure
cmake -B build

# Build
cmake --build build

# Run tests
cd build && ctest
```

### Asking Claude for Help

Example prompts:

- "Implement the base stream structure and operations from the spec"
- "Create the file_stream implementation for POSIX systems"
- "Add unit tests for mem_stream"
- "Implement gzip compression stream using zlib"
- "Add example program that walks a directory and lists compressed files"

### Code Style

- Follow Linux kernel coding style
- Use tabs for indentation (width 8)
- Max line length: 80 characters
- Function names: lowercase_with_underscores
- Type names: lowercase_with_underscores (struct stream)
- Constants: UPPERCASE_WITH_UNDERSCORES

### Testing

All new code should include:
- Unit tests in tests/
- Example usage in examples/
- Documentation updates

