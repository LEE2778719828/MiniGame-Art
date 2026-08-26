"""Migrate canonical Night assets to RouteModes and FoeDropMap."""

import copy
import json
import os

import unreal


CONFIG_PATH = "/Game/Night/Course/Config/DA_Course"
RULE_PATH = "/Game/Night/Course/Config/DA_Rules"
LEGACY_DUMP = os.path.join(
    unreal.Paths.project_saved_dir(),
    "NightLegacyRuleData.json",
)


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: {}".format(path))
    return asset


def make_package_writable(asset_path):
    filename = os.path.join(
        unreal.Paths.project_content_dir(),
        asset_path.replace("/Game/", "") + ".uasset",
    )
    if os.path.exists(filename):
        os.chmod(filename, 0o666)


def enum_member(enum_type, canonical_name):
    target = canonical_name.lower().replace("_", "")
    for name in dir(enum_type):
        if name.lower().replace("_", "") == target:
            return getattr(enum_type, name)
    raise RuntimeError(
        "Could not find enum member {} on {}".format(
            canonical_name,
            enum_type,
        )
    )


def call_import_json(rule, json_text):
    method = getattr(rule, "import_json", None)
    if method is None:
        method = getattr(rule, "ImportJson", None)
    if method is None:
        raise RuntimeError("UNightCourseRuleData.ImportJson is not exposed to Python")
    try:
        result = method(json_text)
    except TypeError:
        result = method(json_text, "")
    error = ""
    if isinstance(result, tuple):
        ok = result[0]
        if len(result) > 1:
            error = result[1]
    elif isinstance(result, str):
        # Unreal Python exposes FString& OutError as the return value for
        # this UFUNCTION; the bool return is not included in that wrapper.
        error = result
        ok = len(rule.get_editor_property("RouteModes")) > 0
    else:
        ok = result
    if not ok:
        raise RuntimeError("DA_Rules ImportJson failed: {}".format(error))


def read_legacy_json():
    if not os.path.exists(LEGACY_DUMP):
        raise RuntimeError(
            "Missing legacy rule backup {}; run DumpNightLegacyRuleData.py first".format(
                LEGACY_DUMP
            )
        )
    with open(LEGACY_DUMP, "r", encoding="utf-8") as source:
        return json.load(source)


def build_new_rule_json(legacy):
    main_queue = {
        "targetAtomCount": legacy.get("baseAtomCount", 0),
        "atoms": legacy.get("baseRoute", []),
    }
    route_modes = {
        "A": copy.deepcopy(main_queue),
        "B": copy.deepcopy(main_queue),
        "C": copy.deepcopy(main_queue),
    }
    return json.dumps(
        {
            "seed": legacy.get("seed", 1001),
            "autoSelectAtomKeys": legacy.get("autoSelectAtomKeys", True),
            "routeModes": route_modes,
            "branchRoutes": legacy.get("branchRoutes", {}),
            "forkAfterBaseAtomIndex": legacy.get(
                "forkAfterBaseAtomIndex", -1
            ),
        },
        ensure_ascii=False,
    )


def migrate_rules():
    make_package_writable(RULE_PATH)
    rule = load_asset(RULE_PATH)
    legacy = read_legacy_json()
    new_json = build_new_rule_json(legacy)
    call_import_json(rule, new_json)
    rule.modify()
    if not unreal.EditorAssetLibrary.save_asset(RULE_PATH):
        raise RuntimeError("Could not save {}".format(RULE_PATH))
    unreal.log("[NightMigration] migrated DA_Rules to RouteModes A/B/C")


def migrate_drops():
    make_package_writable(CONFIG_PATH)
    config = load_asset(CONFIG_PATH)
    foe_enum = type(config.get_editor_property("DefaultFoeId"))
    ingredient_enum = type(config.get_editor_property("DefaultDropId"))
    config.set_editor_property(
        "FoeDropMap",
        {
            enum_member(foe_enum, "M01"): enum_member(ingredient_enum, "F01_LingGu"),
            enum_member(foe_enum, "M02"): enum_member(ingredient_enum, "F02_YinShanJun"),
            enum_member(foe_enum, "M03"): enum_member(ingredient_enum, "F03_ChiYanJiao"),
            enum_member(foe_enum, "M04"): enum_member(ingredient_enum, "F04_YueLinYu"),
            enum_member(foe_enum, "M05"): enum_member(ingredient_enum, "F05_XuanYuQin"),
        },
    )
    config.set_editor_property("PreviewDefaultRoute", getattr(
        type(config.get_editor_property("PreviewDefaultRoute")),
        "A",
    ))
    config.modify()
    if not unreal.EditorAssetLibrary.save_asset(CONFIG_PATH):
        raise RuntimeError("Could not save {}".format(CONFIG_PATH))
    unreal.log("[NightMigration] configured DA_Course FoeDropMap M01-M05")


def main():
    migrate_rules()
    migrate_drops()
    unreal.log("[NightMigration] COMPLETE")


main()
