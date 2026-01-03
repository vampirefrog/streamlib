#!/bin/bash
#
# Release automation script for StreamIO
# Usage: ./scripts/release.sh VERSION
#
# This script will:
# 1. Update version in CMakeLists.txt
# 2. Create a git tag
# 3. Push the tag to trigger the release workflow
#

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 VERSION"
    echo "Example: $0 0.1.0"
    exit 1
fi

VERSION="$1"
TAG="v${VERSION}"

# Validate version format (X.Y.Z)
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in format X.Y.Z (e.g., 0.1.0)"
    exit 1
fi

echo "Creating release ${TAG}"
echo "====================="
echo

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Must run from project root directory"
    exit 1
fi

# Check if working directory is clean
if ! git diff-index --quiet HEAD --; then
    echo "Error: Working directory is not clean. Commit your changes first."
    git status
    exit 1
fi

# Update version in CMakeLists.txt
echo "Updating version in CMakeLists.txt..."
sed -i.bak "s/^project(streamio VERSION [0-9.]*)/project(streamio VERSION ${VERSION})/" CMakeLists.txt
rm -f CMakeLists.txt.bak

# Show the diff
echo
echo "Version update:"
git diff CMakeLists.txt

# Confirm
echo
read -p "Continue with release ${TAG}? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborting."
    git checkout CMakeLists.txt
    exit 1
fi

# Commit version change if needed
echo
if git diff --quiet CMakeLists.txt; then
    echo "Version already set to ${VERSION}, no commit needed."
else
    echo "Committing version change..."
    git add CMakeLists.txt
    git commit -m "Release ${TAG}"
fi

# Create tag
echo
echo "Creating tag ${TAG}..."
git tag -a "${TAG}" -m "Release ${TAG}"

# Show instructions
echo
echo "Release ${TAG} prepared!"
echo
echo "To complete the release, push the tag:"
echo "  git push origin main ${TAG}"
echo
echo "This will trigger the GitHub Actions workflow to:"
echo "  - Build for Windows (static + DLL)"
echo "  - Build for Linux (static)"
echo "  - Build for macOS (static)"
echo "  - Create source archives"
echo "  - Publish release on GitHub"
echo
echo "To cancel, run:"
echo "  git tag -d ${TAG}"
echo "  git reset --hard HEAD~1"
