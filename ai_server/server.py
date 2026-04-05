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
from pathlib import Path

from flask import Flask, request, jsonify

# Add the ai_server directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from image_analyzer import ImageAnalyzer
import mesh_generator
import mesh_generator_midas

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
            "name": "chair"
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

    print(f"[Server] Generating from path: {image_path} -> {model_output_dir}")

    if method == "midas":
        result = mesh_generator_midas.generate_mesh_midas(image_path, model_output_dir, unique_name)
    elif method == "triposr":
        result = mesh_generator.generate_mesh(image_path, model_output_dir, unique_name)
    else:
        result = mesh_generator.generate_mesh(image_path, model_output_dir, unique_name)
        if not result["success"]:
            result = mesh_generator_midas.generate_mesh_midas(
                image_path, model_output_dir, unique_name
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
            "method": result.get("method", "auto"),
        })
    else:
        return jsonify({
            "success": False,
            "error": result.get("error", "Generation failed"),
        }), 500


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
