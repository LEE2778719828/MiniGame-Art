# Import the hatless parkour hero mesh onto the EXISTING SK_Hero_Skeleton as SK_Hero_NoHat.
#
# zhujuepaoku.fbx is the artist's re-export of the rigged hero with the hat geometry deleted:
# 5,016 verts against Slash_fast.fbx's 5,376, the difference being exactly maozi (180) plus
# pasted__maozi (180). Tools/CompareFbxRigs.py confirms it is a drop-in -- same 53 bones with
# an identical hierarchy, and all 47 skin bind matrices match to 0.0000 cm. It carries 15 extra
# leaf terminator bones ("_end" chains) that the exporter keeps appending; those hold no skin
# weights and merge into the skeleton harmlessly.
#
# SK_Hero is left untouched, so switching back is a one-field change on the pawn.
#
# The Blender Z-up conversion settings have to match Tools/ImportHeroRig.py exactly, or the
# mesh imports lying on its back and the shared animations no longer line up.
#
# INTERCHANGE MUST BE OFF FOR THE FBX PATH. On 5.8 Interchange claims FBX by default, and
# import_asset_tasks then trips "Assertion failed: ++Queue(QueueIndex).RecursionGuard == 1"
# (TaskGraph.cpp:705) inside InterchangeEngine and takes the whole editor down -- observed five
# times. Interchange also ignores FbxImportUI outright, so options.skeleton and the Z-up
# conversion below would be silently dropped even without the crash. The legacy importer is the
# one that produced SK_Hero in the first place, so it is also the only way to stay consistent
# with it. guard_interchange() refuses to import unless the flag is really off.
#
# Run from the editor console as THREE commands:
#
#   Interchange.FeatureFlags.Import.FBX 0
#   py "<project>/Tools/ImportHeroNoHat.py"
#   Interchange.FeatureFlags.Import.FBX 1

import os
import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_FBX = os.path.normpath(
    os.path.join(SCRIPT_DIR, "..", "..", "ArtSubmit", "Character", "mini_zhujue", "zhujuepaoku.fbx")
)

DEST_ROOT = "/Game/Night/Character"
SKELETON_PATH = DEST_ROOT + "/SK_Hero_Skeleton"
REFERENCE_MESH = DEST_ROOT + "/SK_Hero"
TARGET_MESH = DEST_ROOT + "/SK_Hero_NoHat"

# Whole-model vertex total reported by Tools/InspectFbxStructure.py for the source file.
EXPECTED_SOURCE_VERTS = 5016

TAG = "[ImportHeroNoHat]"


def log(msg):
    unreal.log("{} {}".format(TAG, msg))


def fail(msg):
    unreal.log_error("{} {}".format(TAG, msg))


def package_path(object_path):
    return str(object_path).split(".")[0]


def asset_class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def assets_of_class(directory, class_name, recursive=False):
    found = []
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        return found
    for object_path in unreal.EditorAssetLibrary.list_assets(
        directory, recursive=recursive, include_folder=False
    ):
        data = unreal.EditorAssetLibrary.find_asset_data(object_path)
        if data and asset_class_name(data) == class_name:
            found.append(package_path(object_path))
    return found


INTERCHANGE_FBX_CVAR = "Interchange.FeatureFlags.Import.FBX"


def read_cvar(name):
    try:
        return unreal.SystemLibrary.get_console_variable_int_value(name)
    except Exception as ex:
        log("cannot read {}: {}".format(name, ex))
        return None


def guard_interchange():
    """
    Make sure the legacy FBX importer will handle the task, and refuse to run if not.

    execute_console_command can be queued rather than applied immediately, so setting the flag
    from here is only a fallback -- the value is read back and a still-enabled Interchange is
    treated as a hard stop. Crashing the editor is a far worse outcome than not importing.
    """
    value = read_cvar(INTERCHANGE_FBX_CVAR)
    if value == 0:
        log("Interchange FBX path is off; the legacy importer will handle this")
        return True
    if value is None:
        fail("could not read {}; run the console command yourself before this script".format(
            INTERCHANGE_FBX_CVAR))
        return False

    log("Interchange FBX path is on (value={}); attempting to turn it off".format(value))
    world = None
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        world = None
    unreal.SystemLibrary.execute_console_command(
        world, "{} 0".format(INTERCHANGE_FBX_CVAR))

    value = read_cvar(INTERCHANGE_FBX_CVAR)
    if value != 0:
        fail("{} is still {}. Importing now would assert in InterchangeEngine and kill the "
             "editor. Run this in the console first, then re-run the script:".format(
                 INTERCHANGE_FBX_CVAR, value))
        fail("    {} 0".format(INTERCHANGE_FBX_CVAR))
        return False

    log("Interchange FBX path turned off")
    return True


def apply_scene_conversion(import_data):
    """Kept byte-identical to Tools/ImportHeroRig.py: Blender writes Z-up."""
    if import_data is None:
        return
    for name, value in (
        ("convert_scene", True),
        ("force_front_x_axis", False),
        ("convert_scene_unit", False),
    ):
        try:
            import_data.set_editor_property(name, value)
        except Exception as ex:
            fail("could not set {} on {}: {}".format(name, type(import_data).__name__, ex))


def mesh_options(skeleton):
    """
    Mesh only, bound to the skeleton we already ship.

    Materials and textures are skipped on purpose: MI_HeroSkin / MI_HeroSkel already exist and
    are wired into the pawn, and a fresh import would create a second unused set. The slots are
    copied off SK_Hero afterwards instead.
    """
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeleton = skeleton
    apply_scene_conversion(options.skeletal_mesh_import_data)
    apply_scene_conversion(options.anim_sequence_import_data)
    return options


def vertex_count(mesh):
    try:
        subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        return subsystem.get_num_verts(mesh, 0)
    except Exception as ex:
        log("vertex count unavailable: {}".format(ex))
        return -1


def describe(label, mesh):
    materials = mesh.get_editor_property("materials") or []
    slots = []
    for entry in materials:
        try:
            slot = str(entry.get_editor_property("material_slot_name"))
            interface = entry.get_editor_property("material_interface")
        except Exception:
            slot, interface = "?", None
        slots.append("{}={}".format(slot, interface.get_name() if interface else "None"))

    bounds = mesh.get_imported_bounds()
    log("{}: verts={} slots={} [{}]".format(
        label, vertex_count(mesh), len(materials), ", ".join(slots)))
    log("{}: bounds origin={} extent={} skeleton={}".format(
        label, bounds.origin, bounds.box_extent,
        mesh.get_editor_property("skeleton").get_name()))
    return materials


def slot_name(entry):
    try:
        return str(entry.get_editor_property("material_slot_name"))
    except Exception:
        return ""


def copy_materials(source, target):
    """
    Point the new mesh's slots at the materials SK_Hero already uses.

    Matching is by slot name, not index: the hatless export enumerates its meshes in a different
    order, so SK_Hero comes in as [blinn3_002, pasted__blinn3_002] while the new mesh comes in
    reversed. Both happen to resolve to MI_HeroSkin today, but relying on that would break the
    moment the two slots differ.

    Fresh FSkeletalMaterial structs are built rather than mutating the ones read back from the
    asset: those are copies, and writing the mutated list back left the original assignments in
    place with no error reported.
    """
    source_materials = source.get_editor_property("materials") or []
    target_materials = target.get_editor_property("materials") or []
    by_name = {slot_name(entry): entry.get_editor_property("material_interface")
               for entry in source_materials}

    rebuilt = []
    unmatched = []
    for entry in target_materials:
        name = slot_name(entry)
        if name in by_name:
            rebuilt.append(unreal.SkeletalMaterial(
                material_interface=by_name[name], material_slot_name=name))
        else:
            unmatched.append(name)
            rebuilt.append(entry)

    if unmatched:
        fail("no SK_Hero slot named {}; assign those by hand".format(unmatched))
        return False

    target.set_editor_property("materials", rebuilt)

    # Read back: the previous attempt reported success while nothing had changed.
    applied = target.get_editor_property("materials") or []
    for entry in applied:
        interface = entry.get_editor_property("material_interface")
        expected = by_name.get(slot_name(entry))
        if interface != expected:
            fail("slot {} still holds {} instead of {}".format(
                slot_name(entry),
                interface.get_name() if interface else "None",
                expected.get_name() if expected else "None"))
            return False

    log("copied {} material slot(s) from SK_Hero, verified".format(len(applied)))
    return True


def main():
    if not os.path.isfile(SOURCE_FBX):
        fail("source missing: {}".format(SOURCE_FBX))
        return
    log("source: {}".format(SOURCE_FBX))

    # Checked before anything is deleted or created, so a refusal leaves the project untouched.
    if not guard_interchange():
        return

    skeleton = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
    if skeleton is None:
        fail("shared skeleton missing at {}".format(SKELETON_PATH))
        return

    reference = unreal.EditorAssetLibrary.load_asset(REFERENCE_MESH)
    if reference is None:
        fail("reference mesh missing at {}".format(REFERENCE_MESH))
        return
    describe("SK_Hero (before)", reference)

    skeletons_before = assets_of_class(DEST_ROOT, "Skeleton", recursive=True)

    # Re-runs must not stack up SK_Hero_NoHat_1, _2, ... so the previous result is removed.
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MESH):
        if not unreal.EditorAssetLibrary.delete_asset(TARGET_MESH):
            fail("could not delete existing {} (asset editor open?)".format(TARGET_MESH))
            return
        log("removed previous {}".format(TARGET_MESH))

    meshes_before = set(assets_of_class(DEST_ROOT, "SkeletalMesh"))

    task = unreal.AssetImportTask()
    task.filename = SOURCE_FBX
    task.destination_path = DEST_ROOT
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = mesh_options(skeleton)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = [package_path(p) for p in task.imported_object_paths]
    log("import produced {} asset(s): {}".format(len(imported), imported))

    new_meshes = [p for p in assets_of_class(DEST_ROOT, "SkeletalMesh") if p not in meshes_before]
    if len(new_meshes) != 1:
        fail("expected exactly 1 new skeletal mesh, got {}: {}".format(len(new_meshes), new_meshes))
        return

    if new_meshes[0] != TARGET_MESH:
        if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MESH):
            fail("rename target already exists: {}".format(TARGET_MESH))
            return
        if not unreal.EditorAssetLibrary.rename_asset(new_meshes[0], TARGET_MESH):
            fail("rename {} -> {} failed".format(new_meshes[0], TARGET_MESH))
            return
        log("renamed {} -> {}".format(new_meshes[0], TARGET_MESH))

    target = unreal.EditorAssetLibrary.load_asset(TARGET_MESH)
    if target is None:
        fail("could not load {}".format(TARGET_MESH))
        return

    materials_ok = copy_materials(reference, target)
    describe("SK_Hero_NoHat", target)

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)

    log("=== RESULT ===")
    problems = []

    # A second skeleton means the import ignored options.skeleton, and every existing clip
    # would refuse to play on the new mesh.
    skeletons_after = assets_of_class(DEST_ROOT, "Skeleton", recursive=True)
    if len(skeletons_after) != len(skeletons_before):
        problems.append("skeleton count went {} -> {}: {}".format(
            len(skeletons_before), len(skeletons_after), skeletons_after))
    else:
        log("OK: still {} skeleton(s), no duplicate rig".format(len(skeletons_after)))

    bound = target.get_editor_property("skeleton")
    if bound is None or bound.get_path_name().split(".")[0] != SKELETON_PATH:
        problems.append("new mesh is bound to {} instead of {}".format(
            bound.get_path_name() if bound else "None", SKELETON_PATH))
    else:
        log("OK: bound to the shared SK_Hero_Skeleton")

    hat_verts = vertex_count(reference) - vertex_count(target)
    log("vertex delta vs SK_Hero: {} (source files differ by 360 across 4 mesh nodes)".format(
        hat_verts))
    if hat_verts <= 0:
        problems.append("new mesh is not lighter than SK_Hero; the hat may still be present")

    if not materials_ok:
        problems.append("material slots were not copied")

    if problems:
        for item in problems:
            fail("FAILED: " + item)
    else:
        log("OK: SK_Hero_NoHat ready. Set BP_NightCoursePawn -> Hero Skeletal Mesh to it.")


main()
