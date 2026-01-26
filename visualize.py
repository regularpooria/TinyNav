import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from tqdm import tqdm

# --- Parameters ---
FPS = 20
FRAME_DT_MS = 1000 / FPS
csv_path = "logs/revised_log_0029_20260126_010822.csv"

# --- Load CSV ---
df = pd.read_csv(csv_path, comment="#", header=None)

# --- Parse frames ---
frames = []

for _, row in df.iterrows():
    w = int(row.iloc[1])
    h = int(row.iloc[2])
    data = row.iloc[3:].to_numpy(dtype=np.float32)
    frames.append(data.reshape((h, w)))

frames = np.stack(frames)
num_frames = len(frames)

# --- Figure setup (keep it minimal) ---
fig, ax = plt.subplots(figsize=(4, 4))
im = ax.imshow(
    frames[0],
    cmap="viridis",
    vmin=frames.min(),
    vmax=frames.max(),
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

# --- Save (FAST SETTINGS) ---
ani.save(
    "depth_animation.mp4",
    writer="ffmpeg",
    fps=FPS,
    dpi=80,                    # BIG speedup vs default 150+
    codec="libx264",
    extra_args=[
        "-preset", "ultrafast",  # fastest encode
        "-pix_fmt", "yuv420p"
    ],
    progress_callback=progress_callback
)

pbar.close()
