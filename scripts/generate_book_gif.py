"""Render an animated GIF of the order book demo.

Pipeline:
    1. Build the C++ demo (cmake + make, incremental).
    2. Run ``order_book_demo`` so it writes a fresh ``snapshots.json``
       describing the book state and trades after every event.
    3. Read that JSON and render one frame per snapshot, then assemble
       the frames into ``docs/order_book_demo.gif``.

The Python side does NOT model the order book. It is purely a renderer
over the snapshots emitted by the C++ engine, which means the animation
tracks engine behavior even as the engine evolves.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
DEMO_BIN = BUILD_DIR / "order_book_demo"
SNAPSHOT_PATH = BUILD_DIR / "snapshots.json"
OUT_GIF = PROJECT_ROOT / "docs" / "order_book_demo.gif"


# ---------------------------------------------------------------------------
# Step 1+2: build & run the C++ demo to produce a fresh snapshots.json.
# ---------------------------------------------------------------------------

def build_and_run_demo() -> None:
    if shutil.which("cmake") is None:
        sys.exit("error: cmake not found on PATH")

    BUILD_DIR.mkdir(exist_ok=True)
    subprocess.run(
        ["cmake", "..", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF"],
        cwd=BUILD_DIR, check=True,
    )
    subprocess.run(["make", "-j", "order_book_demo"], cwd=BUILD_DIR, check=True)

    if not DEMO_BIN.exists():
        sys.exit(f"error: demo binary not produced at {DEMO_BIN}")

    # Run the demo with our chosen snapshot path. The demo also prints its
    # human-readable trace to stdout — we suppress it here to keep the
    # rendering pipeline output clean.
    subprocess.run(
        [str(DEMO_BIN), str(SNAPSHOT_PATH)],
        cwd=BUILD_DIR, check=True, stdout=subprocess.DEVNULL,
    )

    if not SNAPSHOT_PATH.exists():
        sys.exit(f"error: demo did not write {SNAPSHOT_PATH}")


# ---------------------------------------------------------------------------
# Step 3: render frames from the JSON.
# ---------------------------------------------------------------------------

def load_frames() -> list[dict]:
    with open(SNAPSHOT_PATH) as f:
        data = json.load(f)
    return data["frames"]


def render(frames: list[dict]) -> Path:
    # Determine global axis bounds across the full animation.
    all_prices: set[int] = set()
    max_qty = 0
    for fr in frames:
        for side in ("bids", "asks"):
            for lvl in fr[side]:
                all_prices.add(lvl["price"])
                max_qty = max(max_qty, lvl["qty"])
    if not all_prices:
        all_prices = {99, 100, 101}
    max_qty = max(max_qty, 10)
    sorted_prices = sorted(all_prices)

    fig, ax = plt.subplots(figsize=(9, 5.2), dpi=110)

    def render_one(i: int) -> Image.Image:
        ax.clear()
        fr = frames[i]
        bdict = {lvl["price"]: lvl["qty"] for lvl in fr["bids"]}
        adict = {lvl["price"]: lvl["qty"] for lvl in fr["asks"]}

        bar_h = 0.7
        for p in sorted_prices:
            if p in bdict and bdict[p] > 0:
                ax.barh(p, -bdict[p], color="#2E7D32", edgecolor="white", height=bar_h)
                ax.text(-bdict[p] - max_qty * 0.02, p, f"{bdict[p]}",
                        va="center", ha="right", color="#1B5E20", fontsize=9)
            if p in adict and adict[p] > 0:
                ax.barh(p, adict[p], color="#C62828", edgecolor="white", height=bar_h)
                ax.text(adict[p] + max_qty * 0.02, p, f"{adict[p]}",
                        va="center", ha="left", color="#B71C1C", fontsize=9)

        bb = max(bdict) if bdict else None
        ba = min(adict) if adict else None
        bbo = f"BBO   bid: {bb if bb is not None else '—'}   ask: {ba if ba is not None else '—'}"
        if bb is not None and ba is not None:
            bbo += f"   spread: {ba - bb}"

        ax.set_title(f"Step {i}/{len(frames) - 1}: {fr['label']}",
                     fontsize=11, loc="left", pad=12, fontweight="bold")
        ax.axvline(0, color="#444", linewidth=0.8)
        ax.set_xlim(-max_qty * 1.35, max_qty * 1.35)
        ax.set_ylim(min(sorted_prices) - 1, max(sorted_prices) + 1)
        ax.set_yticks(sorted_prices)
        ax.set_xlabel("← bid quantity      ask quantity →", fontsize=9)
        ax.set_ylabel("price", fontsize=9)
        ax.grid(axis="x", alpha=0.15)
        ax.spines["top"].set_visible(False)
        ax.spines["right"].set_visible(False)

        ax.text(0.99, 1.02, bbo, transform=ax.transAxes,
                ha="right", va="bottom", fontsize=9,
                family="monospace", color="#222")

        new_trades = fr["trades"]
        if new_trades:
            msg = "TRADES:  " + "   ".join(
                f"#{t['aggressor']}↔#{t['resting']} @{t['price']}×{t['qty']}"
                for t in new_trades
            )
            color = "#0D47A1"
        else:
            msg = "no trades this step"
            color = "#777"
        ax.text(0.5, -0.18, msg, transform=ax.transAxes,
                ha="center", va="top", fontsize=10, color=color, family="monospace")

        fig.tight_layout(rect=(0, 0.04, 1, 1))
        fig.canvas.draw()
        w, h = fig.canvas.get_width_height()
        return Image.frombytes(
            "RGBA", (w, h), fig.canvas.buffer_rgba().tobytes()
        ).convert("RGB")

    images = [render_one(i) for i in range(len(frames))]

    # Frame durations (ms): linger longer on frames where trades occur.
    durations = []
    for i, fr in enumerate(frames):
        if i == 0:
            durations.append(900)
        elif fr["trades"]:
            durations.append(2400)
        else:
            durations.append(1500)
    durations[-1] = 3000

    OUT_GIF.parent.mkdir(exist_ok=True)
    images[0].save(
        OUT_GIF,
        save_all=True,
        append_images=images[1:],
        duration=durations,
        loop=0,
        optimize=True,
    )
    return OUT_GIF


def main() -> None:
    build_and_run_demo()
    frames = load_frames()
    out = render(frames)
    print(f"wrote {out} ({out.stat().st_size // 1024} KB, {len(frames)} frames)")


if __name__ == "__main__":
    main()
