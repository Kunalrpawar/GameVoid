"""
GameVoid Engine — MiDaS Depth-Based Mesh Generator (Improved)
================================================================
Uses MiDaS for monocular depth estimation, then constructs a thick volumetric
mesh via Open3D. Produces genuine 3D meshes with proper thickness and detail.

Key improvements:
 - Higher resolution mesh (192)
 - Bilateral depth smoothing
 - Double-sided mesh with volume (front + back + sides)
 - Better UV mapping (cylindrical projection)
 - Poisson depth 9 for more geometric detail
 - Target 8000 faces for higher quality
"""

import os
import time
import numpy as np
from pathlib import Path
from PIL import Image, ImageFilter


def generate_mesh_midas(image_path: str, output_dir: str, object_name: str = "model") -> dict:
    """Generate a 3D mesh from a single image using MiDaS depth estimation."""
    result = {
        "success": False,
        "obj_path": "",
        "texture_path": "",
        "vertex_count": 0,
        "face_count": 0,
        "error": "",
        "generation_time": 0,
        "method": "midas",
    }

    try:
        import torch
        import open3d as o3d

        os.makedirs(output_dir, exist_ok=True)
        start_time = time.time()

        print(f"[MiDaS] Processing image: {image_path}")

        # Load image
        img = Image.open(image_path).convert("RGB")
        orig_w, orig_h = img.size

        # Save texture
        texture_path = os.path.join(output_dir, f"{object_name}_texture.png")
        img.save(texture_path)

        # ── Step 1: MiDaS Depth Estimation ──
        print("[MiDaS] Loading MiDaS model...")
        midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small", trust_repo=True)
        device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        midas.to(device)
        midas.eval()

        midas_transforms = torch.hub.load("intel-isl/MiDaS", "transforms", trust_repo=True)
        transform = midas_transforms.small_transform

        input_img = np.array(img)
        input_batch = transform(input_img).to(device)

        with torch.no_grad():
            prediction = midas(input_batch)
            prediction = torch.nn.functional.interpolate(
                prediction.unsqueeze(1),
                size=(384, 384),  # Higher resolution
                mode="bicubic",
                align_corners=False,
            ).squeeze()

        depth_map = prediction.cpu().numpy()

        # Normalize depth to [0, 1]
        depth_min = depth_map.min()
        depth_max = depth_map.max()
        if depth_max - depth_min > 1e-6:
            depth_map = (depth_map - depth_min) / (depth_max - depth_min)
        else:
            depth_map = np.zeros_like(depth_map)

        # Bilateral filtering for edge-preserving smoothing
        depth_map = _bilateral_filter(depth_map)

        print(f"[MiDaS] Depth map computed: {depth_map.shape}")

        # ── Step 2: Create volumetric mesh with front + back ──
        mesh_resolution = 192  # Increased from 128
        h, w = depth_map.shape

        if h != mesh_resolution or w != mesh_resolution:
            depth_img = Image.fromarray((depth_map * 255).astype(np.uint8))
            depth_img = depth_img.resize((mesh_resolution, mesh_resolution), Image.Resampling.LANCZOS)
            depth_map = np.array(depth_img, dtype=np.float32) / 255.0
            h, w = depth_map.shape

        # Try to get object mask for better results
        mask = _get_object_mask(image_path, h, w)

        color_img = img.resize((w, h), Image.Resampling.LANCZOS)
        colors = np.array(color_img, dtype=np.float32) / 255.0

        # Create BOTH front and back point clouds for volume
        front_points = []
        back_points = []
        all_colors = []

        depth_scale = 0.5   # Increased depth for more 3D effect
        back_offset = 0.15  # Distance behind the object

        for y in range(h):
            for x in range(w):
                if not mask[y, x]:
                    continue
                px = (x / (w - 1)) - 0.5
                py = 0.5 - (y / (h - 1))
                pz_front = depth_map[y, x] * depth_scale
                pz_back = -back_offset - depth_map[y, x] * 0.1  # Slight depth variation on back

                front_points.append([px, py, pz_front])
                back_points.append([px, py, pz_back])
                all_colors.append(colors[y, x])

        if not front_points:
            result["error"] = "No object pixels found for mesh generation"
            return result

        # Combine front and back points
        points = np.array(front_points + back_points, dtype=np.float64)
        point_colors = np.array(all_colors + all_colors, dtype=np.float64)  # Same color for both sides

        # ── Step 3: Open3D Mesh Construction ──
        print("[MiDaS] Constructing volumetric mesh via Open3D...")

        pcd = o3d.geometry.PointCloud()
        pcd.points = o3d.utility.Vector3dVector(points)
        pcd.colors = o3d.utility.Vector3dVector(point_colors)

        pcd.estimate_normals(
            search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=0.04, max_nn=40)
        )
        pcd.orient_normals_towards_camera_location(camera_location=[0, 0, 3])

        # Poisson reconstruction at higher depth
        mesh_o3d, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
            pcd, depth=9, width=0, scale=1.1, linear_fit=False
        )

        # Remove low-density vertices
        densities = np.asarray(densities)
        density_threshold = np.quantile(densities, 0.04)
        vertices_to_remove = densities < density_threshold
        mesh_o3d.remove_vertices_by_mask(vertices_to_remove)

        # Higher target face count for detail
        target_faces = 8000
        if len(mesh_o3d.triangles) > target_faces:
            mesh_o3d = mesh_o3d.simplify_quadric_decimation(target_faces)

        mesh_o3d.compute_vertex_normals()

        # ── Step 4: Post-process ──
        vertices = np.asarray(mesh_o3d.vertices, dtype=np.float32)
        faces = np.asarray(mesh_o3d.triangles, dtype=np.int32)

        # Center and normalize
        center = (vertices.max(axis=0) + vertices.min(axis=0)) / 2
        vertices -= center
        scale = np.abs(vertices).max()
        if scale > 0:
            vertices /= scale

        mesh_normals = np.asarray(mesh_o3d.vertex_normals, dtype=np.float32)
        if len(mesh_normals) != len(vertices):
            mesh_normals = _compute_normals(vertices, faces)

        # Cylindrical UV mapping (better for 3D objects than flat planar)
        uvs = _generate_cylindrical_uvs(vertices)

        # ── Step 5: Export ──
        obj_path = os.path.join(output_dir, f"{object_name}.obj")
        mtl_path = os.path.join(output_dir, f"{object_name}.mtl")
        _export_obj(vertices, faces, mesh_normals, uvs,
                    obj_path, mtl_path, object_name, texture_path)

        result["success"] = True
        result["obj_path"] = obj_path
        result["texture_path"] = texture_path
        result["vertex_count"] = len(vertices)
        result["face_count"] = len(faces)
        result["generation_time"] = time.time() - start_time

        print(f"[MiDaS] Done! {result['vertex_count']} verts, "
              f"{result['face_count']} faces, {result['generation_time']:.1f}s")

    except ImportError as e:
        result["error"] = f"Missing dependency: {e}. Install with: pip install torch open3d"
        print(f"[MiDaS] {result['error']}")

    except Exception as e:
        result["error"] = str(e)
        print(f"[MiDaS] Error: {e}")
        import traceback
        traceback.print_exc()

    return result


def _bilateral_filter(depth: np.ndarray) -> np.ndarray:
    """Edge-preserving bilateral filter on depth map."""
    try:
        import cv2
        depth_u8 = (depth * 255).astype(np.uint8)
        filtered = cv2.bilateralFilter(depth_u8, 7, 50, 50)
        return filtered.astype(np.float32) / 255.0
    except ImportError:
        depth_img = Image.fromarray((depth * 255).astype(np.uint8))
        depth_img = depth_img.filter(ImageFilter.GaussianBlur(radius=1.5))
        return np.array(depth_img, dtype=np.float32) / 255.0


def _get_object_mask(image_path: str, target_h: int, target_w: int) -> np.ndarray:
    """Get an object mask via rembg, or return all-True if unavailable."""
    try:
        from rembg import remove
        from PIL import Image as PILImage
        img = PILImage.open(image_path).convert("RGBA")
        masked = remove(img)
        alpha = np.array(masked)[:, :, 3]
        mask_img = PILImage.fromarray(alpha).resize((target_w, target_h), PILImage.Resampling.LANCZOS)
        return np.array(mask_img, dtype=np.float32) / 255.0 > 0.3
    except Exception:
        return np.ones((target_h, target_w), dtype=bool)


def _generate_cylindrical_uvs(vertices: np.ndarray) -> np.ndarray:
    """Cylindrical UV projection — wraps texture around the object."""
    uvs = np.zeros((len(vertices), 2), dtype=np.float32)
    for i, v in enumerate(vertices):
        # U from angle around Y axis
        u = 0.5 + np.arctan2(v[0], v[2]) / (2 * np.pi)
        # V from height
        v_coord = v[1] + 0.5  # Map [-0.5, 0.5] → [0, 1]
        uvs[i] = [u, np.clip(v_coord, 0.0, 1.0)]
    return np.clip(uvs, 0.0, 1.0)


def _compute_normals(vertices: np.ndarray, faces: np.ndarray) -> np.ndarray:
    """Compute per-vertex normals by averaging face normals."""
    normals = np.zeros_like(vertices)
    for face in faces:
        v0, v1, v2 = vertices[face[0]], vertices[face[1]], vertices[face[2]]
        edge1 = v1 - v0
        edge2 = v2 - v0
        fn = np.cross(edge1, edge2)
        norm = np.linalg.norm(fn)
        if norm > 1e-8:
            fn /= norm
        normals[face[0]] += fn
        normals[face[1]] += fn
        normals[face[2]] += fn
    norms = np.linalg.norm(normals, axis=1, keepdims=True)
    norms = np.maximum(norms, 1e-8)
    normals /= norms
    return normals


def _export_obj(vertices, faces, normals, uvs, obj_path, mtl_path, name, texture_path):
    """Export mesh to OBJ with MTL material file."""
    texture_filename = os.path.basename(texture_path)

    with open(mtl_path, "w") as f:
        f.write(f"# GameVoid AI-Generated Material (MiDaS)\n")
        f.write(f"newmtl {name}_material\n")
        f.write(f"Ka 0.2 0.2 0.2\n")
        f.write(f"Kd 0.8 0.8 0.8\n")
        f.write(f"Ks 0.1 0.1 0.1\n")
        f.write(f"Ns 32.0\n")
        f.write(f"d 1.0\n")
        f.write(f"illum 2\n")
        f.write(f"map_Kd {texture_filename}\n")

    with open(obj_path, "w") as f:
        f.write(f"# GameVoid AI-Generated 3D Model (MiDaS depth)\n")
        f.write(f"# Object: {name}\n")
        f.write(f"# Vertices: {len(vertices)}, Faces: {len(faces)}\n")
        f.write(f"mtllib {os.path.basename(mtl_path)}\n")
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

    print(f"[MiDaS] Exported: {obj_path}")
