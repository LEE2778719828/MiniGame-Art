import os
import unreal

AUDIO_DIR = os.path.join(
    unreal.Paths.project_content_dir(),
    "Night",
    "Course",
    "Audio",
)
DEST = "/Game/Night/Course/Audio"
NAMES = [
    "SW_Slash.wav",
    "SW_IngredientDrop.wav",
    "SW_Hit_Aquatic_Voice.wav",
    "SW_Hit_Rice_Voice.wav",
    "SW_Hit_Rice_Material.wav",
    "SW_Hit_Bat_Material.mp3",
    "SW_Hit_Fish_Voice.wav",
    "SW_Hit_Fish_Material.wav",
]

tasks = []
for name in NAMES:
    source = os.path.join(AUDIO_DIR, name)
    if not os.path.isfile(source):
        unreal.log_error("Night audio import missing: %s" % source)
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", os.path.splitext(name)[0])
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tasks.append(task)

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for task in tasks:
        for path in task.get_editor_property("imported_object_paths"):
            unreal.log("Night audio imported: %s" % path)
