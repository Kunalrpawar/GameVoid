"""
GameVoid Engine — AI Model Server (Flask)
==========================================
Local REST API server that provides Image → 3D Model generation.

Endpoints:
    GET  /health     — Server health check
    POST /analyze    — Analyze image (Gemini Vision → object classification)
    POST /generate   — Generate 3D model from image (TripoSR or MiDaS)

The server runs on localhost:5000 and is called by the GameVoid C++ engine.
"""

import os
import sys
import json
import time
import uuid
import argparse
import shutil
from pathlib import Path

from flask import Flask, request, jsonify
from PIL import Image
import numpy as np

# Add the ai_server directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from image_analyzer import ImageAnalyzer
import mesh_generator
import mesh_generator_midas
import sam_segmenter

# ── Configuration ────────────────────────────────────────────────────────────

app = Flask(__name__)

# Default output directory (relative to gamevoid root)
GAMEVOID_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(GAMEVOID_ROOT, "generated_models")
CONFIG_FILE = os.path.join(GAMEVOID_ROOT, "gamevoid_config.ini")

# Temp directory for uploaded images
UPLOAD_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uploads")
os.makedirs(UPLOAD_DIR, exist_ok=True)
os.makedirs(OUTPUT_DIR, exist_ok=True)


def _load_api_key() -> str:
    """Load the Gemini API key from the GameVoid config file."""
    if not os.path.exists(CONFIG_FILE):
        return os.environ.get("GEMINI_API_KEY", "")

    with open(CONFIG_FILE, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("apiKey="):
                return line.split("=", 1)[1].strip()
    return os.environ.get("GEMINI_API_KEY", "")


def _clamp01(v: float) -> float:
    return max(0.0, min(1.0, float(v)))


def _crop_from_normalized_selection(image_path: str, data: dict) -> str:
    """Crop selected normalized region and return cropped temp image path.
    Falls back to original path if selection is invalid.
    """
    if not data.get("use_selection", False):
        return image_path

    try:
        min_x = _clamp01(data.get("selection_min_x", 0.0))
        min_y = _clamp01(data.get("selection_min_y", 0.0))
        max_x = _clamp01(data.get("selection_max_x", 1.0))
        max_y = _clamp01(data.get("selection_max_y", 1.0))

        if max_x < min_x:
            min_x, max_x = max_x, min_x
        if max_y < min_y:
            min_y, max_y = max_y, min_y

        with Image.open(image_path) as img:
            w, h = img.size
            x0 = int(min_x * w)
            y0 = int(min_y * h)
            x1 = int(max_x * w)
            y1 = int(max_y * h)

            # Ensure non-empty crop and valid bounds
            x0 = max(0, min(x0, w - 1))
            y0 = max(0, min(y0, h - 1))
            x1 = max(x0 + 1, min(x1, w))
            y1 = max(y0 + 1, min(y1, h))

            if (x1 - x0) < 8 or (y1 - y0) < 8:
                print("[Server] Selection too small, using original image")
                return image_path

            cropped = img.crop((x0, y0, x1, y1))
            temp_crop = os.path.join(UPLOAD_DIR, f"crop_{uuid.uuid4().hex[:8]}.png")
            cropped.save(temp_crop)
            print(f"[Server] Using selected crop region: ({x0},{y0})-({x1},{y1}) -> {temp_crop}")
            return temp_crop
    except Exception as e:
        print(f"[Server] Crop selection failed ({e}); using original image")
        return image_path


def _prepare_work_image(image_path: str, data: dict) -> str:
    """Prepare the image used by generation.

    Priority:
      1) Provided masked image path (precomputed by /segment)
      2) SAM point-based segmentation (positive + negative points)
      3) Legacy single smart-point segmentation
      4) Rectangle crop fallback
      5) Original image
    """
    mask_path = data.get("mask_image_path", "")
    if mask_path and os.path.exists(mask_path):
        print(f"[Server] Using provided mask image: {mask_path}")
        return mask_path

    positive_points = data.get("positive_points", [])
    negative_points = data.get("negative_points", [])

    if not positive_points and data.get("use_smart_point", False):
        positive_points = [[_clamp01(data.get("smart_point_x", 0.5)),
                            _clamp01(data.get("smart_point_y", 0.5))]]

    if positive_points:
        try:
            pos = [(float(p[0]), float(p[1])) for p in positive_points]
            neg = [(float(p[0]), float(p[1])) for p in negative_points]
            mask = sam_segmenter.segment_from_points(image_path, pos, neg)
            if mask is not None and mask.any():
                cropped, _ = sam_segmenter.create_masked_image(image_path, mask)
                out_path = os.path.join(UPLOAD_DIR, f"smart_{uuid.uuid4().hex[:8]}.png")
                Image.fromarray(cropped, mode="RGBA").save(out_path)
                print(
                    f"[Server] SAM pre-segmentation applied ({len(pos)}+/{len(neg)}-) -> {out_path}"
                )
                return out_path
        except Exception as e:
            print(f"[Server] SAM pre-segmentation failed ({e}); falling back")

    return _crop_from_normalized_selection(image_path, data)


def _export_model_file(obj_path: str, texture_path: str, export_format: str, output_dir: str = ""):
    """Export generated OBJ model into requested format (obj/gltf/glb)."""
    fmt = (export_format or "obj").strip().lower()
    if fmt not in ("obj", "gltf", "glb"):
        raise ValueError(f"Unsupported export format: {export_format}")

    obj_path = os.path.normpath(obj_path)
    if not os.path.exists(obj_path):
        raise FileNotFoundError(f"OBJ not found: {obj_path}")

    src_dir = os.path.dirname(obj_path)
    stem = Path(obj_path).stem
    dst_dir = os.path.normpath(output_dir) if output_dir else src_dir
    os.makedirs(dst_dir, exist_ok=True)

    if fmt == "obj":
        exported = []
        obj_out = os.path.join(dst_dir, f"{stem}.obj")
        if os.path.normcase(obj_out) != os.path.normcase(obj_path):
            shutil.copy2(obj_path, obj_out)
        else:
            obj_out = obj_path
        exported.append(obj_out)

        mtl_src = os.path.splitext(obj_path)[0] + ".mtl"
        if os.path.exists(mtl_src):
            mtl_out = os.path.join(dst_dir, os.path.basename(mtl_src))
            if os.path.normcase(mtl_out) != os.path.normcase(mtl_src):
                shutil.copy2(mtl_src, mtl_out)
            else:
                mtl_out = mtl_src
            exported.append(mtl_out)

        if texture_path and os.path.exists(texture_path):
            tex_out = os.path.join(dst_dir, os.path.basename(texture_path))
            if os.path.normcase(tex_out) != os.path.normcase(texture_path):
                shutil.copy2(texture_path, tex_out)
            else:
                tex_out = texture_path
            exported.append(tex_out)

        return obj_out, exported

    # GLTF/GLB conversion via trimesh scene export
    import trimesh

    loaded = trimesh.load(obj_path, force="scene", process=False)
    scene = loaded if isinstance(loaded, trimesh.Scene) else trimesh.Scene(loaded)

    out_path = os.path.join(dst_dir, f"{stem}.{fmt}")
    if fmt == "glb":
        glb_data = scene.export(file_type="glb")
        with open(out_path, "wb") as f:
            f.write(glb_data)
        return out_path, [out_path]

    # gltf export may create multiple files (json + bin + textures)
    export_result = scene.export(file_type="gltf")
    exported_paths = []
    if isinstance(export_result, dict):
        for rel_name, blob in export_result.items():
            target = os.path.join(dst_dir, rel_name)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            mode = "wb"
            with open(target, mode) as f:
                f.write(blob)
            exported_paths.append(target)
        gltf_candidates = [p for p in exported_paths if p.lower().endswith(".gltf")]
        primary = gltf_candidates[0] if gltf_candidates else (exported_paths[0] if exported_paths else "")
        return primary, exported_paths

    # Some trimesh versions may return text JSON for gltf.
    with open(out_path, "w", encoding="utf-8") as f:
        if isinstance(export_result, bytes):
            f.write(export_result.decode("utf-8"))
        else:
            f.write(str(export_result))
    return out_path, [out_path]


# ── Segmentation Endpoints ───────────────────────────────────────────────────


@app.route("/segment", methods=["POST"])
def segment_object():
    """
    Segment an object in an image using SAM-style click points.

    Expects JSON:
        {
            "image_path": "C:/path/to/image.png",
            "positive_points": [[0.5, 0.3], [0.6, 0.4]],  // normalized (x, y)
            "negative_points": [[0.1, 0.9]]                // optional
        }

    Returns JSON with path to the cropped masked image.
    """
    data = request.get_json(silent=True)
    if not data or "image_path" not in data:
        return jsonify({"success": False, "error": "No image_path provided"}), 400

    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"success": False, "error": f"Image not found: {image_path}"}), 400

    positive_points = data.get("positive_points", [])
    negative_points = data.get("negative_points", [])

    if not positive_points:
        return jsonify({"success": False, "error": "At least one positive point required"}), 400

    try:
        # Convert to tuples
        pos = [(float(p[0]), float(p[1])) for p in positive_points]
        neg = [(float(p[0]), float(p[1])) for p in negative_points]

        print(f"[Server] Segmenting with {len(pos)} positive, {len(neg)} negative points")

        mask = sam_segmenter.segment_from_points(image_path, pos, neg)
        if mask is None or not mask.any():
            return jsonify({"success": False, "error": "Segmentation produced empty mask"}), 500

        # Create cropped masked image
        cropped, bbox = sam_segmenter.create_masked_image(image_path, mask)
        out_path = os.path.join(UPLOAD_DIR, f"seg_{uuid.uuid4().hex[:8]}.png")
        from PIL import Image as PILImage
        PILImage.fromarray(cropped, mode="RGBA").save(out_path)

        pixel_count = int(mask.sum())
        total_pixels = int(mask.size)

        return jsonify({
            "success": True,
            "masked_image_path": out_path,
            "mask_pixel_count": pixel_count,
            "total_pixels": total_pixels,
            "coverage_percent": round(pixel_count / max(1, total_pixels) * 100, 1),
            "bbox": list(bbox),
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


@app.route("/segment_preview", methods=["POST"])
def segment_preview():
    """
    Generate a preview visualization of segmentation mask overlaid on image.

    Expects JSON same as /segment.
    Returns JSON with path to the preview PNG.
    """
    data = request.get_json(silent=True)
    if not data or "image_path" not in data:
        return jsonify({"success": False, "error": "No image_path provided"}), 400

    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"success": False, "error": f"Image not found: {image_path}"}), 400

    positive_points = data.get("positive_points", [])
    negative_points = data.get("negative_points", [])

    if not positive_points:
        return jsonify({"success": False, "error": "At least one positive point required"}), 400

    try:
        pos = [(float(p[0]), float(p[1])) for p in positive_points]
        neg = [(float(p[0]), float(p[1])) for p in negative_points]

        mask = sam_segmenter.segment_from_points(image_path, pos, neg)
        if mask is None or not mask.any():
            return jsonify({"success": False, "error": "Segmentation produced empty mask"}), 500

        # Create overlay visualization
        overlay = sam_segmenter.create_mask_overlay(image_path, mask, pos, neg)
        out_path = os.path.join(UPLOAD_DIR, f"preview_{uuid.uuid4().hex[:8]}.png")
        from PIL import Image as PILImage
        PILImage.fromarray(overlay, mode="RGBA").save(out_path)

        return jsonify({
            "success": True,
            "preview_path": out_path,
            "mask_pixel_count": int(mask.sum()),
        })

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


# ── Endpoints ────────────────────────────────────────────────────────────────


@app.route("/health", methods=["GET"])
def health():
    """Health check — returns server status and capabilities."""
    api_key = _load_api_key()
    return jsonify({
        "status": "ok",
        "version": "1.0.0",
        "engine": "GameVoid",
        "capabilities": {
            "analyze": bool(api_key),
            "generate_triposr": True,
            "generate_midas": True,
        },
        "output_dir": OUTPUT_DIR,
        "api_key_configured": bool(api_key),
    })


@app.route("/analyze", methods=["POST"])
def analyze_image():
    """
    Analyze an uploaded image using Gemini Vision API.

    Expects: multipart form with 'image' file
    Returns: JSON with object type, category, description
    """
    api_key = _load_api_key()
    if not api_key:
        return jsonify({
            "success": False,
            "error": "No Gemini API key configured. Set it in gamevoid_config.ini or GEMINI_API_KEY env var.",
        }), 400

    if "image" not in request.files:
        return jsonify({"success": False, "error": "No image file provided"}), 400

    file = request.files["image"]
    if file.filename == "":
        return jsonify({"success": False, "error": "Empty filename"}), 400

    # Save uploaded file
    ext = Path(file.filename).suffix or ".png"
    temp_path = os.path.join(UPLOAD_DIR, f"analyze_{uuid.uuid4().hex[:8]}{ext}")
    file.save(temp_path)

    try:
        analyzer = ImageAnalyzer(api_key)
        result = analyzer.analyze(temp_path)

        if "error" in result and result.get("object") is None:
            return jsonify({"success": False, "error": result["error"]}), 500

        return jsonify({
            "success": True,
            "object": result.get("object", "unknown_object"),
            "category": result.get("category", "unknown"),
            "description": result.get("description", ""),
        })

    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

    finally:
        # Cleanup temp file
        if os.path.exists(temp_path):
            try:
                os.remove(temp_path)
            except OSError:
                pass


@app.route("/analyze_from_path", methods=["POST"])
def analyze_from_path():
    """
    Analyze a local image path using Gemini Vision API.
    Expects JSON: {"image_path": "C:/path/to/image.png"}
    """
    data = request.get_json(silent=True)
    if not data or "image_path" not in data:
        return jsonify({"success": False, "error": "No image_path provided"}), 400
        
    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"success": False, "error": f"Image not found: {image_path}"}), 400
        
    api_key = _load_api_key()
    if not api_key:
        return jsonify({"success": False, "error": "No Gemini API key configured"}), 400

    try:
        analyzer = ImageAnalyzer(api_key)
        result = analyzer.analyze(image_path)
        
        if "error" in result and result.get("object") is None:
            return jsonify({"success": False, "error": result["error"]}), 500

        return jsonify({
            "success": True,
            "object": result.get("object", "unknown_object"),
            "category": result.get("category", "unknown"),
            "description": result.get("description", "")
        })
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500


@app.route("/generate", methods=["POST"])
def generate_model():
    """
    Generate a 3D model from an uploaded image.

    Expects: multipart form with:
        - 'image': image file
        - 'method': 'auto', 'triposr', or 'midas' (optional, default 'auto')
        - 'name': object name (optional, auto-detected if missing)

    Returns: JSON with file paths to generated OBJ + texture
    """
    if "image" not in request.files:
        return jsonify({"success": False, "error": "No image file provided"}), 400

    file = request.files["image"]
    if file.filename == "":
        return jsonify({"success": False, "error": "Empty filename"}), 400

    method = request.form.get("method", "auto")
    object_name = request.form.get("name", "")

    # Save uploaded file
    ext = Path(file.filename).suffix or ".png"
    temp_path = os.path.join(UPLOAD_DIR, f"gen_{uuid.uuid4().hex[:8]}{ext}")
    file.save(temp_path)

    try:
        # Step 1: Auto-detect object name if not provided
        if not object_name:
            api_key = _load_api_key()
            if api_key:
                try:
                    analyzer = ImageAnalyzer(api_key)
                    analysis = analyzer.analyze(temp_path)
                    object_name = analysis.get("object", "generated_model")
                    print(f"[Server] Detected object: {object_name} ({analysis.get('category', 'unknown')})")
                except Exception as e:
                    print(f"[Server] Analysis failed, using default name: {e}")
                    object_name = "generated_model"
            else:
                object_name = "generated_model"

        # Add timestamp to prevent overwrites
        timestamp = int(time.time())
        unique_name = f"{object_name}_{timestamp}"

        # Create output subdirectory
        model_output_dir = os.path.join(OUTPUT_DIR, unique_name)
        os.makedirs(model_output_dir, exist_ok=True)

        # Step 2: Generate 3D model
        print(f"[Server] Generating 3D model (method={method}) for '{object_name}'...")

        if method == "midas":
            result = mesh_generator_midas.generate_mesh_midas(
                temp_path, model_output_dir, unique_name
            )
        elif method == "triposr":
            result = mesh_generator.generate_mesh(
                temp_path, model_output_dir, unique_name
            )
        else:
            # Auto: try TripoSR first, fall back to MiDaS
            print("[Server] Auto mode: trying TripoSR first...")
            result = mesh_generator.generate_mesh(
                temp_path, model_output_dir, unique_name
            )

            if not result["success"]:
                print(f"[Server] TripoSR failed ({result.get('error')}), falling back to MiDaS...")
                result = mesh_generator_midas.generate_mesh_midas(
                    temp_path, model_output_dir, unique_name
                )

        if result["success"]:
            return jsonify({
                "success": True,
                "object_name": object_name,
                "obj_path": result["obj_path"],
                "texture_path": result["texture_path"],
                "vertex_count": result["vertex_count"],
                "face_count": result["face_count"],
                "generation_time": result.get("generation_time", 0),
                "method": result.get("method", "triposr"),
            })
        else:
            return jsonify({
                "success": False,
                "error": result.get("error", "Unknown error during generation"),
            }), 500

    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500

    finally:
        if os.path.exists(temp_path):
            try:
                os.remove(temp_path)
            except OSError:
                pass


@app.route("/generate_from_path", methods=["POST"])
def generate_from_path():
    """
    Generate a 3D model from a local file path (no upload needed).
    Used by the C++ engine when the image is already on disk.

    Expects JSON body:
        {
            "image_path": "C:/path/to/image.png",
            "method": "auto",
            "name": "chair",
            "mask_image_path": "C:/path/to/masked.png",        // optional
            "positive_points": [[0.5, 0.3], [0.6, 0.4]],         // optional
            "negative_points": [[0.1, 0.9]],                     // optional
            "use_selection": true,                               // optional legacy crop
            "selection_min_x": 0.1,
            "selection_min_y": 0.2,
            "selection_max_x": 0.8,
            "selection_max_y": 0.9,
            "use_smart_point": true,                             // optional legacy point
            "smart_point_x": 0.45,
            "smart_point_y": 0.56
        }
    """
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"success": False, "error": "No JSON body provided"}), 400

    image_path = data.get("image_path", "")
    if not image_path or not os.path.exists(image_path):
        return jsonify({"success": False, "error": f"Image not found: {image_path}"}), 400

    method = data.get("method", "auto")
    object_name = data.get("name", "")

    # Auto-detect object name
    if not object_name:
        api_key = _load_api_key()
        if api_key:
            try:
                analyzer = ImageAnalyzer(api_key)
                analysis = analyzer.analyze(image_path)
                object_name = analysis.get("object", "generated_model")
            except Exception:
                object_name = Path(image_path).stem
        else:
            object_name = Path(image_path).stem

    timestamp = int(time.time())
    unique_name = f"{object_name}_{timestamp}"
    model_output_dir = os.path.join(OUTPUT_DIR, unique_name)
    os.makedirs(model_output_dir, exist_ok=True)

    work_image_path = _prepare_work_image(image_path, data)
    print(f"[Server] Generating from path: {work_image_path} -> {model_output_dir}")

    try:
        if method == "midas":
            result = mesh_generator_midas.generate_mesh_midas(work_image_path, model_output_dir, unique_name)
        elif method == "triposr":
            result = mesh_generator.generate_mesh(work_image_path, model_output_dir, unique_name)
        else:
            result = mesh_generator.generate_mesh(work_image_path, model_output_dir, unique_name)
            if not result["success"]:
                result = mesh_generator_midas.generate_mesh_midas(
                    work_image_path, model_output_dir, unique_name
                )
    finally:
        # Only cleanup temporary files created in ai_server/uploads.
        normalized_upload = os.path.normpath(UPLOAD_DIR)
        normalized_work = os.path.normpath(work_image_path)
        if (
            work_image_path != image_path
            and os.path.exists(work_image_path)
            and normalized_work.startswith(normalized_upload)
        ):
            try:
                os.remove(work_image_path)
            except OSError:
                pass

    if result["success"]:
        return jsonify({
            "success": True,
            "object_name": object_name,
            "obj_path": result["obj_path"],
            "texture_path": result["texture_path"],
            "vertex_count": result["vertex_count"],
            "face_count": result["face_count"],
            "generation_time": result.get("generation_time", 0),
            "method": result.get("method", "auto"),
        })
    else:
        return jsonify({
            "success": False,
            "error": result.get("error", "Generation failed"),
        }), 500


@app.route("/export_model", methods=["POST"])
def export_model():
    """Export a generated OBJ model into obj/gltf/glb format."""
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"success": False, "error": "No JSON body provided"}), 400

    obj_path = data.get("obj_path", "")
    texture_path = data.get("texture_path", "")
    export_format = data.get("format", "obj")
    output_dir = data.get("output_dir", "")

    if not obj_path:
        return jsonify({"success": False, "error": "obj_path is required"}), 400

    try:
        primary_path, exported_paths = _export_model_file(obj_path, texture_path, export_format, output_dir)
        return jsonify({
            "success": True,
            "format": export_format.lower(),
            "export_path": primary_path,
            "files": exported_paths,
        })
    except Exception as e:
        import traceback
        traceback.print_exc()
        return jsonify({"success": False, "error": str(e)}), 500


# ── Main ─────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="GameVoid AI Model Server")
    parser.add_argument("--port", type=int, default=5000, help="Server port (default: 5000)")
    parser.add_argument("--host", type=str, default="127.0.0.1", help="Server host")
    parser.add_argument("--debug", action="store_true", help="Enable debug mode")
    args = parser.parse_args()

    print("=" * 60)
    print("  GameVoid AI Model Server")
    print("=" * 60)
    print(f"  Host:       {args.host}:{args.port}")
    print(f"  Output dir: {OUTPUT_DIR}")
    print(f"  API key:    {'configured' if _load_api_key() else 'NOT SET'}")
    print("=" * 60)

    app.run(host=args.host, port=args.port, debug=args.debug)
