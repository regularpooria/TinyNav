import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from tqdm import tqdm
import argparse
import sys

# --- Parse command-line arguments ---
parser = argparse.ArgumentParser(
    description='Create GIF animation from CSV depth log files',
    formatter_class=argparse.ArgumentDefaultsHelpFormatter
)
parser.add_argument('csv_file', nargs='?', default='revised_log_0053.csv',
                    help='Input CSV file path')
parser.add_argument('-o', '--output', default='depth_animation.gif',
                    help='Output file path (GIF format)')
parser.add_argument('-f', '--fps', type=int, default=15,
                    help='Frames per second')
parser.add_argument('--dpi', type=int, default=80,
                    help='Output DPI (higher = better quality but slower)')
parser.add_argument('--vmin', type=int, default=100,
                    help='Minimum depth value for colormap')
parser.add_argument('--vmax', type=int, default=2000,
                    help='Maximum depth value for colormap')
parser.add_argument('--cmap', default='viridis',
                    help='Matplotlib colormap name')

args = parser.parse_args()

# Validate output format
if not args.output.lower().endswith('.gif'):
    print("WARNING: Pillow writer only supports GIF format.")
    print(f"Changing output from '{args.output}' to '{args.output.rsplit('.', 1)[0]}.gif'")
    args.output = args.output.rsplit('.', 1)[0] + '.gif'

# --- Parameters ---
FPS = args.fps
FRAME_DT_MS = 1000 / FPS
csv_path = args.csv_file

# --- Load CSV ---
try:
    df = pd.read_csv(csv_path, comment="#", header=None)
except FileNotFoundError:
    print(f"ERROR: File not found: {csv_path}")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: Failed to read CSV: {e}")
    sys.exit(1)

# --- Parse frames ---
frames = []

for _, row in df.iterrows():
    w = int(row.iloc[3])
    h = int(row.iloc[4])
    data = row.iloc[5:].to_numpy(dtype=np.float32)
    frames.append(data.reshape((h, w)))

frames = np.stack(frames)
num_frames = len(frames)

# --- Figure setup (keep it minimal) ---
fig, ax = plt.subplots(figsize=(4, 4))
im = ax.imshow(
    frames[0],
    cmap=args.cmap,
    vmin=args.vmin,
    vmax=args.vmax,
    animated=False
)

ax.axis("off")  # faster than drawing ticks
text = ax.text(
    0.02, 0.98, "",
    transform=ax.transAxes,
    color="white",
    fontsize=9,
    va="top",
    bbox=dict(facecolor="black", alpha=0.5, pad=2)
)

# --- Update ---
def update(i):
    im.set_data(frames[i])
    text.set_text(f"Frame {i}  |  {i * FRAME_DT_MS:.0f} ms")
    return im, text

ani = FuncAnimation(
    fig,
    update,
    frames=num_frames,
    interval=FRAME_DT_MS,
    blit=False   # IMPORTANT: faster with text overlays
)

# --- Progress bar ---
pbar = tqdm(total=num_frames, desc="Encoding")

def progress_callback(i, total):
    pbar.update(1)

# --- Save using Pillow writer (GIF format only) ---
print(f"Saving animation to {args.output}...")
print("Note: Using Pillow writer (GIF format). For MP4, use ffmpeg.")

ani.save(
    args.output,
    writer="pillow",
    fps=FPS,
    dpi=args.dpi,
    progress_callback=progress_callback
)

pbar.close()
print(f"✓ Animation saved to {args.output}")
