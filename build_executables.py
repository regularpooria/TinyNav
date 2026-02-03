#!/usr/bin/env python3
"""
Build script to create executables for Windows and Linux using PyInstaller.
"""
import subprocess
import sys
import os
import platform

# Force UTF-8 encoding for output (helps with Windows)
if sys.platform == 'win32':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        # Python < 3.7
        import codecs
        sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')

def build_executable(script_name, onefile=True, console=True, hidden_imports=None):
    """Build an executable using PyInstaller"""
    print(f"\n{'='*60}")
    print(f"Building {script_name}...")
    print(f"{'='*60}\n")
    
    cmd = [
        sys.executable, '-m', 'PyInstaller',
        '--clean',
        '--noconfirm',
    ]
    
    if onefile:
        cmd.append('--onefile')
    
    if console:
        cmd.append('--console')
    else:
        cmd.append('--noconsole')
    
    # Add hidden imports
    if hidden_imports:
        for imp in hidden_imports:
            cmd.extend(['--hidden-import', imp])
    
    # Platform-specific name
    system = platform.system()
    if system == 'Windows':
        name = script_name.replace('.py', '.exe')
    else:
        name = script_name.replace('.py', '')
    
    cmd.extend(['--name', name])
    cmd.append(script_name)
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=False)
        print(f"[OK] Successfully built {name}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"[FAIL] Failed to build {script_name}: {e}")
        return False

def main():
    # Get the directory where this script is located
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    print("Building executables for Python scripts...")
    print(f"Platform: {platform.system()}")
    print(f"Architecture: {platform.machine()}")
    print(f"Working directory: {os.getcwd()}")
    
    # Build visualize.py
    build_executable(
        'visualize.py',
        onefile=True,
        console=True,
        hidden_imports=[
            'numpy',
            'pandas',
            'matplotlib',
            'matplotlib.backends.backend_agg',
            'PIL',
            'PIL._imaging'
        ]
    )
    
    # Build download_log.py
    # Note: on Windows, curses requires windows-curses package
    curses_imports = ['curses']
    if platform.system() == 'Windows':
        curses_imports.append('_curses')
    
    build_executable(
        'download_log.py',
        onefile=True,
        console=True,
        hidden_imports=['serial', 'serial.tools', 'serial.tools.list_ports'] + curses_imports
    )
    
    # Build read_serial.py
    build_executable(
        'read_serial.py',
        onefile=True,
        console=True,
        hidden_imports=[
            'serial',
            'serial.tools',
            'serial.tools.list_ports',
            'numpy',
            'matplotlib',
            'matplotlib.backends.backend_tkagg',
            'PIL',
            'PIL._imaging'
        ]
    )
    
    print(f"\n{'='*60}")
    print("Build complete!")
    print(f"{'='*60}")
    print("\nExecutables are located in the 'dist' directory:")
    if os.path.exists('dist'):
        for item in os.listdir('dist'):
            full_path = os.path.join('dist', item)
            if os.path.isfile(full_path):
                size_mb = os.path.getsize(full_path) / (1024 * 1024)
                print(f"  - {item} ({size_mb:.1f} MB)")

if __name__ == '__main__':
    main()
