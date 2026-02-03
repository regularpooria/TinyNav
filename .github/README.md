# GitHub Configuration

This directory contains GitHub-specific configuration files.

## Contents

- **workflows/** - GitHub Actions workflow definitions
  - `build-executables.yml` - Main build workflow for Linux and Windows executables
  - `README.md` - Documentation for workflows

- **RELEASE_TEMPLATE.md** - Template for creating releases
- **WORKFLOW_GUIDE.md** - Complete guide for using GitHub Actions workflows

## Quick Links

### For Users

- [How to Download Executables](workflows/README.md#downloading-artifacts)
- [Release Notes Template](RELEASE_TEMPLATE.md)

### For Developers

- [Workflow Documentation](workflows/README.md)
- [Workflow Guide](WORKFLOW_GUIDE.md)
- [Triggering Builds Manually](WORKFLOW_GUIDE.md#manually-trigger-build)

## GitHub Actions

The repository uses GitHub Actions to automatically build executables for:
- Linux (x86_64)
- Windows (x86_64)

Builds are triggered on:
- Push to main/master branch
- Pull requests
- Manual workflow dispatch
- Release creation

See [Workflow Guide](WORKFLOW_GUIDE.md) for complete documentation.
