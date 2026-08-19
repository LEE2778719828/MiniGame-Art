# Make L_S_DayWhitebox workable in the editor viewport.
#
# The level ships with no lights because ASDayBoardPresenter creates its own
# WhiteboxKeyLight / WhiteboxFillLight components in BeginPlay. That leaves the editor
# viewport black, so dressing the level meant starting PIE, where actors cannot be moved.
#
# These lights are flagged editor-only so cooked builds strip them, and hidden in game so PIE
# does not light the scene twice: is_editor_only_actor only removes actors from a cook, not from
# a PIE session, and the doubled-up light was what made PIE brighter and warmer than it ships.
# Key light values mirror the presenter so the viewport roughly matches what ships.

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
LABEL_PREFIX = "EDITORONLY_Light_"
ACTOR_FOLDER = "Environment/EditorOnlyLighting"

# The key light shines along the camera axis, back to front, so it is read from the tagged
# composition camera rather than hard-coded. ASDayBoardPresenter does the same at BeginPlay.
CAMERA_TAG = "SDayCamera"
KEY_ROTATION_FALLBACK = (-55.0, -35.0)
# Matches DayArtKeyLightIntensity in SDayBoardPresentation.cpp. The project renders at a fixed
# exposure, so intensity is the frame's brightness; keep both sides in step or the viewport lies.
KEY_INTENSITY = 1.8
# Matches DayArtKeyLightColor. The old warm key was doing the work the (still grey) restaurant
# materials will do later, which tipped the whole frame yellow.
KEY_COLOR = (1.0, 0.985, 0.965)

# The fill shares the camera's yaw but comes from higher up, so upward-facing surfaces such
# as the counter top keep some shape once the key light is nearly horizontal.
FILL_PITCH_OFFSET = -40.0

# Two directional lights otherwise tie for the single slot used by forward shading,
# translucency, water and volumetric fog. Distinct priorities name the key light as that
# slot's owner instead of letting the engine fall back to picking by overall brightness.
KEY_FORWARD_PRIORITY = 1
FILL_FORWARD_PRIORITY = 0

# Stand-in for WhiteboxFillLight. A SkyLight is deliberately avoided: real-time capture
# warns and returns black without a SkyAtmosphere or VolumetricCloud, and the presenter's
# own SkyLight has no cubemap assigned, so it contributes nothing at runtime either.
# Scaled with the key so it stays a fill rather than becoming the brightest light in frame.
FILL_INTENSITY = 0.35
FILL_COLOR = (0.55, 0.68, 1.0)

# Drop the viewport camera onto the board using the tilted framing we settled on.
VIEWPORT_LOCATION = (0.0, -699.0, 1802.0)
VIEWPORT_ROTATION = (-55.0, 90.0)


def log(message):
    unreal.log("[EditorLights] " + message)


def mark_editor_only(actor):
    """Keep these lights out of cooked builds, and out of the way in PIE."""
    try:
        actor.set_editor_property("is_editor_only_actor", True)
        actor.set_actor_hidden_in_game(True)
        return True
    except Exception as ex:
        unreal.log_warning("[EditorLights] could not flag editor-only: {}".format(ex))
        return False


def camera_rotation(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        if CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            rotation = actor.get_actor_rotation()
            return rotation.pitch, rotation.yaw
    unreal.log_warning("[EditorLights] no camera tagged {}; using fallback key angle".format(
        CAMERA_TAG))
    return KEY_ROTATION_FALLBACK


def main():
    # This edits and saves the level, and get_editor_world() reads as empty during PIE, which
    # would trigger a map reload and throw away unsaved edits such as a hand-placed camera.
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor_subsystem.get_game_world() is not None:
        unreal.log_error("[EditorLights] stop PIE first")
        return

    # Only load the map when it is not the one already open, for the same reason.
    current = editor_subsystem.get_editor_world()
    if current is None or current.get_path_name().split(".")[0] != LEVEL_PATH:
        unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    key_rotation = camera_rotation(actor_subsystem)
    fill_rotation = (key_rotation[0] + FILL_PITCH_OFFSET, key_rotation[1])
    log("key aimed along camera pitch={:.1f} yaw={:.1f}; fill pitch={:.1f}".format(
        key_rotation[0], key_rotation[1], fill_rotation[0]))

    # Make reruns deterministic.
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label().startswith(LABEL_PREFIX):
            actor_subsystem.destroy_actor(actor)
            removed += 1
    log("removed {} previous light(s)".format(removed))

    key = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 1200.0),
        unreal.Rotator(roll=0.0, pitch=key_rotation[0], yaw=key_rotation[1]),
    )
    if key:
        key.set_actor_label(LABEL_PREFIX + "Key")
        key.set_folder_path(ACTOR_FOLDER)
        component = key.get_component_by_class(unreal.DirectionalLightComponent)
        if component:
            component.set_intensity(KEY_INTENSITY)
            component.set_light_color(unreal.LinearColor(*KEY_COLOR))
            # Static lighting would demand a build; keep it dynamic for a whitebox.
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_editor_property(
                "forward_shading_priority", KEY_FORWARD_PRIORITY)
        mark_editor_only(key)
        log("key light ready")

    fill = actor_subsystem.spawn_actor_from_class(
        unreal.DirectionalLight,
        unreal.Vector(0.0, 0.0, 1200.0),
        unreal.Rotator(roll=0.0, pitch=fill_rotation[0], yaw=fill_rotation[1]),
    )
    if fill:
        fill.set_actor_label(LABEL_PREFIX + "Fill")
        fill.set_folder_path(ACTOR_FOLDER)
        component = fill.get_component_by_class(unreal.DirectionalLightComponent)
        if component:
            component.set_intensity(FILL_INTENSITY)
            component.set_light_color(unreal.LinearColor(*FILL_COLOR))
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            # One shadow caster is enough for a whitebox; the fill only lifts dark sides.
            component.set_editor_property("cast_shadows", False)
            component.set_editor_property(
                "forward_shading_priority", FILL_FORWARD_PRIORITY)
        mark_editor_only(fill)
        log("fill light ready")

    # Park the viewport on the composition camera when there is one, so what the viewport
    # shows matches what PIE will render.
    viewport_location = unreal.Vector(*VIEWPORT_LOCATION)
    viewport_rotation = unreal.Rotator(
        roll=0.0, pitch=VIEWPORT_ROTATION[0], yaw=VIEWPORT_ROTATION[1])
    for actor in actor_subsystem.get_all_level_actors():
        if CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            viewport_location = actor.get_actor_location()
            viewport_rotation = actor.get_actor_rotation()
            break

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(viewport_location, viewport_rotation)

    for actor in actor_subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        if not label.startswith(LABEL_PREFIX):
            continue
        component = actor.get_component_by_class(unreal.DirectionalLightComponent)
        log("{} editor_only={} forward_priority={}".format(
            label,
            actor.get_editor_property("is_editor_only_actor"),
            component.get_editor_property("forward_shading_priority") if component else "n/a"))

    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
    log("saved={}".format(saved))


main()
