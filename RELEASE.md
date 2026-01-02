# Release Process

This document describes how to create releases for StreamLib.

## Overview

Releases are fully automated via GitHub Actions. When you push a version tag (e.g., `v0.1.0`), the workflow automatically:

1. Creates a GitHub release
2. Builds binaries for all platforms
3. Packages everything with dependencies
4. Uploads release assets

## Release Assets

Each release includes:

### Source Code
- `streamio-X.Y.Z-source.tar.gz` - Source archive (gzip)
- `streamio-X.Y.Z-source.zip` - Source archive (zip)

### Windows x64
- `streamio-X.Y.Z-windows-x64-static.zip` - Static library (.lib) with all dependencies
- `streamio-X.Y.Z-windows-x64-dll.zip` - DLL build with all runtime DLLs included

**Windows static package includes:**
- `stream.lib` - Main library
- All dependency .lib files (zlib, bzip2, lzma, zstd, libarchive)
- Headers
- Examples
- Documentation

**Windows DLL package includes:**
- `stream.dll` + `stream.lib` - Main library
- All dependency DLLs (zlib1.dll, bz2.dll, lzma.dll, zstd.dll, archive.dll, etc.)
- Headers
- Examples
- Documentation

### Linux x64
- `streamio-X.Y.Z-linux-x64.tar.gz` - Static library (libstream.a)
  - Requires system libraries: zlib, bzip2, liblzma, libzstd, libarchive
  - Install deps: `sudo apt-get install zlib1g-dev libbz2-dev liblzma-dev libzstd-dev libarchive-dev`

### macOS x64
- `streamio-X.Y.Z-macos-x64.tar.gz` - Static library (libstream.a)
  - Requires Homebrew libraries: zlib, bzip2, xz, zstd, libarchive
  - Install deps: `brew install zlib bzip2 xz zstd libarchive`

## Creating a Release

### Method 1: Using the Release Script (Recommended)

```bash
# Make sure you're on the main branch with latest changes
git checkout main
git pull

# Run the release script
./scripts/release.sh 0.1.0

# Review the changes, then push
git push origin main v0.1.0
```

The script will:
1. Validate version format
2. Check working directory is clean
3. Update version in CMakeLists.txt
4. Commit the version change
5. Create a git tag
6. Show instructions for pushing

### Method 2: Manual Process

```bash
# 1. Update version in CMakeLists.txt
sed -i 's/^project(streamio VERSION .*/project(streamio VERSION 0.1.0)/' CMakeLists.txt

# 2. Commit the version change
git add CMakeLists.txt
git commit -m "Release v0.1.0"

# 3. Create and push tag
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin main v0.1.0
```

### Method 3: GitHub UI (Manual Trigger)

You can also trigger a release manually from GitHub:

1. Go to Actions tab
2. Select "Release Build" workflow
3. Click "Run workflow"
4. Enter version (e.g., `0.1.0`)
5. Click "Run workflow"

This is useful for rebuilding releases without creating a new tag.

## After Pushing the Tag

Once you push the tag, GitHub Actions will:

1. **Create Release Job** (~30 seconds)
   - Creates GitHub release draft
   - Generates release notes

2. **Build Jobs** (run in parallel, ~10-15 minutes total)
   - **Windows Static**: Builds static .lib with vcpkg dependencies
   - **Windows DLL**: Builds DLL with all runtime dependencies
   - **Linux**: Builds static library
   - **macOS**: Builds static library
   - **Source**: Creates source archives

3. **Upload Assets** (automatic)
   - All packages uploaded to the release
   - Release published automatically

## Monitoring Progress

Watch the build progress:
```bash
# Open in browser
https://github.com/YOUR_USERNAME/streamiolib/actions

# Or use GitHub CLI
gh run list --workflow=release.yml
gh run watch
```

## Release Checklist

Before creating a release:

- [ ] All tests passing (`ctest` in build directory)
- [ ] CI/CD pipeline green (check GitHub Actions)
- [ ] CHANGELOG updated with new features/fixes
- [ ] README updated if API changed
- [ ] Version bumped appropriately:
  - **Major**: Breaking API changes (X.0.0)
  - **Minor**: New features, backwards compatible (0.X.0)
  - **Patch**: Bug fixes, backwards compatible (0.0.X)
- [ ] No uncommitted changes
- [ ] On main branch

## Version Numbering

StreamLib follows [Semantic Versioning](https://semver.org/):

- **0.x.y** - Pre-1.0 releases (current phase)
  - Breaking changes allowed in minor versions
  - Use for initial development

- **1.0.0** - First stable release
  - API considered stable
  - Breaking changes only in major versions

## Troubleshooting

### Build Fails on Windows

**vcpkg dependency issues:**
```yaml
# Check vcpkg cache in Actions logs
# vcpkg will auto-cache dependencies between runs
# If cache is corrupted, manually clear it via Actions cache settings
```

**Missing DLLs in package:**
- Check the Copy-Item commands in `.github/workflows/release.yml`
- DLL names may vary between vcpkg versions
- Look in vcpkg installed directory for actual names

### Build Fails on Linux/macOS

**Missing dependencies:**
- Check apt-get/brew install commands in workflow
- Ensure all required libraries are listed

### Release Creation Fails

**Wrong tag format:**
- Must start with `v` (e.g., `v0.1.0`, not `0.1.0`)
- Use script to ensure correct format

**Permission denied:**
- Ensure GITHUB_TOKEN has write permissions
- Check repository settings → Actions → General → Workflow permissions

### Assets Not Uploading

**Upload timeout:**
- Large packages may timeout on slow runners
- Windows DLL packages are largest (~50MB)
- Workflow will retry automatically

**Wrong file path:**
- Check package directory names match upload globs
- Verify files are created in correct location

## Rolling Back a Release

If you need to delete a release:

```bash
# Delete the release on GitHub
gh release delete v0.1.0

# Delete the tag locally and remotely
git tag -d v0.1.0
git push origin :refs/tags/v0.1.0

# Revert the version commit
git revert HEAD
git push origin main
```

## Testing Releases Locally

Before pushing a tag, you can test packaging locally:

### Windows (PowerShell)
```powershell
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build --config Release

# Package (adapt from workflow)
$VERSION = "0.1.0-test"
$PKG_DIR = "streamio-${VERSION}-windows-x64-test"
New-Item -ItemType Directory -Force -Path "${PKG_DIR}/include"
# ... (copy files as in workflow)
Compress-Archive -Path "${PKG_DIR}" -DestinationPath "${PKG_DIR}.zip"
```

### Linux/macOS
```bash
# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Package
VERSION="0.1.0-test"
PKG_DIR="streamio-${VERSION}-linux-x64-test"
mkdir -p ${PKG_DIR}/{include,lib,examples}
cp -r include/* ${PKG_DIR}/include/
cp build/libstream.a ${PKG_DIR}/lib/
tar czf ${PKG_DIR}.tar.gz ${PKG_DIR}/
```

## Example Release Timeline

**v0.1.0** - Initial release
- Core stream functionality
- Compression support (gzip, bzip2, xz, zstd)
- Archive support (read/write)
- Path walker

**v0.2.0** - Feature additions (example)
- New compression formats
- Performance improvements
- Additional archive formats

**v1.0.0** - Stable release (example)
- API freeze
- Production ready
- Full documentation
- Complete test coverage
