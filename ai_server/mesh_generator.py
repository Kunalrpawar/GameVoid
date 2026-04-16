"""
GameVoid Engine — 3D Mesh Generator (TripoSR + Enhanced Depth Fallback)
========================================================================
Uses TripoSR for full 360° mesh generation, with a significantly improved
depth-based fallback that creates thick, volumetric meshes instead of flat reliefs.
"""

import os
import sys
import time
import numpy as np
from pathlib import Path
from PIL import Image, ImageFilter

# Global model cache
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
        print("[MeshGenerator] TripoSR not installed. Using enhanced depth fallback.")
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
    square_img = square_img.resize((size, size), Image.Resampling.LANCZOS)

    return square_img


def generate_mesh(image_path: str, output_dir: str, object_name: str = "model") -> dict:
    """Generate a 3D mesh from a single image using TripoSR or enhanced depth fallback."""
    result = {
        "success": False,
        "obj_path": "",
        "texture_path": "",
        "vertex_count": 0,
        "face_count": 0,
        "error": "",
        "generation_time": 0,
        "method": "triposr",
    }

    try:
        import torch
        os.makedirs(output_dir, exist_ok=True)
        start_time = time.time()

        print(f"[MeshGenerator] Preprocessing image: {image_path}")
        processed_img = _preprocess_image(image_path)

        # Save texture from the processed image so texture aligns with segmentation/crop.
        texture_path = os.path.join(output_dir, f"{object_name}_texture.png")
        processed_img.convert("RGB").save(texture_path)

        # Try TripoSR
        model = _load_model()
        if model is not None:
            mesh = _generate_triposr(model, processed_img)
            result["method"] = "triposr"
        else:
            print("[MeshGenerator] Using enhanced depth-based generation")
            mesh = _generate_depth_enhanced(image_path)
            result["method"] = "depth_enhanced"

        if mesh is None:
            result["error"] = "Mesh generation failed"
            return result

        obj_path = os.path.join(output_dir, f"{object_name}.obj")
        mtl_path = os.path.join(output_dir, f"{object_name}.mtl")
        _export_obj_with_material(mesh, obj_path, mtl_path, object_name, texture_path)

        result["success"] = True
        result["obj_path"] = obj_path
        result["texture_path"] = texture_path
        result["vertex_count"] = len(mesh["vertices"])
        result["face_count"] = len(mesh["faces"])
        result["generation_time"] = time.time() - start_time

        print(f"[MeshGenerator] Done! {result['vertex_count']} verts, "
              f"{result['face_count']} faces, {result['generation_time']:.1f}s")

    except Exception as e:
        result["error"] = str(e)
        print(f"[MeshGenerator] Error: {e}")
        import traceback
        traceback.print_exc()

    return result


def _generate_triposr(model, processed_img: Image.Image) -> dict:
    """Run TripoSR inference."""
    import torch

    device = _get_device()
    try:
        with torch.no_grad():
            scene_codes = model([processed_img], device=device)
            meshes = model.extract_mesh(scene_codes, resolution=256)

        if not meshes or len(meshes) == 0:
            return None

        mesh = meshes[0]
        vertices = np.array(mesh.vertices, dtype=np.float32)
        center = (vertices.max(axis=0) + vertices.min(axis=0)) / 2
        vertices -= center
        scale = np.abs(vertices).max()
        if scale > 0:
            vertices /= scale

        faces = np.array(mesh.faces, dtype=np.int32)
        uvs = None
        if hasattr(mesh, "visual") and hasattr(mesh.visual, "uv") and mesh.visual.uv is not None:
            uvs = np.array(mesh.visual.uv, dtype=np.float32)
        else:
            uvs = _generate_spherical_uvs(vertices)
        normals = _compute_normals(vertices, faces)

        return {"vertices": vertices, "faces": faces, "normals": normals, "uvs": uvs}

    except Exception as e:
        print(f"[MeshGenerator] TripoSR inference failed: {e}")
        import traceback
        traceback.print_exc()
        return None


def _bilateral_filter_depth(depth: np.ndarray, d: int = 5, sigma_color: float = 0.1,
                            sigma_space: float = 5.0) -> np.ndarray:
    """Apply bilateral filter to depth map for edge-preserving smoothing."""
    try:
        import cv2
        depth_u8 = (depth * 255).astype(np.uint8)
        filtered = cv2.bilateralFilter(depth_u8, d, sigma_color * 255, sigma_space)
        return filtered.astype(np.float32) / 255.0
    except ImportError:
        # Fallback: simple gaussian smoothing
        from PIL import ImageFilter
        depth_img = Image.fromarray((depth * 255).astype(np.uint8))
        depth_img = depth_img.filter(ImageFilter.GaussianBlur(radius=1.5))
        return np.array(depth_img, dtype=np.float32) / 255.0


def _laplacian_smooth(vertices: np.ndarray, faces: np.ndarray, iterations: int = 3,
                      weight: float = 0.3) -> np.ndarray:
    """Apply Laplacian smoothing to mesh vertices."""
    n_verts = len(vertices)
    # Build adjacency
    adjacency = [set() for _ in range(n_verts)]
    for face in faces:
        for i in range(len(face)):
            for j in range(len(face)):
                if i != j:
                    adjacency[face[i]].add(face[j])

    smoothed = vertices.copy()
    for _ in range(iterations):
        new_verts = smoothed.copy()
        for i in range(n_verts):
            if not adjacency[i]:
                continue
            neighbors = list(adjacency[i])
            avg = np.mean(smoothed[neighbors], axis=0)
            new_verts[i] = smoothed[i] * (1 - weight) + avg * weight
        smoothed = new_verts

    return smoothed


def _generate_depth_enhanced(image_path: str) -> dict:
    """
    Enhanced depth-based mesh generation that creates a THICK, VOLUMETRIC 3D object.
    
    Key improvements over the old method:
    - Higher resolution grid (128×128)
    - Bilateral-filtered depth for smooth surfaces
    - Thicker extrusion with proper back face
    - Edge silhouette creates actual 3D volume
    - Laplacian smoothing for nicer mesh
    - Object mask (via rembg) to only generate geometry for the actual object
    """
    print("[MeshGenerator] Enhanced depth mesh generation starting")

    # Load and process image
    img = Image.open(image_path).convert("RGBA")

    # Try to get mask via rembg for better object isolation
    has_mask = False
    try:
        from rembg import remove
        img_masked = remove(img)
        alpha = np.array(img_masked)[:, :, 3]
        has_mask = alpha.max() > 128
        if has_mask:
            print("[MeshGenerator] Object mask obtained via rembg")
    except Exception:
        alpha = None

    # Convert to grayscale for depth estimation
    img_rgb = img.convert("RGB")
    grid_size = 128  # Higher resolution grid

    # Simple depth estimation from grayscale + edge detection
    img_resized = img_rgb.resize((grid_size, grid_size), Image.Resampling.LANCZOS)
    gray = np.array(img_resized.convert("L"), dtype=np.float32) / 255.0

    # Use edges to create depth variation (objects with more detail = closer)
    from PIL import ImageFilter
    edges_img = img_resized.convert("L").filter(ImageFilter.FIND_EDGES)
    edges = np.array(edges_img, dtype=np.float32) / 255.0

    # Combine: luminance + edge-based depth
    depth = gray * 0.6 + (1.0 - edges) * 0.2 + 0.2
    depth = _bilateral_filter_depth(depth)

    # Resize mask to grid
    if has_mask:
        mask_img = Image.fromarray(alpha).resize((grid_size, grid_size), Image.Resampling.LANCZOS)
        mask = np.array(mask_img, dtype=np.float32) / 255.0 > 0.3
    else:
        mask = np.ones((grid_size, grid_size), dtype=bool)

    h, w = grid_size, grid_size

    # ── Build thick 3D mesh: front face + back face + side walls ──

    # Extrusion parameters
    front_depth = 0.35    # How far the front face protrudes
    back_depth = -0.15    # How far back the back face goes
    min_thickness = 0.08  # Minimum thickness to ensure it's not flat

    vertices = []
    uvs = []
    vert_map_front = np.full((h, w), -1, dtype=np.int32)
    vert_map_back = np.full((h, w), -1, dtype=np.int32)

    # Front face vertices
    for y in range(h):
        for x in range(w):
            if not mask[y, x]:
                continue
            px = (x / (w - 1)) - 0.5
            py = 0.5 - (y / (h - 1))
            pz = depth[y, x] * front_depth
            vert_map_front[y, x] = len(vertices)
            vertices.append([px, py, pz])
            uvs.append([x / (w - 1), 1 - y / (h - 1)])

    # Back face vertices (mirrored depth, pulled back)
    for y in range(h):
        for x in range(w):
            if not mask[y, x]:
                continue
            px = (x / (w - 1)) - 0.5
            py = 0.5 - (y / (h - 1))
            # Back face: negative depth to create volume
            back_z = back_depth - depth[y, x] * 0.1
            pz = min(depth[y, x] * front_depth - min_thickness, back_z)
            vert_map_back[y, x] = len(vertices)
            vertices.append([px, py, pz])
            uvs.append([x / (w - 1), 1 - y / (h - 1)])

    vertices = np.array(vertices, dtype=np.float32)
    uvs = np.array(uvs, dtype=np.float32)

    # ── Create faces ──
    faces = []

    # Front face triangles
    for y in range(h - 1):
        for x in range(w - 1):
            i00 = vert_map_front[y, x]
            i10 = vert_map_front[y, x + 1]
            i01 = vert_map_front[y + 1, x]
            i11 = vert_map_front[y + 1, x + 1]
            if i00 >= 0 and i10 >= 0 and i01 >= 0:
                faces.append([i00, i01, i10])
            if i10 >= 0 and i01 >= 0 and i11 >= 0:
                faces.append([i10, i01, i11])

    # Back face triangles (reversed winding)
    for y in range(h - 1):
        for x in range(w - 1):
            i00 = vert_map_back[y, x]
            i10 = vert_map_back[y, x + 1]
            i01 = vert_map_back[y + 1, x]
            i11 = vert_map_back[y + 1, x + 1]
            if i00 >= 0 and i10 >= 0 and i01 >= 0:
                faces.append([i00, i10, i01])
            if i10 >= 0 and i01 >= 0 and i11 >= 0:
                faces.append([i10, i11, i01])

    # ── Side walls: connect front and back edges ──
    # Find edge pixels (mask boundary)
    def is_edge(y, x):
        if not mask[y, x]:
            return False
        for dy, dx in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            ny, nx = y + dy, x + dx
            if ny < 0 or ny >= h or nx < 0 or nx >= w or not mask[ny, nx]:
                return True
        return False

    # Connect edge front/back vertices
    for y in range(h - 1):
        for x in range(w - 1):
            # Right edge
            if is_edge(y, x) and is_edge(y + 1, x):
                f0 = vert_map_front[y, x]
                f1 = vert_map_front[y + 1, x]
                b0 = vert_map_back[y, x]
                b1 = vert_map_back[y + 1, x]
                if f0 >= 0 and f1 >= 0 and b0 >= 0 and b1 >= 0:
                    faces.append([f0, f1, b0])
                    faces.append([f1, b1, b0])

            # Bottom edge
            if is_edge(y, x) and is_edge(y, x + 1):
                f0 = vert_map_front[y, x]
                f1 = vert_map_front[y, x + 1]
                b0 = vert_map_back[y, x]
                b1 = vert_map_back[y, x + 1]
                if f0 >= 0 and f1 >= 0 and b0 >= 0 and b1 >= 0:
                    faces.append([f0, b0, f1])
                    faces.append([f1, b0, b1])

    if not faces:
        print("[MeshGenerator] No faces generated")
        return None

    faces = np.array(faces, dtype=np.int32)

    # Apply Laplacian smoothing
    vertices = _laplacian_smooth(vertices, faces, iterations=2, weight=0.25)

    # Normalize to unit cube
    center = (vertices.max(axis=0) + vertices.min(axis=0)) / 2
    vertices -= center
    scale = np.abs(vertices).max()
    if scale > 0:
        vertices /= scale

    normals = _compute_normals(vertices, faces)

    print(f"[MeshGenerator] Enhanced depth mesh: {len(vertices)} verts, {len(faces)} faces")
    return {"vertices": vertices, "faces": faces, "normals": normals, "uvs": uvs}


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

    norms = np.linalg.norm(normals, axis=1, keepdims=True)
    norms = np.maximum(norms, 1e-8)
    normals /= norms
    return normals


def _generate_spherical_uvs(vertices: np.ndarray) -> np.ndarray:
    """Generate spherical UV mapping for better texture coverage on 3D meshes."""
    uvs = np.zeros((len(vertices), 2), dtype=np.float32)
    for i, v in enumerate(vertices):
        # Spherical projection
        r = np.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
        if r < 1e-8:
            uvs[i] = [0.5, 0.5]
            continue
        # Azimuthal angle (longitude) → U
        u = 0.5 + np.arctan2(v[0], v[2]) / (2 * np.pi)
        # Polar angle (latitude) → V
        v_coord = 0.5 + np.arcsin(np.clip(v[1] / r, -1, 1)) / np.pi
        uvs[i] = [u, v_coord]

    return np.clip(uvs, 0.0, 1.0)


def _export_obj_with_material(mesh: dict, obj_path: str, mtl_path: str,
                               name: str, texture_path: str):
    """Export mesh to OBJ format with MTL material file."""
    vertices = mesh["vertices"]
    normals = mesh["normals"]
    uvs = mesh["uvs"]
    faces = mesh["faces"]

    texture_filename = os.path.basename(texture_path)
    with open(mtl_path, "w") as f:
        f.write(f"# GameVoid AI-Generated Material\n")
        f.write(f"newmtl {name}_material\n")
        f.write(f"Ka 0.2 0.2 0.2\n")
        f.write(f"Kd 0.8 0.8 0.8\n")
        f.write(f"Ks 0.1 0.1 0.1\n")
        f.write(f"Ns 32.0\n")
        f.write(f"d 1.0\n")
        f.write(f"illum 2\n")
        f.write(f"map_Kd {texture_filename}\n")

    mtl_filename = os.path.basename(mtl_path)
    with open(obj_path, "w") as f:
        f.write(f"# GameVoid AI-Generated 3D Model\n")
        f.write(f"# Object: {name}\n")
        f.write(f"# Vertices: {len(vertices)}, Faces: {len(faces)}\n")
        f.write(f"mtllib {mtl_filename}\n")
        f.write(f"usemtl {name}_material\n\n")

        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
        f.write("\n")
        for uv in uvs:
            f.write(f"vt {uv[0]:.6f} {uv[1]:.6f}\n")
        f.write("\n")
        for n in normals:
            f.write(f"vn {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}\n")
        f.write("\n")
        for face in faces:
            i0, i1, i2 = face[0] + 1, face[1] + 1, face[2] + 1
            f.write(f"f {i0}/{i0}/{i0} {i1}/{i1}/{i1} {i2}/{i2}/{i2}\n")

    print(f"[MeshGenerator] Exported: {obj_path} ({len(vertices)} verts, {len(faces)} faces)")
