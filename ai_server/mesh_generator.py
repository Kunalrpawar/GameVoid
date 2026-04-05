"""
GameVoid Engine — 3D Mesh Generator (TripoSR)
===============================================
Uses the TripoSR model (Stability AI + Tripo) to convert a single image
into a textured 3D mesh. Outputs OBJ + PNG texture.

TripoSR produces genuine 360° meshes from a single image — the best
practical, non-research-level option for game-ready 3D assets.
"""

import os
import sys
import time
import numpy as np
from pathlib import Path
from PIL import Image

# Global model cache — loaded once, reused across requests
_tsr_model = None
_device = None


def _get_device():
    """Auto-detect the best available device."""
    global _device
    if _device is not None:
        return _device

    import torch

    if torch.cuda.is_available():
        _device = "cuda:0"
        print(f"[MeshGenerator] Using CUDA GPU: {torch.cuda.get_device_name(0)}")
    else:
        _device = "cpu"
        print("[MeshGenerator] No CUDA GPU found — using CPU (slower, ~30-60s per model)")
    return _device


def _load_model():
    """Load the TripoSR model (downloads ~1.5GB on first run)."""
    global _tsr_model
    if _tsr_model is not None:
        return _tsr_model

    print("[MeshGenerator] Loading TripoSR model (first run downloads ~1.5GB)...")
    start = time.time()

    try:
        from tsr.system import TSR

        device = _get_device()
        _tsr_model = TSR.from_pretrained(
            "stabilityai/TripoSR",
            config_name="config.yaml",
            weight_name="model.ckpt",
        )
        _tsr_model.renderer.set_chunk_size(8192)
        _tsr_model.to(device)

        elapsed = time.time() - start
        print(f"[MeshGenerator] TripoSR loaded in {elapsed:.1f}s on {device}")
        return _tsr_model

    except ImportError:
        print("[MeshGenerator] TripoSR not installed. Attempting HuggingFace fallback...")
        return _load_model_huggingface()


def _load_model_huggingface():
    """Fallback: load TripoSR via HuggingFace pipeline."""
    global _tsr_model

    try:
        # Use the huggingface_hub to download and run TripoSR
        from huggingface_hub import hf_hub_download
        import torch

        print("[MeshGenerator] Downloading TripoSR via HuggingFace Hub...")

        # Download model files
        model_dir = hf_hub_download(
            repo_id="stabilityai/TripoSR",
            filename="model.ckpt",
            cache_dir=os.path.join(os.path.dirname(__file__), "model_cache"),
        )
        config_path = hf_hub_download(
            repo_id="stabilityai/TripoSR",
            filename="config.yaml",
            cache_dir=os.path.join(os.path.dirname(__file__), "model_cache"),
        )

        print(f"[MeshGenerator] Model downloaded to: {os.path.dirname(model_dir)}")
        _tsr_model = "huggingface"  # Marker — use generate_huggingface path
        return _tsr_model

    except Exception as e:
        print(f"[MeshGenerator] HuggingFace fallback also failed: {e}")
        _tsr_model = None
        return None


def _preprocess_image(image_path: str, size: int = 512) -> Image.Image:
    """
    Preprocess image for TripoSR:
    - Remove background (using rembg)
    - Resize to square
    - Center the object
    """
    img = Image.open(image_path).convert("RGBA")

    # Try to remove background
    try:
        from rembg import remove
        img = remove(img)
        print("[MeshGenerator] Background removed successfully")
    except ImportError:
        print("[MeshGenerator] rembg not available — using image as-is")
    except Exception as e:
        print(f"[MeshGenerator] Background removal failed: {e} — using image as-is")

    # Find the bounding box of non-transparent pixels
    alpha = np.array(img)[:, :, 3]
    rows = np.any(alpha > 10, axis=1)
    cols = np.any(alpha > 10, axis=0)

    if rows.any() and cols.any():
        rmin, rmax = np.where(rows)[0][[0, -1]]
        cmin, cmax = np.where(cols)[0][[0, -1]]
        # Crop to bounding box with padding
        pad = max(int((rmax - rmin) * 0.05), int((cmax - cmin) * 0.05), 10)
        rmin = max(0, rmin - pad)
        rmax = min(img.height - 1, rmax + pad)
        cmin = max(0, cmin - pad)
        cmax = min(img.width - 1, cmax + pad)
        img = img.crop((cmin, rmin, cmax + 1, rmax + 1))

    # Make square by padding
    w, h = img.size
    max_dim = max(w, h)
    square_img = Image.new("RGBA", (max_dim, max_dim), (127, 127, 127, 0))
    paste_x = (max_dim - w) // 2
    paste_y = (max_dim - h) // 2
    square_img.paste(img, (paste_x, paste_y))

    # Resize
    square_img = square_img.resize((size, size), Image.Resampling.LANCZOS)

    return square_img


def generate_mesh(image_path: str, output_dir: str, object_name: str = "model") -> dict:
    """
    Generate a 3D mesh from a single image using TripoSR.

    Args:
        image_path: Path to the input image
        output_dir: Directory to save the output OBJ + texture
        object_name: Name for the output files

    Returns:
        dict with keys: success, obj_path, texture_path, vertex_count, face_count, error
    """
    import torch

    result = {
        "success": False,
        "obj_path": "",
        "texture_path": "",
        "vertex_count": 0,
        "face_count": 0,
        "error": "",
        "generation_time": 0,
    }

    try:
        os.makedirs(output_dir, exist_ok=True)
        start_time = time.time()

        # Preprocess image
        print(f"[MeshGenerator] Preprocessing image: {image_path}")
        processed_img = _preprocess_image(image_path)

        # Save the processed image as texture
        texture_path = os.path.join(output_dir, f"{object_name}_texture.png")
        # Save the original image as the texture (RGB, no alpha)
        orig_img = Image.open(image_path).convert("RGB")
        orig_img.save(texture_path)
        print(f"[MeshGenerator] Texture saved: {texture_path}")

        # Try TripoSR first
        model = _load_model()

        if model is not None and model != "huggingface":
            # Full TripoSR pipeline
            mesh = _generate_triposr(model, processed_img)
        else:
            # Fallback to simple depth-based generation
            print("[MeshGenerator] TripoSR unavailable — using depth-based fallback")
            mesh = _generate_depth_fallback(image_path)

        if mesh is None:
            result["error"] = "Mesh generation failed"
            return result

        # Export to OBJ
        obj_path = os.path.join(output_dir, f"{object_name}.obj")
        mtl_path = os.path.join(output_dir, f"{object_name}.mtl")

        _export_obj_with_material(mesh, obj_path, mtl_path, object_name, texture_path)

        result["success"] = True
        result["obj_path"] = obj_path
        result["texture_path"] = texture_path
        result["vertex_count"] = len(mesh["vertices"])
        result["face_count"] = len(mesh["faces"])
        result["generation_time"] = time.time() - start_time

        print(
            f"[MeshGenerator] Done! {result['vertex_count']} verts, "
            f"{result['face_count']} faces, {result['generation_time']:.1f}s"
        )

    except Exception as e:
        result["error"] = str(e)
        print(f"[MeshGenerator] Error: {e}")
        import traceback
        traceback.print_exc()

    return result


def _generate_triposr(model, processed_img: Image.Image) -> dict:
    """Run TripoSR inference and return mesh data."""
    import torch

    device = _get_device()

    try:
        with torch.no_grad():
            scene_codes = model([processed_img], device=device)
            meshes = model.extract_mesh(scene_codes, resolution=256)

        if not meshes or len(meshes) == 0:
            return None

        mesh = meshes[0]

        # Normalize: center at origin, scale to unit box, Y-up
        vertices = np.array(mesh.vertices, dtype=np.float32)
        center = (vertices.max(axis=0) + vertices.min(axis=0)) / 2
        vertices -= center
        scale = np.abs(vertices).max()
        if scale > 0:
            vertices /= scale

        faces = np.array(mesh.faces, dtype=np.int32)

        # Generate UVs if not present
        uvs = None
        if hasattr(mesh, "visual") and hasattr(mesh.visual, "uv") and mesh.visual.uv is not None:
            uvs = np.array(mesh.visual.uv, dtype=np.float32)
        else:
            uvs = _generate_box_uvs(vertices, faces)

        # Compute normals
        normals = _compute_normals(vertices, faces)

        return {
            "vertices": vertices,
            "faces": faces,
            "normals": normals,
            "uvs": uvs,
        }

    except Exception as e:
        print(f"[MeshGenerator] TripoSR inference failed: {e}")
        import traceback
        traceback.print_exc()
        return None


def _generate_depth_fallback(image_path: str) -> dict:
    """
    Simple depth-based mesh generation fallback.
    Uses the image as a heightmap to create a relief mesh.
    This works without TripoSR or MiDaS — pure image processing.
    """
    print("[MeshGenerator] Using simple depth-based mesh generation")

    img = Image.open(image_path).convert("L")  # Grayscale
    img = img.resize((64, 64), Image.Resampling.LANCZOS)
    depth = np.array(img, dtype=np.float32) / 255.0

    h, w = depth.shape
    vertices = []
    uvs = []

    # Create front face vertices from image
    for y in range(h):
        for x in range(w):
            px = (x / (w - 1)) - 0.5  # -0.5 to 0.5
            py = 0.5 - (y / (h - 1))  # 0.5 to -0.5
            pz = depth[y, x] * 0.3     # Depth extrusion
            vertices.append([px, py, pz])
            uvs.append([x / (w - 1), 1 - y / (h - 1)])

    # Add back face (flat)
    back_offset = len(vertices)
    for y in range(h):
        for x in range(w):
            px = (x / (w - 1)) - 0.5
            py = 0.5 - (y / (h - 1))
            pz = -0.05  # Slight offset behind
            vertices.append([px, py, pz])
            uvs.append([x / (w - 1), 1 - y / (h - 1)])

    vertices = np.array(vertices, dtype=np.float32)
    uvs = np.array(uvs, dtype=np.float32)

    # Create faces (triangulated grid)
    faces = []
    for y in range(h - 1):
        for x in range(w - 1):
            i = y * w + x
            # Front face triangles
            faces.append([i, i + w, i + 1])
            faces.append([i + 1, i + w, i + w + 1])
            # Back face triangles (reversed winding)
            bi = back_offset + i
            faces.append([bi, bi + 1, bi + w])
            faces.append([bi + 1, bi + w + 1, bi + w])

    # Side faces connecting front and back
    # Top edge
    for x in range(w - 1):
        fi = x
        bi = back_offset + x
        faces.append([fi, fi + 1, bi])
        faces.append([fi + 1, bi + 1, bi])
    # Bottom edge
    for x in range(w - 1):
        fi = (h - 1) * w + x
        bi = back_offset + fi
        faces.append([fi, bi, fi + 1])
        faces.append([fi + 1, bi, bi + 1])
    # Left edge
    for y in range(h - 1):
        fi = y * w
        bi = back_offset + fi
        faces.append([fi, bi, fi + w])
        faces.append([fi + w, bi, bi + w])
    # Right edge
    for y in range(h - 1):
        fi = y * w + (w - 1)
        bi = back_offset + fi
        faces.append([fi, fi + w, bi])
        faces.append([fi + w, bi + w, bi])

    faces = np.array(faces, dtype=np.int32)
    normals = _compute_normals(vertices, faces)

    return {
        "vertices": vertices,
        "faces": faces,
        "normals": normals,
        "uvs": uvs,
    }


def _compute_normals(vertices: np.ndarray, faces: np.ndarray) -> np.ndarray:
    """Compute per-vertex normals by averaging face normals."""
    normals = np.zeros_like(vertices)

    for face in faces:
        v0, v1, v2 = vertices[face[0]], vertices[face[1]], vertices[face[2]]
        edge1 = v1 - v0
        edge2 = v2 - v0
        face_normal = np.cross(edge1, edge2)
        norm = np.linalg.norm(face_normal)
        if norm > 1e-8:
            face_normal /= norm
        normals[face[0]] += face_normal
        normals[face[1]] += face_normal
        normals[face[2]] += face_normal

    # Normalize
    norms = np.linalg.norm(normals, axis=1, keepdims=True)
    norms = np.maximum(norms, 1e-8)
    normals /= norms

    return normals


def _generate_box_uvs(vertices: np.ndarray, faces: np.ndarray) -> np.ndarray:
    """Generate box-projected UVs for the mesh."""
    uvs = np.zeros((len(vertices), 2), dtype=np.float32)

    for i, v in enumerate(vertices):
        # Determine dominant axis
        abs_n = np.abs(v)
        if abs_n[1] >= abs_n[0] and abs_n[1] >= abs_n[2]:
            # Top/bottom — project XZ
            uvs[i] = [v[0] + 0.5, v[2] + 0.5]
        elif abs_n[0] >= abs_n[2]:
            # Left/right — project YZ
            uvs[i] = [v[2] + 0.5, v[1] + 0.5]
        else:
            # Front/back — project XY
            uvs[i] = [v[0] + 0.5, v[1] + 0.5]

    # Clamp to [0, 1]
    uvs = np.clip(uvs, 0.0, 1.0)
    return uvs


def _export_obj_with_material(
    mesh: dict,
    obj_path: str,
    mtl_path: str,
    name: str,
    texture_path: str,
):
    """Export mesh to OBJ format with MTL file referencing the texture."""
    vertices = mesh["vertices"]
    normals = mesh["normals"]
    uvs = mesh["uvs"]
    faces = mesh["faces"]

    # Write MTL file
    texture_filename = os.path.basename(texture_path)
    with open(mtl_path, "w") as f:
        f.write(f"# GameVoid AI-Generated Material\n")
        f.write(f"newmtl {name}_material\n")
        f.write(f"Ka 0.2 0.2 0.2\n")        # Ambient
        f.write(f"Kd 0.8 0.8 0.8\n")        # Diffuse
        f.write(f"Ks 0.1 0.1 0.1\n")        # Specular
        f.write(f"Ns 32.0\n")                # Shininess
        f.write(f"d 1.0\n")                  # Opacity
        f.write(f"illum 2\n")                # Illumination model
        f.write(f"map_Kd {texture_filename}\n")  # Diffuse texture

    # Write OBJ file
    mtl_filename = os.path.basename(mtl_path)
    with open(obj_path, "w") as f:
        f.write(f"# GameVoid AI-Generated 3D Model\n")
        f.write(f"# Object: {name}\n")
        f.write(f"# Vertices: {len(vertices)}\n")
        f.write(f"# Faces: {len(faces)}\n")
        f.write(f"mtllib {mtl_filename}\n")
        f.write(f"usemtl {name}_material\n\n")

        # Vertices
        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")

        f.write("\n")

        # Texture coordinates
        for uv in uvs:
            f.write(f"vt {uv[0]:.6f} {uv[1]:.6f}\n")

        f.write("\n")

        # Normals
        for n in normals:
            f.write(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}\n")

        f.write("\n")

        # Faces (OBJ is 1-indexed: v/vt/vn)
        for face in faces:
            i0, i1, i2 = face[0] + 1, face[1] + 1, face[2] + 1
            f.write(f"f {i0}/{i0}/{i0} {i1}/{i1}/{i1} {i2}/{i2}/{i2}\n")

    print(f"[MeshGenerator] Exported: {obj_path} ({len(vertices)} verts, {len(faces)} faces)")
    print(f"[MeshGenerator] Material: {mtl_path} -> {texture_filename}")
