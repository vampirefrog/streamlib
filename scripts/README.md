# Scripts Directory

## Release Script

### Quick Start

```bash
# Create version 0.1.0
./scripts/release.sh 0.1.0

# Push to trigger release
git push origin main v0.1.0
```

### What it does

1. ✓ Validates version format (X.Y.Z)
2. ✓ Checks working directory is clean
3. ✓ Updates version in CMakeLists.txt
4. ✓ Commits the version change
5. ✓ Creates git tag
6. ✓ Shows push instructions

### What happens next

When you push the tag, GitHub Actions automatically:

1. Creates GitHub release
2. Builds for all platforms:
   - **Windows x64 Static** - .lib files with all dependencies
   - **Windows x64 DLL** - .dll files with all runtime dependencies
   - **Linux x64** - Static library
   - **macOS x64** - Static library
   - **Source code** - .tar.gz and .zip
3. Uploads all packages to the release
4. Publishes release automatically

### Build time

Approximately 10-15 minutes for all platforms.

### Requirements

- Git repository with GitHub remote
- Clean working directory (no uncommitted changes)
- On main branch
- All tests passing

### Troubleshooting

**Script not executable:**
```bash
chmod +x scripts/release.sh
```

**Wrong version format:**
```bash
# ✓ Correct
./scripts/release.sh 0.1.0

# ✗ Wrong
./scripts/release.sh v0.1.0  # Don't include 'v' prefix
./scripts/release.sh 0.1     # Must be X.Y.Z format
```

**Working directory not clean:**
```bash
# Commit or stash your changes first
git status
git add .
git commit -m "Your changes"

# Then run release script
./scripts/release.sh 0.1.0
```

**Want to cancel after running script:**
```bash
# Delete the tag
git tag -d v0.1.0

# Reset the commit
git reset --hard HEAD~1
```

### See Also

- [RELEASE.md](../RELEASE.md) - Complete release documentation
- [CHANGELOG.md](../CHANGELOG.md) - Version history
- [.github/workflows/release.yml](../.github/workflows/release.yml) - Release automation
