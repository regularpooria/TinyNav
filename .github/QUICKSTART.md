# GitHub Actions Quick Start

Get your workflow running in 5 steps.

## Step 1: Push the Workflow

```bash
git add .github/
git commit -m "Add GitHub Actions workflow for building executables"
git push origin main
```

## Step 2: Enable Actions (if needed)

If this is a new repository:
1. Go to repository on GitHub
2. Click "Actions" tab
3. If prompted, click "I understand my workflows, go ahead and enable them"

## Step 3: Trigger Your First Build

**Option A: Manual Trigger (Recommended for first time)**
1. Go to "Actions" tab
2. Click "Build Executables" in left sidebar
3. Click "Run workflow" button
4. Select branch (main)
5. Click green "Run workflow" button

**Option B: Push a Change**
```bash
# Make any change to trigger files
echo "# Test" >> visualize.py
git add visualize.py
git commit -m "Test workflow trigger"
git push
```

## Step 4: Monitor the Build

1. Go to "Actions" tab
2. Click on the running workflow
3. Watch the progress:
   - `build-linux` and `build-windows` run in parallel
   - Each takes ~5-8 minutes
   - Green ✓ = success, Red ✗ = failed

## Step 5: Download Your Executables

After build completes successfully:
1. Scroll down to "Artifacts" section
2. Download:
   - `tinynav-linux-x86_64` → Extract .tar.gz
   - `tinynav-windows-x86_64` → Extract .zip

## Bonus: Create a Release

```bash
# Tag a version
git tag v1.0.0
git push origin v1.0.0

# Create release on GitHub from this tag
# Workflow will automatically attach executables!
```

## That's It!

Your workflow is now active and will automatically build executables whenever you:
- Push changes to the source files
- Create a pull request
- Create a new release
- Manually trigger it

## Need Help?

See full documentation:
- `.github/workflows/README.md` - Workflow details
- `.github/README.md` - Overview
- `BUILD_EXECUTABLES.md` - Build instructions
