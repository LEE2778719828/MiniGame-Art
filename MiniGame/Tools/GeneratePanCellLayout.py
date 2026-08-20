"""Rebuild DT_SDayBoardLayout from the wells modelled into the pan mesh.

The pan is a 2.5D panel tilted toward the camera, and its wells are what the concept art
calls the cooking holes, so they are the authority on where dishes sit. Placing cells on a
horizontal plane instead leaves them floating off the panel, which is the bug this fixes.

Rows are written in the pan actor's local space with a rotation that puts each cell's +Z
along the panel normal. ASDayBoardPresenter composes them with the live SDay.Board transform,
so moving, rotating or rescaling the pan in the level keeps every cell seated in its well.

Run after any pan re-import or re-placement.
"""

import math

import unreal


LEVEL_PATH = "/Game/Day/Maps/L_S_DayWhitebox"
TABLE_PATH = "/Game/Day/Data/DT_SDayBoardLayout"
BOARD_TAG = "SDay.Board"

SAMPLES_U = 120
SAMPLES_V = 156
# Well floors sit ~5 below the fitted panel plane; the walls between them rise above it.
WELL_DEPTH_THRESHOLD = 4.0
MIN_CLUSTER_SAMPLES = 100
# Keep the clickable disc inside the well wall rather than spilling onto the rim.
RADIUS_INSET = 0.94


def log(message):
    unreal.log("[PanLayout] " + message)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def solve3(m, rhs):
    m = [row[:] for row in m]
    rhs = rhs[:]
    for i in range(3):
        pivot = max(range(i, 3), key=lambda r: abs(m[r][i]))
        m[i], m[pivot] = m[pivot], m[i]
        rhs[i], rhs[pivot] = rhs[pivot], rhs[i]
        for r in range(i + 1, 3):
            factor = m[r][i] / m[i][i]
            for c in range(i, 3):
                m[r][c] -= factor * m[i][c]
            rhs[r] -= factor * rhs[i]
    out = [0.0, 0.0, 0.0]
    for i in reversed(range(3)):
        out[i] = (rhs[i] - sum(m[i][c] * out[c] for c in range(i + 1, 3))) / m[i][i]
    return out


def rotator_to_quat(pitch, yaw, roll):
    """Mirrors FRotator::Quaternion so the CSV matches what the engine would produce."""
    half = math.radians(0.5)
    sp, cp = math.sin(pitch * half), math.cos(pitch * half)
    sy, cy = math.sin(yaw * half), math.cos(yaw * half)
    sr, cr = math.sin(roll * half), math.cos(roll * half)
    return (
        cr * sp * sy - sr * cp * cy,
        -cr * sp * cy - sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy,
    )


def find_board(subsystem):
    for actor in subsystem.get_all_level_actors():
        if BOARD_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            return actor
    return None


def sample_panel(component):
    """Return (plane origin, normal, u axis, v axis, depth grid, u span, v span)."""
    mesh = unreal.DynamicMesh()
    mesh, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        component.static_mesh, mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())
    mesh = unreal.GeometryScript_MeshTransforms.transform_mesh(
        mesh, component.get_world_transform())
    bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh)
    if isinstance(bvh, tuple):
        bvh = bvh[-1]
    options = unreal.GeometryScriptSpatialQueryOptions()
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    lo, hi = box.min, box.max

    def cast(origin, direction):
        return unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
            mesh, bvh, origin, direction, options)[1]

    # Coarse top-down pass to fit the panel plane.
    coarse = []
    for row in range(47):
        y = lo.y + (hi.y - lo.y) * (row + 0.5) / 47
        for col in range(61):
            x = lo.x + (hi.x - lo.x) * (col + 0.5) / 61
            hit = cast(unreal.Vector(x, y, hi.z + 50.0), unreal.Vector(0.0, 0.0, -1.0))
            if hit.hit:
                coarse.append((x, y, hit.hit_position.z))

    n = float(len(coarse))
    acc = dict.fromkeys(("x", "y", "z", "xx", "yy", "xy", "xz", "yz"), 0.0)
    for px, py, pz in coarse:
        acc["x"] += px
        acc["y"] += py
        acc["z"] += pz
        acc["xx"] += px * px
        acc["yy"] += py * py
        acc["xy"] += px * py
        acc["xz"] += px * pz
        acc["yz"] += py * pz
    a, b, c = solve3(
        [[acc["xx"], acc["xy"], acc["x"]],
         [acc["xy"], acc["yy"], acc["y"]],
         [acc["x"], acc["y"], n]],
        [acc["xz"], acc["yz"], acc["z"]])

    length = math.sqrt(a * a + b * b + 1.0)
    normal = (-a / length, -b / length, 1.0 / length)
    centre = (acc["x"] / n, acc["y"] / n, acc["z"] / n)
    u = (1.0, 0.0, -a)
    ulen = math.sqrt(dot(u, u))
    u = (u[0] / ulen, u[1] / ulen, u[2] / ulen)
    v = (normal[1] * u[2] - normal[2] * u[1],
         normal[2] * u[0] - normal[0] * u[2],
         normal[0] * u[1] - normal[1] * u[0])

    offsets = [(p[0] - centre[0], p[1] - centre[1], p[2] - centre[2]) for p in coarse]
    span_u = (min(dot(o, u) for o in offsets), max(dot(o, u) for o in offsets))
    span_v = (min(dot(o, v) for o in offsets), max(dot(o, v) for o in offsets))

    lift = 200.0
    depths = []
    for row in range(SAMPLES_V):
        cv = span_v[0] + (span_v[1] - span_v[0]) * (row + 0.5) / SAMPLES_V
        line = []
        for col in range(SAMPLES_U):
            cu = span_u[0] + (span_u[1] - span_u[0]) * (col + 0.5) / SAMPLES_U
            origin = unreal.Vector(
                centre[0] + u[0] * cu + v[0] * cv + normal[0] * lift,
                centre[1] + u[1] * cu + v[1] * cv + normal[1] * lift,
                centre[2] + u[2] * cu + v[2] * cv + normal[2] * lift)
            hit = cast(origin, unreal.Vector(-normal[0], -normal[1], -normal[2]))
            line.append(hit.ray_parameter - lift if hit.hit else None)
        depths.append(line)

    log("panel centre=({:.1f}, {:.1f}, {:.1f}) normal=({:.4f}, {:.4f}, {:.4f}) "
        "tilt={:.1f} deg".format(
            centre[0], centre[1], centre[2], normal[0], normal[1], normal[2],
            90.0 - math.degrees(math.asin(normal[2]))))
    return centre, normal, u, v, depths, span_u, span_v


def find_wells(depths, span_u, span_v):
    step_u = (span_u[1] - span_u[0]) / SAMPLES_U
    step_v = (span_v[1] - span_v[0]) / SAMPLES_V
    inside = [[depths[r][c] is not None and depths[r][c] >= WELL_DEPTH_THRESHOLD
               for c in range(SAMPLES_U)] for r in range(SAMPLES_V)]
    seen = [[False] * SAMPLES_U for _ in range(SAMPLES_V)]

    wells = []
    for row in range(SAMPLES_V):
        for col in range(SAMPLES_U):
            if not inside[row][col] or seen[row][col]:
                continue
            stack = [(row, col)]
            seen[row][col] = True
            members = []
            while stack:
                r, cc = stack.pop()
                members.append((r, cc))
                for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nr, nc = r + dr, cc + dc
                    if (0 <= nr < SAMPLES_V and 0 <= nc < SAMPLES_U
                            and inside[nr][nc] and not seen[nr][nc]):
                        seen[nr][nc] = True
                        stack.append((nr, nc))
            if len(members) < MIN_CLUSTER_SAMPLES:
                continue
            us = [span_u[0] + (cc + 0.5) * step_u for _, cc in members]
            vs = [span_v[0] + (r + 0.5) * step_v for r, _ in members]
            area = len(members) * step_u * step_v
            wells.append({
                "u": sum(us) / len(us),
                "v": sum(vs) / len(vs),
                "radius": math.sqrt(area / math.pi),
                "depth": max(depths[r][cc] for r, cc in members),
            })

    # Reading order: top row of the pan first, then screen-left to screen-right. The camera
    # looks along +Y, so screen-right is world -X, which is descending u.
    wells.sort(key=lambda w: -w["v"])
    rows = []
    for well in wells:
        if rows and abs(rows[-1][0]["v"] - well["v"]) <= well["radius"]:
            rows[-1].append(well)
        else:
            rows.append([well])
    for band in rows:
        band.sort(key=lambda w: -w["u"])
    return rows


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = find_board(subsystem)
    if actor is None:
        unreal.log_error("[PanLayout] no actor tagged " + BOARD_TAG)
        return

    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    panel = component.get_world_transform()
    centre, normal, u, v, depths, span_u, span_v = sample_panel(component)
    rows = find_wells(depths, span_u, span_v)
    total = sum(len(band) for band in rows)
    log("found {} well(s) across {} row(s): {}".format(
        total, len(rows), [len(band) for band in rows]))

    # Cell +Z must point out of the panel. Roll about world X tilts up within the YZ plane,
    # which is exactly how this pan leans, so solve the roll from the fitted normal.
    world_roll = math.degrees(math.atan2(-normal[1], normal[2]))
    world_rotation = unreal.Rotator(roll=world_roll, pitch=0.0, yaw=0.0)
    local_rotation = unreal.MathLibrary.inverse_transform_rotation(panel, world_rotation)
    quat = rotator_to_quat(local_rotation.pitch, local_rotation.yaw, local_rotation.roll)
    log("world roll={:.2f} -> local rotator (pitch {:.2f}, yaw {:.2f}, roll {:.2f})".format(
        world_roll, local_rotation.pitch, local_rotation.yaw, local_rotation.roll))

    lines = ["Name,CellIndex,Transform,VisualRadius"]
    index = 0
    worst = 0.0
    for band in rows:
        for well in band:
            floor = (
                centre[0] + u[0] * well["u"] + v[0] * well["v"] - normal[0] * well["depth"],
                centre[1] + u[1] * well["u"] + v[1] * well["v"] - normal[1] * well["depth"],
                centre[2] + u[2] * well["u"] + v[2] * well["v"] - normal[2] * well["depth"],
            )
            world = unreal.Vector(*floor)
            local = unreal.MathLibrary.inverse_transform_location(panel, world)
            # Guard the local-space round trip, since every cell position depends on it.
            back = unreal.MathLibrary.transform_location(panel, local)
            worst = max(worst, max(abs(back.x - world.x), abs(back.y - world.y),
                                   abs(back.z - world.z)))
            radius = well["radius"] * RADIUS_INSET
            lines.append(
                'Cell_{:02d},{},"(Rotation=(X={:.6f},Y={:.6f},Z={:.6f},W={:.6f}),'
                'Translation=(X={:.6f},Y={:.6f},Z={:.6f}),'
                'Scale3D=(X=1.000000,Y=1.000000,Z=1.000000))",{:.2f}'.format(
                    index, index, quat[0], quat[1], quat[2], quat[3],
                    local.x, local.y, local.z, radius))
            log("  cell {:>2} world=({:>8.1f}, {:>8.1f}, {:>8.1f}) local=({:>8.2f}, "
                "{:>8.2f}, {:>8.2f}) r={:.1f}".format(
                    index, world.x, world.y, world.z, local.x, local.y, local.z, radius))
            index += 1
    log("worst local/world round-trip error = {:.4f}".format(worst))

    table = unreal.load_asset(TABLE_PATH)
    problems = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(
        table, "\n".join(lines))
    log("fill_data_table_from_csv_string -> {}".format(problems))
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    log("wrote {} row(s) to {}".format(index, TABLE_PATH))


main()
