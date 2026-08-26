"""Dump the pre-RouteModes DA_Rules data before the C++ property migration."""

import json
import os

import unreal


RULE_PATH = "/Game/Night/Course/Config/DA_Rules"
OUTPUT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(),
    "NightLegacyRuleData.json",
)


def enum_name(value):
    text = str(value)
    text = text.split(":", 1)[0].strip()
    text = text.rsplit(".", 1)[-1]
    return {
        "ENEMY": "Enemy",
        "HAZARD": "Hazard",
    }.get(text.upper(), text)


def read_entry(entry):
    return {
        "atomKey": entry.get_editor_property("AtomKey"),
        "actions": [
            enum_name(action)
            for action in entry.get_editor_property("Actions")
        ],
        "weight": entry.get_editor_property("Weight"),
    }


def read_queue(queue):
    return {
        "targetAtomCount": queue.get_editor_property("TargetAtomCount"),
        "atoms": [
            read_entry(entry)
            for entry in queue.get_editor_property("Atoms")
        ],
    }


def main():
    rule = unreal.EditorAssetLibrary.load_asset(RULE_PATH)
    if rule is None:
        raise RuntimeError("Missing {}".format(RULE_PATH))

    data = {
        "seed": rule.get_editor_property("Seed"),
        "autoSelectAtomKeys": rule.get_editor_property("bAutoSelectAtomKeys"),
        "editorJson": rule.get_editor_property("EditorJson"),
        "baseAtomCount": rule.get_editor_property("BaseAtomCount"),
        "baseRoute": [
            read_entry(entry)
            for entry in rule.get_editor_property("BaseRoute")
        ],
        "branchRoutes": {},
        "forkAfterBaseAtomIndex": rule.get_editor_property(
            "ForkAfterBaseAtomIndex"
        ),
    }
    for route_id, queue in rule.get_editor_property("BranchRoutes").items():
        data["branchRoutes"][enum_name(route_id)] = read_queue(queue)

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as output:
        json.dump(data, output, ensure_ascii=False, indent=2)
    unreal.log(
        "[NightLegacyRule] dumped {} to {}".format(RULE_PATH, OUTPUT_PATH)
    )


main()
