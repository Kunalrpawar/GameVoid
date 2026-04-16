"""
GameVoid Engine — SAM (Segment Anything) Object Segmenter
============================================================
Uses MobileSAM (a lightweight distilled version of Meta's SAM) to
perform interactive object segmentation from click points.

Supports:
  - Positive click points (include this object)
  - Negative click points (exclude this region)
  - Returns binary mask + cropped RGBA image with background removed

Falls back to rembg-based segmentation if MobileSAM is unavailable.
"""

import os
import sys
import time
import numpy as np
from pathlib import Path
from PIL import Image
from typing import List, Tuple, Optional

# Directory for caching model checkpoints
CACHE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "model_cache")
os.makedirs(CACHE_DIR, exist_ok=True)

# Global model cache
_sam_predictor = None
_sam_available = None  # None = not checked, True/False


def _check_sam_available() -> bool:
    """Check if MobileSAM or SAM is importable."""
    global _sam_available
    if _sam_available is not None:
        return _sam_available

    try:
        import torch
        # Try MobileSAM first (lighter weight)
        try:
            from mobile_sam import sam_model_registry, SamPredictor
            _sam_available = True
            print("[SAM] MobileSAM is available")
            return True
        except ImportError:
            pass

        # Try original SAM
        try:
            from segment_anything import sam_model_registry, SamPredictor
            _sam_available = True
            print("[SAM] segment-anything is available")
            return True
        except ImportError:
            pass

        _sam_available = False
        print("[SAM] Neither MobileSAM nor segment-anything installed")
        return False
    except ImportError:
        _sam_available = False
        print("[SAM] PyTorch not available")
        return False


def _load_predictor():
    """Load the SAM predictor (downloads checkpoint on first use)."""
    global _sam_predictor
    if _sam_predictor is not None:
        return _sam_predictor

    import torch

    device = "cuda" if torch.cuda.is_available() else "cpu"

    # Try MobileSAM first (much lighter ~10 MB)
    try:
        from mobile_sam import sam_model_registry, SamPredictor

        checkpoint_path = os.path.join(CACHE_DIR, "mobile_sam.pt")
        if not os.path.exists(checkpoint_path):
            print("[SAM] Downloading MobileSAM checkpoint...")
            try:
                from huggingface_hub import hf_hub_download
                hf_hub_download(
                    repo_id="dhkim2810/MobileSAM",
                    filename="mobile_sam.pt",
                    local_dir=CACHE_DIR,
                )
            except Exception:
                import urllib.request
                url = "https://raw.githubusercontent.com/ChaoningZhang/MobileSAM/master/weights/mobile_sam.pt"
                urllib.request.urlretrieve(url, checkpoint_path)

        if os.path.exists(checkpoint_path):
            model = sam_model_registry["vit_t"](checkpoint=checkpoint_path)
            model.to(device)
            model.eval()
            _sam_predictor = SamPredictor(model)
            print(f"[SAM] MobileSAM loaded on {device}")
            return _sam_predictor

    except (ImportError, Exception) as e:
        print(f"[SAM] MobileSAM load failed: {e}")

    # Fallback: try original SAM with vit_b (smallest)
    try:
        from segment_anything import sam_model_registry, SamPredictor

        checkpoint_path = os.path.join(CACHE_DIR, "sam_vit_b_01ec64.pth")
        if not os.path.exists(checkpoint_path):
            print("[SAM] Downloading SAM ViT-B checkpoint (~375 MB)...")
            import urllib.request
            url = "https://dl.fbaipublicfiles.com/segment_anything/sam_vit_b_01ec64.pth"
            urllib.request.urlretrieve(url, checkpoint_path)

        model = sam_model_registry["vit_b"](checkpoint=checkpoint_path)
        model.to(device)
        model.eval()
        _sam_predictor = SamPredictor(model)
        print(f"[SAM] SAM ViT-B loaded on {device}")
        return _sam_predictor

    except (ImportError, Exception) as e:
        print(f"[SAM] SAM load also failed: {e}")
        _sam_predictor = None
        return None


def segment_from_points(
    image_path: str,
    positive_points: List[Tuple[float, float]],
    negative_points: Optional[List[Tuple[float, float]]] = None,
) -> Optional[np.ndarray]:
    """
    Segment an object in an image using click points.

    Args:
        image_path: Path to the source image
        positive_points: List of (norm_x, norm_y) points ON the object to include
        negative_points: List of (norm_x, norm_y) points to EXCLUDE

    Returns:
        Boolean mask (H, W) where True = object, or None on failure
    """
    if not positive_points:
        return None

    img = Image.open(image_path).convert("RGB")
    img_np = np.array(img)
    h, w = img_np.shape[:2]

    # Convert normalized coords to pixel coords
    pos_px = [(int(x * (w - 1)), int(y * (h - 1))) for x, y in positive_points]
    neg_px = [(int(x * (w - 1)), int(y * (h - 1))) for x, y in (negative_points or [])]

    # Try SAM-based segmentation
    if _check_sam_available():
        predictor = _load_predictor()
        if predictor is not None:
            try:
                return _segment_sam(predictor, img_np, pos_px, neg_px)
            except Exception as e:
                print(f"[SAM] Inference failed: {e}, falling back to rembg")

    # Fallback: rembg + seed BFS
    return _segment_rembg_fallback(image_path, img_np, pos_px)


def _segment_sam(
    predictor,
    img_np: np.ndarray,
    positive_px: List[Tuple[int, int]],
    negative_px: List[Tuple[int, int]],
) -> Optional[np.ndarray]:
    """Run SAM inference with click points."""
    import torch

    predictor.set_image(img_np)

    # Build input arrays
    all_points = positive_px + negative_px
    labels = [1] * len(positive_px) + [0] * len(negative_px)

    input_points = np.array(all_points, dtype=np.float32)
    input_labels = np.array(labels, dtype=np.int32)

    masks, scores, _ = predictor.predict(
        point_coords=input_points,
        point_labels=input_labels,
        multimask_output=True,
    )

    # Pick the highest-scoring mask
    best_idx = scores.argmax()
    mask = masks[best_idx].astype(bool)

    print(f"[SAM] Segmented with score={scores[best_idx]:.3f}, mask covers {mask.sum()}/{mask.size} pixels")
    return mask


def _segment_rembg_fallback(
    image_path: str,
    img_np: np.ndarray,
    seed_points: List[Tuple[int, int]],
) -> Optional[np.ndarray]:
    """Fallback segmentation using rembg + flood fill from seed."""
    from collections import deque

    try:
        from rembg import remove
        img_rgba = remove(Image.open(image_path).convert("RGBA"))
        alpha = np.array(img_rgba)[:, :, 3]
        mask = alpha > 12
    except Exception:
        # Last resort: simple thresholding
        gray = np.mean(img_np, axis=2)
        mask = gray < 240  # Assume non-white is object

    if not mask.any():
        return None

    h, w = mask.shape

    # BFS from seed point to find connected component
    if seed_points:
        sx, sy = seed_points[0]
        sx = max(0, min(w - 1, sx))
        sy = max(0, min(h - 1, sy))

        if mask[sy, sx]:
            # Flood fill from seed
            visited = np.zeros_like(mask, dtype=bool)
            queue = deque([(sx, sy)])
            visited[sy, sx] = True
            component = []

            while queue:
                x, y = queue.popleft()
                component.append((y, x))
                for nx, ny in [(x-1, y), (x+1, y), (x, y-1), (x, y+1)]:
                    if 0 <= nx < w and 0 <= ny < h and not visited[ny, nx] and mask[ny, nx]:
                        visited[ny, nx] = True
                        queue.append((nx, ny))

            # Use only the connected component
            result = np.zeros_like(mask)
            for ry, rx in component:
                result[ry, rx] = True
            return result

    return mask


def create_masked_image(
    image_path: str,
    mask: np.ndarray,
    pad_percent: float = 0.03,
) -> Tuple[np.ndarray, Tuple[int, int, int, int]]:
    """
    Apply mask to image, crop to bounding box with padding.

    Returns:
        (cropped_rgba_array, (x0, y0, x1, y1) bounding box)
    """
    img = Image.open(image_path).convert("RGBA")
    rgba = np.array(img)
    h, w = mask.shape

    # Apply mask to alpha channel
    rgba[:, :, 3] = np.where(mask, 255, 0).astype(np.uint8)

    # Set background to neutral gray (helps with mesh generation)
    rgba[~mask, :3] = 127

    # Find bounding box
    ys, xs = np.where(mask)
    if len(ys) == 0:
        return rgba, (0, 0, w, h)

    x0, x1 = int(xs.min()), int(xs.max())
    y0, y1 = int(ys.min()), int(ys.max())

    pad = max(8, int(max(w, h) * pad_percent))
    x0 = max(0, x0 - pad)
    y0 = max(0, y0 - pad)
    x1 = min(w - 1, x1 + pad)
    y1 = min(h - 1, y1 + pad)

    cropped = rgba[y0:y1+1, x0:x1+1]
    return cropped, (x0, y0, x1, y1)


def create_mask_overlay(
    image_path: str,
    mask: np.ndarray,
    positive_points: List[Tuple[float, float]] = None,
    negative_points: List[Tuple[float, float]] = None,
    mask_color: Tuple[int, int, int] = (64, 200, 255),
    mask_alpha: float = 0.45,
) -> np.ndarray:
    """
    Create a visualization of the mask overlaid on the source image.
    Returns RGBA numpy array.
    """
    img = Image.open(image_path).convert("RGB")
    img_np = np.array(img, dtype=np.float32)
    h, w = img_np.shape[:2]

    # Blend mask color onto image
    overlay = img_np.copy()
    mask_rgb = np.array(mask_color, dtype=np.float32)
    overlay[mask] = overlay[mask] * (1 - mask_alpha) + mask_rgb * mask_alpha

    # Draw edge of mask for clarity
    try:
        import cv2
        mask_u8 = mask.astype(np.uint8) * 255
        contours, _ = cv2.findContours(mask_u8, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        overlay_u8 = overlay.astype(np.uint8)
        cv2.drawContours(overlay_u8, contours, -1, (255, 100, 200), 2)
        overlay = overlay_u8.astype(np.float32)
    except ImportError:
        pass

    result = overlay.astype(np.uint8)

    # Draw click points
    def draw_point(arr, nx, ny, color, radius=8):
        px, py = int(nx * (w - 1)), int(ny * (h - 1))
        rr, cc = [], []
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if dx * dx + dy * dy <= radius * radius:
                    iy, ix = py + dy, px + dx
                    if 0 <= iy < h and 0 <= ix < w:
                        rr.append(iy)
                        cc.append(ix)
        for iy, ix in zip(rr, cc):
            result[iy, ix] = color

    if positive_points:
        for pt in positive_points:
            draw_point(result, pt[0], pt[1], [0, 255, 100])

    if negative_points:
        for pt in negative_points:
            draw_point(result, pt[0], pt[1], [255, 60, 60])

    # Add alpha channel
    rgba = np.dstack([result, np.full((h, w), 255, dtype=np.uint8)])
    return rgba
