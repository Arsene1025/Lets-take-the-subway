"""
Export every greybox mesh in GreyBoxMap001.blend as its own FBX, baked to the object origin,
plus a JSON manifest with the UE-space placement of each piece.

Run headless from the repository root:

  "C:\\Program Files\\Blender Foundation\\Blender 5.1\\blender.exe" -b RawContent\\GreyBox\\GreyBoxMap001.blend --python Tools\\Blender\\export_greybox.py

Why one FBX per object, baked at the origin:
  The FBX node transform is applied differently by the legacy FBX importer and by Interchange
  ("transform vertex to absolute" vs "bake meshes"). Baking rotation and scale into the vertices
  here and leaving the node at identity removes that ambiguity: UE always receives a mesh in
  local space, and the manifest carries the only transform that matters (the location).

The .blend is never modified: everything is done on temporary copies and the file is not saved.
"""

import json
import os
import re
import sys

import bpy
from mathutils import Matrix, Vector

# Objects that are gameplay markers, not level geometry.
EXCLUDE = {"Player", "Player.001"}

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(bpy.data.filepath), "..", ".."))
OUT_DIR = os.path.join(REPO_ROOT, "RawContent", "GreyBox", "FBX")
MANIFEST_PATH = os.path.join(REPO_ROOT, "RawContent", "GreyBox", "GreyBoxMap001_manifest.json")

DIM_SUFFIX = re.compile(r"(\s*-\s*|_)?[\d.]+\*[\d.]+(\*[\d.]+)?\s*$")


def clean_name(name):
    """'Elevator - 3*3*4' -> 'Elevator', 'TicketGate_B1_001_0.5*1*1' -> 'TicketGate_B1_001'."""
    base = DIM_SUFFIX.sub("", name)
    base = re.sub(r"[^A-Za-z0-9]+", "_", base).strip("_")
    return base or "Unnamed"


def collection_path(obj):
    """Innermost user collection and its ancestors, e.g. 'Stage001/B1'."""
    def find(col, target, trail):
        if col is target:
            return trail
        for child in col.children:
            found = find(child, target, trail + [child.name])
            if found is not None:
                return found
        return None

    for col in obj.users_collection:
        trail = find(bpy.context.scene.collection, col, [])
        if trail:
            return "/".join(trail)
    return ""


def to_ue(v):
    """Blender metres (right-handed) -> UE centimetres (left-handed): Y flips."""
    return [round(v.x * 100.0, 3), round(-v.y * 100.0, 3), round(v.z * 100.0, 3)]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    scene = bpy.context.scene
    depsgraph = bpy.context.evaluated_depsgraph_get()

    export_col = bpy.data.collections.new("_GreyBoxExportTmp")
    scene.collection.children.link(export_col)

    manifest = {
        "source_blend": os.path.basename(bpy.data.filepath),
        "units": "UE centimetres, Y flipped from Blender",
        "objects": [],
    }

    world_min = Vector((1e9, 1e9, 1e9))
    world_max = Vector((-1e9, -1e9, -1e9))
    used_names = set()

    sources = [o for o in bpy.data.objects if o.type == "MESH" and o.name not in EXCLUDE]
    sources.sort(key=lambda o: o.name)

    for src in sources:
        name = clean_name(src.name)
        if name in used_names:
            raise RuntimeError(f"Duplicate clean name '{name}' from '{src.name}'")
        used_names.add(name)

        eval_obj = src.evaluated_get(depsgraph)
        mesh = bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers=False, depsgraph=depsgraph)
        mesh.name = f"SM_{name}"

        # Bake rotation and scale; keep the translation as the actor location.
        world = src.matrix_world.copy()
        location = world.to_translation()
        world.translation = Vector((0.0, 0.0, 0.0))
        mesh.transform(world)
        mesh.materials.clear()

        tmp = bpy.data.objects.new(f"SM_{name}", mesh)
        tmp.matrix_world = Matrix.Identity(4)
        export_col.objects.link(tmp)

        bpy.ops.object.select_all(action="DESELECT")
        tmp.select_set(True)
        bpy.context.view_layer.objects.active = tmp

        fbx_path = os.path.join(OUT_DIR, f"SM_{name}.fbx")
        bpy.ops.export_scene.fbx(
            filepath=fbx_path,
            use_selection=True,
            object_types={"MESH"},
            apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_NONE",
            axis_forward="-Y",
            axis_up="Z",
            bake_space_transform=False,
            use_mesh_modifiers=False,
            mesh_smooth_type="FACE",
            use_custom_props=False,
            add_leaf_bones=False,
            path_mode="AUTO",
            embed_textures=False,
        )

        # Bounds in world space for validation.
        bb_min = Vector((1e9, 1e9, 1e9))
        bb_max = Vector((-1e9, -1e9, -1e9))
        for v in mesh.vertices:
            p = v.co + location
            for i in range(3):
                bb_min[i] = min(bb_min[i], p[i])
                bb_max[i] = max(bb_max[i], p[i])
                world_min[i] = min(world_min[i], p[i])
                world_max[i] = max(world_max[i], p[i])

        manifest["objects"].append({
            "asset": f"SM_{name}",
            "source_object": src.name,
            "collection": collection_path(src),
            "fbx": os.path.relpath(fbx_path, REPO_ROOT).replace("\\", "/"),
            "location_ue": to_ue(location),
            "bounds_min_ue": to_ue(bb_min),
            "bounds_max_ue": to_ue(bb_max),
            "vertices": len(mesh.vertices),
            "faces": len(mesh.polygons),
        })

        export_col.objects.unlink(tmp)
        bpy.data.objects.remove(tmp)
        bpy.data.meshes.remove(mesh)
        print(f"exported {src.name!r:36} -> {os.path.basename(fbx_path)}  loc_ue={manifest['objects'][-1]['location_ue']}")

    scene.collection.children.unlink(export_col)
    bpy.data.collections.remove(export_col)

    manifest["bounds_min_ue"] = to_ue(world_min)
    manifest["bounds_max_ue"] = to_ue(world_max)
    manifest["markers"] = {
        "player_start_ue": to_ue(bpy.data.objects["Player"].location) if "Player" in bpy.data.objects else None,
        "player_start_stage000_ue": to_ue(bpy.data.objects["Player.001"].location) if "Player.001" in bpy.data.objects else None,
        "camera_ue": to_ue(bpy.data.objects["Camera"].location) if "Camera" in bpy.data.objects else None,
    }

    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)

    size = world_max - world_min
    print(f"\n{len(manifest['objects'])} objects exported to {OUT_DIR}")
    print(f"world bounds (Blender m): min={tuple(round(v, 2) for v in world_min)} max={tuple(round(v, 2) for v in world_max)} size={tuple(round(v, 2) for v in size)}")
    print(f"manifest: {MANIFEST_PATH}")


try:
    main()
except Exception:
    import traceback
    traceback.print_exc()
    sys.exit(1)
