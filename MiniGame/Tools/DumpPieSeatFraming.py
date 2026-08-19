# Compare the live seat portraits against the plate row in camera space.

import unreal


OUT_PATH = "E:/UEProjects/MiniGame/MiniGame/Saved/NexusCaptures/pie_seat_framing.txt"


def main():
    lines = []

    def emit(text):
        lines.append(text)
        unreal.log("[SeatFrame] " + text)

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        unreal.log_error("[SeatFrame] no PIE world")
        return

    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    camera = None
    plates = None
    seats = []
    for actor in actors:
        tags = [str(t) for t in actor.tags]
        if "SDayCamera" in tags:
            camera = actor
        if "SDay.CustomerPlates" in tags:
            plates = actor
        if "SDayCharacterStandIn" in actor.get_class().get_name():
            seats.append(actor)
        if isinstance(actor, unreal.CameraActor):
            emit("camera actor {} loc={} rot={} tags={}".format(
                actor.get_actor_label(), actor.get_actor_location(),
                actor.get_actor_rotation(), tags))
    if camera is None or plates is None:
        unreal.log_error("[SeatFrame] camera={} plates={}".format(camera, plates))
        return

    comp = camera.camera_component
    loc = camera.get_actor_location()
    rot = camera.get_actor_rotation()
    right = rot.get_right_vector()
    up = rot.get_up_vector()
    forward = rot.get_forward_vector()
    emit("camera loc=({:.1f},{:.1f},{:.1f}) pitch={:.2f} yaw={:.2f} ortho_w={:.1f} aspect={:.4f}".format(
        loc.x, loc.y, loc.z, rot.pitch, rot.yaw, comp.ortho_width, comp.aspect_ratio))

    def frame(x, y, z):
        d = unreal.Vector(x - loc.x, y - loc.y, z - loc.z)
        return (d.dot(right), d.dot(up), d.dot(forward))

    origin, extent = plates.get_actor_bounds(False)
    emit("plates X {:.1f}..{:.1f} Y {:.1f}..{:.1f} Z {:.1f}..{:.1f}".format(
        origin.x - extent.x, origin.x + extent.x,
        origin.y - extent.y, origin.y + extent.y,
        origin.z - extent.z, origin.z + extent.z))
    pitch_x = extent.x * 2.0 / 4.0
    slots = []
    for index in range(4):
        cx = origin.x + extent.x - pitch_x * (index + 0.5)
        r, u, f = frame(cx, origin.y + extent.y, origin.z + extent.z)
        slots.append((cx, r, u, f))
        emit("  plate slot {} centre X {:>7.1f}  cam right {:>8.1f} up {:>8.1f} depth {:>8.1f}".format(
            index, cx, r, u, f))
    plate_top_up = slots[0][2] if slots else 0.0

    seats.sort(key=lambda a: -a.get_actor_location().x)
    for seat in seats:
        p = seat.get_actor_location()
        r, u, f = frame(p.x, p.y, p.z)
        nearest = min(range(len(slots)), key=lambda i: abs(slots[i][1] - r)) if slots else -1
        portrait = None
        for component in seat.get_components_by_class(unreal.BillboardComponent):
            portrait = component
        sprite = portrait.sprite if portrait else None
        scale = portrait.get_world_scale() if portrait else unreal.Vector.ZERO
        emit("  {:<26} occupied={} slot={} world=({:>7.1f},{:>7.1f},{:>7.1f}) cam right {:>8.1f} up {:>8.1f} depth {:>8.1f}".format(
            seat.get_actor_label(), int(seat.get_editor_property("occupied")), nearest,
            p.x, p.y, p.z, r, u, f))
        if sprite:
            height = 0.5 * scale.z * sprite.blueprint_get_size_y()
            centre = portrait.get_world_location()
            _, sprite_up, _ = frame(centre.x, centre.y, centre.z)
            bottom = sprite_up - 0.5 * height
            emit("      sprite {} {}x{} scale {:.3f} -> world height {:.1f} offset {:.1f} image bottom up {:.1f} (plate top up {:.1f}, gap {:.1f})".format(
                sprite.get_name(), sprite.blueprint_get_size_x(), sprite.blueprint_get_size_y(),
                scale.z, height, sprite_up - u, bottom, plate_top_up, bottom - plate_top_up))
        else:
            emit("      sprite none")

    with open(OUT_PATH, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines))


main()
