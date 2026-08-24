import unreal


ROOT = "/Game/Night/Course/Blueprints"
MATERIAL = "/Game/Night/Course/Materials/M_NightUnlitColor"


def load_class(path):
    return unreal.load_class(None, path)


def load_asset(path):
    return unreal.load_asset(path)


def create_or_get(name, parent_path):
    path = f"{ROOT}/{name}"
    existing = load_asset(path)
    if existing:
        return existing
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("ParentClass", load_class(parent_path))
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ROOT, unreal.Blueprint, factory)


def set_default(bp, values):
    if not bp:
        return
    generated_class = bp.generated_class()
    cdo = unreal.get_default_object(generated_class)
    for key, value in values.items():
        cdo.set_editor_property(key, value)
    cdo.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)


stone_parent = "/Script/MiniGame.NightCourseStoneActor"
bridge_parent = "/Script/MiniGame.NightBridgeSegmentActor"
pawn_parent = "/Script/MiniGame.NightCoursePawn"

hero = create_or_get("BP_NightHero", pawn_parent)
set_default(hero, {
    "HeroStaticMesh": load_asset("/Game/Night/Course/Art/Hero/kat"),
    "HeroMaterial": load_asset(MATERIAL),
    "HeroScale": 0.75,
})

foe_meshes = [
    "/Game/Night/Course/Art/Foe/fish",
    "/Game/Night/Course/Art/Foe/box1",
    "/Game/Night/Course/Art/Foe/box2",
    "/Game/Night/Course/Art/Foe/box3",
    "/Game/Night/Course/Art/Foe/cantingguai",
]
foes = []
for index, mesh_path in enumerate(foe_meshes, 1):
    bp = create_or_get(f"BP_NightFoeM{index:02d}", stone_parent)
    set_default(bp, {
        "FoeStaticMesh": load_asset(mesh_path),
        "FoeMaterial": load_asset(MATERIAL),
        "FoeYawOffsetDeg": 90.0,
        "FoeScale": 0.6,
        "FoeHeightOffsetCm": 70.0,
    })
    foes.append(bp)

bridges = []
for name, mesh_path in (
    ("BP_NightBridgeA", "/Game/Night/Course/Art/Bridge/muban1"),
    ("BP_NightBridgeB", "/Game/Night/Course/Art/Bridge/muban2"),
):
    bp = create_or_get(name, bridge_parent)
    set_default(bp, {
        "BridgeMeshOverride": load_asset(mesh_path),
        "BridgeMaterialOverride": load_asset(MATERIAL),
        "BridgeScaleMultiplier": 1.0,
        "BridgePivotOffsetCm": unreal.Vector(0.0, 0.0, 0.0),
        "bBridgeCollisionEnabled": True,
    })
    bridges.append(bp)

config = load_asset("/Game/Night/Course/Config/DA_Course")
if config:
    config.set_editor_property("HeroClass", hero.generated_class())
    # EFoeId is not always exported as a top-level unreal module symbol.
    # Reuse the reflected enum type held by the existing config property so
    # MapProperty receives native enum keys instead of Python strings.
    existing_default_foe = config.get_editor_property("DefaultFoeId")
    foe_enum = type(existing_default_foe)
    foe_map = {}
    for index, bp in enumerate(foes, 1):
        if not bp:
            raise RuntimeError("Missing foe Blueprint M{:02d}".format(index))
        foe_name = f"M{index:02d}"
        generated_class = bp.generated_class()
        if generated_class is None:
            raise RuntimeError("Foe Blueprint has no generated class: {}".format(bp.get_name()))
        if not unreal.MathLibrary.class_is_child_of(
                generated_class, unreal.NightCourseStoneActor):
            raise RuntimeError(
                "{} is not a NightCourseStoneActor Blueprint".format(bp.get_name()))
        foe_key = getattr(foe_enum, foe_name)
        foe_map[foe_key] = generated_class
    config.set_editor_property("FoeActorMap", foe_map)
    config.modify()
    config.MarkPackageDirtyForEditor()
    print("DA_Course HeroClass and FoeActorMap modified in memory; use UE Save All manually.")

print("VISUAL_BP_MIGRATION_COMPLETE")
