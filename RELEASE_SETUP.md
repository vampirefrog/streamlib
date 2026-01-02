# Release Automation Setup - Complete ✓

## What Has Been Set Up

Your StreamLib project now has complete release automation! Here's what's ready:

### 🚀 Automated Release Workflow

**Location:** `.github/workflows/release.yml`

**Triggers on:** Git tags matching `v*` (e.g., `v0.1.0`)

**Builds:**
1. ✅ **Windows x64 Static** - `stream.lib` + all dependency .lib files
2. ✅ **Windows x64 DLL** - `stream.dll` + all runtime DLLs (zlib1.dll, bz2.dll, etc.)
3. ✅ **Linux x64** - `libstream.a` static library
4. ✅ **macOS x64** - `libstream.a` static library
5. ✅ **Source Code** - Both .tar.gz and .zip archives

**Dependencies Bundled (Windows):**
- zlib (compression)
- bzip2 (compression)
- liblzma (compression)
- zstd (compression)
- libarchive (archives)
- All required DLLs (iconv, charset, etc.)

**Total Release Assets:** 7 files per release

### 📋 Release Helper Script

**Location:** `scripts/release.sh`

**Usage:**
```bash
./scripts/release.sh 0.1.0
git push origin main v0.1.0
```

**Features:**
- ✅ Version format validation
- ✅ Working directory checks
- ✅ Automatic version updates
- ✅ Git tag creation
- ✅ Clear instructions
- ✅ Easy rollback

### 📚 Documentation

**Created:**
- `RELEASE.md` - Complete release process guide
- `CHANGELOG.md` - Version history starting with v0.1.0
- `scripts/README.md` - Quick reference for scripts
- `RELEASE_SETUP.md` - This file

### ⚙️ Version Configuration

**Current version:** `0.1.0` (set in CMakeLists.txt)
**Project name:** `streamlib`

## How to Create v0.1.0 Release

### Method 1: Automatic (Recommended)

```bash
# 1. Make sure you're on main branch with latest changes
git checkout main
git pull

# 2. Ensure all tests pass
cd build
ctest
cd ..

# 3. Run the release script
./scripts/release.sh 0.1.0

# 4. Review the changes
git log -1
git show v0.1.0

# 5. Push to trigger release
git push origin main v0.1.0
```

### Method 2: Manual Trigger (No Git Tag)

If you want to test the release workflow without creating a tag:

1. Go to GitHub → Actions tab
2. Click "Release Build" workflow
3. Click "Run workflow"
4. Enter version: `0.1.0`
5. Click "Run workflow"

This will create the release without needing a git tag.

## What Happens After You Push

### Timeline (Approximate)

```
00:00  Push tag v0.1.0
00:01  GitHub Actions triggers
00:02  Create release job starts
00:03  5 build jobs start in parallel:
       - Windows Static
       - Windows DLL
       - Linux
       - macOS
       - Source Archives
00:15  All builds complete
00:16  Assets uploaded to release
00:17  Release published automatically
```

### Monitoring

Watch the build:
```bash
# In browser
https://github.com/YOUR_USERNAME/streamlib/actions

# Or with GitHub CLI
gh run watch
```

### Success Indicators

✅ All 5 jobs show green checkmarks
✅ 7 assets attached to release:
   - streamlib-0.1.0-source.tar.gz
   - streamlib-0.1.0-source.zip
   - streamlib-0.1.0-windows-x64-static.zip
   - streamlib-0.1.0-windows-x64-dll.zip
   - streamlib-0.1.0-linux-x64.tar.gz
   - streamlib-0.1.0-macos-x64.tar.gz
✅ Release published on GitHub

## Package Contents

### Windows Static Package
```
streamlib-0.1.0-windows-x64-static.zip
├── include/
│   └── stream.h
├── lib/
│   ├── stream.lib          (your library)
│   ├── zlib.lib
│   ├── bz2.lib
│   ├── lzma.lib
│   ├── zstd.lib
│   └── archive.lib
├── examples/
│   └── *.c
├── README.md
└── LICENSE
```

### Windows DLL Package
```
streamlib-0.1.0-windows-x64-dll.zip
├── include/
│   └── stream.h
├── lib/
│   └── stream.lib          (import library)
├── bin/
│   ├── stream.dll          (your DLL)
│   ├── zlib1.dll
│   ├── bz2.dll
│   ├── lzma.dll
│   ├── zstd.dll
│   ├── archive.dll
│   ├── liblzma.dll
│   ├── charset.dll
│   └── iconv.dll
├── examples/
│   └── *.c
├── README.md
└── LICENSE
```

### Linux/macOS Packages
```
streamlib-0.1.0-linux-x64.tar.gz
├── include/
│   └── stream.h
├── lib/
│   └── libstream.a
├── examples/
│   └── *.c
├── README.md             (platform-specific instructions)
└── LICENSE
```

## Future Releases

Making future releases is now super easy:

```bash
# For version 0.2.0
./scripts/release.sh 0.2.0
git push origin main v0.2.0

# For version 1.0.0
./scripts/release.sh 1.0.0
git push origin main v1.0.0
```

Just remember to:
1. Update CHANGELOG.md with new features
2. Run tests
3. Run the script
4. Push the tag

## Pre-Release Checklist

Before creating v0.1.0:

- [x] Release workflow configured
- [x] Release script created
- [x] Version set to 0.1.0
- [x] CHANGELOG.md created
- [x] Documentation complete
- [ ] All tests passing
- [ ] CI/CD pipeline green
- [ ] Archive creation feature tested
- [ ] Ready to publish

## Next Steps

1. **Review CHANGELOG.md** - Update if needed
2. **Run final tests** - Make sure everything works
3. **Check CI/CD** - Ensure latest commits pass
4. **Create release** - Run `./scripts/release.sh 0.1.0`
5. **Push tag** - `git push origin main v0.1.0`
6. **Wait ~15 minutes** - Watch the build in Actions
7. **Verify release** - Check all 7 assets are attached
8. **Celebrate!** 🎉

## Troubleshooting

### Common Issues

**"Working directory is not clean"**
```bash
git status
git add .
git commit -m "Prepare for release"
```

**"Permission denied: ./scripts/release.sh"**
```bash
chmod +x scripts/release.sh
```

**Windows builds fail**
- Check vcpkg cache in Actions logs
- May need to clear Actions cache and retry

**DLLs missing from package**
- DLL names may vary between vcpkg versions
- Check actual filenames in vcpkg/installed/x64-windows/bin/

### Getting Help

- **Release Process:** See [RELEASE.md](RELEASE.md)
- **Workflow Details:** See [.github/workflows/release.yml](.github/workflows/release.yml)
- **Script Help:** Run `./scripts/release.sh` without arguments

## Summary

You now have a professional release setup that:

✅ Creates releases with one command
✅ Builds for 3 platforms automatically
✅ Bundles all Windows dependencies
✅ Creates both static and DLL builds
✅ Generates source archives
✅ Publishes everything to GitHub
✅ Takes ~15 minutes total

No manual packaging, no manual uploads, no platform-specific builds needed!

---

**Ready to release?** Run: `./scripts/release.sh 0.1.0`
