#!/usr/bin/env python3
"""
augment_dataset_with_yolo.py – post‑process a recorded **LeRobotDataset** by running
YOLO detection on the stored camera frames and appending the detections as a new
column (``yolo_detections``) in the underlying 🤗 datasets.Dataset.  Optionally the
augmented dataset can be pushed to the Hub under a new repo‑id so it is directly
usable with the standard LeRobot training / evaluation pipeline.

Each entry in ``yolo_detections`` is a *list of detections* for that frame.  A
single detection is encoded exactly like the live‑stream script you wrote
(``yolo_display.py``):

    [center_x, center_y, width, height, class_id, confidence]

All values are **float** except ``class_id`` (int).  Units are *pixels* in the
original image resolution.

Example — run YOLO‑v8n on an existing dataset and push to the Hub:

```bash
python lerobot/scripts/augment_dataset_with_yolo.py \
  --repo-id  StarLionJiang/so100_pick_try3   \
  --root     data                             \
  --model-path yolov8n.pt                     \
  --classes   39 65 76                        \
  --batch-size 16                             \
  --output-repo-id StarLionJiang/so100_pick_try3_yolo \
  --push-to-hub
```

The script makes **no change** to the original dataset on disk (it writes a new
``with_yolo`` folder next to it) and uses the ordinary ``LeRobotDataset`` API
so nothing else in the pipeline needs to change.
"""

import argparse
import logging
from pathlib import Path
from typing import List, Sequence

import numpy as np
import torch
from tqdm import tqdm
from ultralytics import YOLO

from lerobot.common.datasets.lerobot_dataset import LeRobotDataset


def chw_float_to_hwc_uint8(img: torch.Tensor) -> np.ndarray:
    """Convert a CHW float32 tensor in [0,1] (LeRobotDataset format) to HWC uint8."""
    assert img.ndim == 3 and img.dtype == torch.float32, "Expect CHW float32 image"
    img_uint8 = (img.clamp(0.0, 1.0) * 255).to(torch.uint8)
    return img_uint8.permute(1, 2, 0).cpu().numpy()


def run_yolo_on_batch(model: YOLO, images: Sequence[np.ndarray], classes: List[int] | None):
    """Run YOLO on a batch of np.ndarray images (HWC uint8)."""
    results = model(images, classes=classes, verbose=False)
    out: list[list[list[float]]] = []
    for r in results:
        dets: list[list[float]] = []
        if r.boxes is not None and len(r.boxes) > 0:
            boxes = r.boxes.xyxy.cpu().numpy()
            confs = r.boxes.conf.cpu().numpy()
            cls   = r.boxes.cls.cpu().numpy().astype(int)
            for (x1, y1, x2, y2), conf, cid in zip(boxes, confs, cls):
                cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
                dets.append([float(cx), float(cy), float(x2 - x1), float(y2 - y1), int(cid), float(conf)])
        out.append(dets)
    return out


def main():
    parser = argparse.ArgumentParser("Append YOLO detections to an existing LeRobotDataset")
    parser.add_argument("--repo-id", required=True, help="Source LeRobotDataset repo‑id or local name")
    parser.add_argument("--root", type=Path, default=None, help="Optional local root directory for the dataset")
    parser.add_argument("--model-path", default="yolov8n.pt", help="Path or name of YOLO model checkpoint")
    parser.add_argument("--classes", type=int, nargs="*", default=None, help="Optional class id filter (e.g. 39 65 76)")
    parser.add_argument("--batch-size", type=int, default=8, help="YOLO inference batch size")
    parser.add_argument("--output-repo-id", type=str, default=None, help="Push the augmented dataset here (defaults to --repo-id)")
    parser.add_argument("--push-to-hub", action="store_true", help="If set, push the augmented dataset to the Hub")

    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
    logging.info(f"Loading LeRobotDataset '{args.repo_id}' …")
    ds = LeRobotDataset(args.repo_id, root=args.root)
    hf_ds = ds.hf_dataset  # 🤗 Datasets object

    cam_key = ds.meta.camera_keys[0]
    logging.info(f"Using camera key '{cam_key}' for YOLO inference")

    model = YOLO(args.model_path)

    # Storage for detections corresponding to dataset rows
    detections: list[list[list[float]]] = [None] * len(hf_ds)

    images_buf: list[np.ndarray] = []
    idx_buf:    list[int]       = []

    def flush():
        """Run inference on buffered images, write results, then clear buffer."""
        nonlocal images_buf, idx_buf
        if not images_buf:
            return
        det_batch = run_yolo_on_batch(model, images_buf, args.classes)
        for row_idx, det in zip(idx_buf, det_batch):
            detections[row_idx] = det
        images_buf.clear()
        idx_buf.clear()

    logging.info("Running YOLO inference … (this may take a while)")
    for row_idx, example in enumerate(tqdm(hf_ds, total=len(hf_ds))):
        img = chw_float_to_hwc_uint8(example[cam_key])
        images_buf.append(img)
        idx_buf.append(row_idx)
        if len(images_buf) == args.batch_size:
            flush()
    flush()  # final partial batch

    # Add the new column to a *new* dataset object to keep the original untouched
    logging.info("Adding 'yolo_detections' column to the dataset")
    new_hf_ds = hf_ds.add_column("yolo_detections", detections)

    # Persist to disk next to the original dataset folder
    save_dir = (ds.root or Path.home() / ".cache/huggingface/datasets") / (ds.repo_id.replace("/", "_") + "_with_yolo")
    save_dir.mkdir(parents=True, exist_ok=True)
    logging.info(f"Saving augmented dataset to {save_dir}")
    new_hf_ds.save_to_disk(str(save_dir))

    if args.push_to_hub:
        target_repo = args.output_repo_id or args.repo_id
        logging.info(f"Pushing augmented dataset to the Hub at '{target_repo}' …")
        new_hf_ds.push_to_hub(target_repo)
        logging.info("Push complete ✔️")

    logging.info("Done – the dataset now contains a 'yolo_detections' column ready for training 🐙")


if __name__ == "__main__":
    main()
