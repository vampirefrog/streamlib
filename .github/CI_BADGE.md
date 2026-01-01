# CI Status Badge

Add this to the top of your README.md to show the CI build status:

```markdown
[![CI](https://github.com/YOUR_USERNAME/streamlib/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/streamlib/actions/workflows/ci.yml)
```

Replace `YOUR_USERNAME` with your GitHub username or organization name.

## Example

If your repository is at `https://github.com/johndoe/streamlib`, use:

```markdown
[![CI](https://github.com/johndoe/streamlib/actions/workflows/ci.yml/badge.svg)](https://github.com/johndoe/streamlib/actions/workflows/ci.yml)
```

This will display:
- ✅ Green badge when all tests pass
- ❌ Red badge when any test fails
- 🟡 Yellow badge when tests are running
