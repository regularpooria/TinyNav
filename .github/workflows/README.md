# GitHub Actions Workflows

## Build Executables Workflow

Automatically builds standalone executables for Linux and Windows.

### Triggers

The workflow runs on:

1. **Push to main/master branch** - When changes are made to:
   - `visualize.py`
   - `download_log.py`
   - `read_serial.py`
   - `build_executables.py`
   - `requirements.txt`
   - The workflow file itself

2. **Pull Requests** - To test builds before merging

3. **Manual Trigger** - Via "Actions" tab → "Build Executables" → "Run workflow"

4. **Release Creation** - Automatically attaches executables to GitHub releases

### What It Does

#### Linux Job (`build-linux`)
1. Sets up Ubuntu with Python 3.12
2. Installs dependencies from requirements.txt
3. Installs PyInstaller
4. Runs `build_executables.py`
5. Tests executables with `--help` flags
6. Packages into `tinynav-executables-linux-x86_64.tar.gz`
7. Uploads as artifact (available for 90 days)
8. If triggered by release, attaches to release

#### Windows Job (`build-windows`)
1. Sets up Windows with Python 3.12
2. Installs dependencies including windows-curses
3. Installs PyInstaller
4. Runs `build_executables.py`
5. Tests executables with `--help` flags
6. Packages into `tinynav-executables-windows-x86_64.zip`
7. Uploads as artifact (available for 90 days)
8. If triggered by release, attaches to release

#### Summary Job
- Runs after both builds complete
- Shows build status summary in the Actions tab

### Downloading Artifacts

After a successful workflow run:

1. Go to the "Actions" tab in GitHub
2. Click on the completed workflow run
3. Scroll to "Artifacts" section
4. Download:
   - `tinynav-linux-x86_64` (tar.gz file)
   - `tinynav-windows-x86_64` (zip file)

### Creating a Release with Executables

To automatically attach executables to a release:

```bash
# Create and push a tag
git tag v1.0.0
git push origin v1.0.0

# Or create a release through GitHub UI
# The workflow will build and attach executables automatically
```

### Manual Trigger

To manually trigger a build:

1. Go to "Actions" tab
2. Select "Build Executables" workflow
3. Click "Run workflow" button
4. Select branch
5. Click green "Run workflow" button

### Workflow File

Location: `.github/workflows/build-executables.yml`

### Requirements

- Python 3.12
- All packages from `requirements.txt`
- PyInstaller
- windows-curses (Windows only)

### Output Files

**Linux:**
- `tinynav-executables-linux-x86_64.tar.gz`
  - Contains: visualize, download_log, read_serial + docs

**Windows:**
- `tinynav-executables-windows-x86_64.zip`
  - Contains: visualize.exe, download_log.exe, read_serial.exe + docs

### Artifact Retention

Artifacts are kept for **90 days** by default. After that, they are automatically deleted. Release assets are kept permanently.
