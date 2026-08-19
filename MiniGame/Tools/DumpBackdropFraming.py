# Report the frame backdrop in camera-space, to check it matches the orthographic frame.

import unreal


CAMERA_TAG = "SDayCamera"
BACKDROP_TAG = "SDay.Backdrop"


def emit(message):
    unreal.log("[Backdrop] " + message)


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() or editor.get_editor_world()
    if world is None:
        unreal.log_error("[Backdrop] no world")
        return

    camera = None
    backdrop = None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        if CAMERA_TAG in tags:
            camera = actor
        elif BACKDROP_TAG in tags:
            backdrop = actor
    if camera is None or backdrop is None:
        unreal.log_error("[Backdrop] camera={} backdrop={}".format(camera, backdrop))
        return

    component = camera.camera_component
    rotation = camera.get_actor_rotation()
    origin = component.get_world_location()
    forward = unreal.MathLibrary.get_forward_vector(rotation)
    right = unreal.MathLibrary.get_right_vector(rotation)
    up = unreal.MathLibrary.get_up_vector(rotation)
    ortho_width = component.get_editor_property("ortho_width")
    aspect = component.get_editor_property("aspect_ratio")
    emit("camera ortho_width={:.1f} aspect={:.4f} -> frame {:.1f} x {:.1f}".format(
        ortho_width, aspect, ortho_width, ortho_width / aspect))

    def to_frame(point):
        delta = unreal.Vector(point.x - origin.x, point.y - origin.y, point.z - origin.z)
        return (delta.x * right.x + delta.y * right.y + delta.z * right.z,
                delta.x * up.x + delta.y * up.y + delta.z * up.z,
                delta.x * forward.x + delta.y * forward.y + delta.z * forward.z)

    emit("backdrop loc={} rot={} scale={}".format(
        backdrop.get_actor_location(), backdrop.get_actor_rotation(), backdrop.get_actor_scale3d()))

    centre, extent = backdrop.get_actor_bounds(False)
    emit("backdrop world bounds centre={} extent={}".format(centre, extent))

    component = backdrop.get_component_by_class(unreal.StaticMeshComponent)
    transform = component.get_world_transform()
    local = component.static_mesh.get_bounds()
    corners = []
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            corner = unreal.Vector(
                local.origin.x + sx * local.box_extent.x,
                local.origin.y + sy * local.box_extent.y,
                local.origin.z)
            corners.append(to_frame(unreal.MathLibrary.transform_location(transform, corner)))
    rights = [c[0] for c in corners]
    ups = [c[1] for c in corners]
    depths = [c[2] for c in corners]
    emit("mesh local extent={} (x{} y{})".format(
        local.box_extent, local.box_extent.x * 2.0, local.box_extent.y * 2.0))
    emit("corners in camera space: right {:.1f}..{:.1f} ({:.1f})  up {:.1f}..{:.1f} ({:.1f})  depth {:.1f}..{:.1f}".format(
        min(rights), max(rights), max(rights) - min(rights),
        min(ups), max(ups), max(ups) - min(ups),
        min(depths), max(depths)))


main()
