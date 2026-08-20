"""Extract the pan's hole centres in both world space and pan-plane space.

The pan is a 2.5D panel tilted 55 degrees toward the camera, so sampling straight down
foreshortens it. This samples along the pan's own normal instead, which keeps the holes
circular and makes clustering reliable.
"""

import math

import unreal


LEVEL_PATH = "/Game/Day/Maps/L_S_DayWhitebox"
BOARD_TAG = "SDay.Board"
# Plane-space sampling grid; ~7 units per sample over an 830 x 1080 face.
SAMPLES_U = 120
SAMPLES_V = 156
MIN_CLUSTER_SAMPLES = 100
# Well floors sit ~5 below the fitted plane, the walls between them rise above it.
WELL_DEPTH_THRESHOLD = 4.0


def log(message):
    unreal.log("[PanHoles] " + message)


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


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = None
    for candidate in subsystem.get_all_level_actors():
        if BOARD_TAG in [str(tag) for tag in candidate.get_editor_property("tags")]:
            actor = candidate
            break
    if actor is None:
        unreal.log_error("[PanHoles] no actor tagged " + BOARD_TAG)
        return

    component = actor.get_component_by_class(unreal.StaticMeshComponent)
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

    # Coarse pass straight down to fit the face plane.
    coarse = []
    for row in range(47):
        y = lo.y + (hi.y - lo.y) * (row + 0.5) / 47
        for col in range(61):
            x = lo.x + (hi.x - lo.x) * (col + 0.5) / 61
            hit = unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
                mesh, bvh, unreal.Vector(x, y, hi.z + 50.0),
                unreal.Vector(0.0, 0.0, -1.0), options)[1]
            if hit.hit:
                coarse.append((x, y, hit.hit_position.z))

    n = float(len(coarse))
    sums = {key: 0.0 for key in ("x", "y", "z", "xx", "yy", "xy", "xz", "yz")}
    for px, py, pz in coarse:
        sums["x"] += px
        sums["y"] += py
        sums["z"] += pz
        sums["xx"] += px * px
        sums["yy"] += py * py
        sums["xy"] += px * py
        sums["xz"] += px * pz
        sums["yz"] += py * pz
    a, b, c = solve3(
        [[sums["xx"], sums["xy"], sums["x"]],
         [sums["xy"], sums["yy"], sums["y"]],
         [sums["x"], sums["y"], n]],
        [sums["xz"], sums["yz"], sums["z"]])

    length = math.sqrt(a * a + b * b + 1.0)
    normal = (-a / length, -b / length, 1.0 / length)
    centre = (sums["x"] / n, sums["y"] / n, sums["z"] / n)
    u = (1.0, 0.0, -a)
    ulen = math.sqrt(dot(u, u))
    u = (u[0] / ulen, u[1] / ulen, u[2] / ulen)
    v = (normal[1] * u[2] - normal[2] * u[1],
         normal[2] * u[0] - normal[0] * u[2],
         normal[0] * u[1] - normal[1] * u[0])
    log("plane centre=({:.1f}, {:.1f}, {:.1f}) normal=({:.4f}, {:.4f}, {:.4f})".format(
        centre[0], centre[1], centre[2], normal[0], normal[1], normal[2]))
    log("basis u=({:.4f}, {:.4f}, {:.4f}) v=({:.4f}, {:.4f}, {:.4f})".format(*(u + v)))

    span_u = [min(dot((p[0] - centre[0], p[1] - centre[1], p[2] - centre[2]), u) for p in coarse),
              max(dot((p[0] - centre[0], p[1] - centre[1], p[2] - centre[2]), u) for p in coarse)]
    span_v = [min(dot((p[0] - centre[0], p[1] - centre[1], p[2] - centre[2]), v) for p in coarse),
              max(dot((p[0] - centre[0], p[1] - centre[1], p[2] - centre[2]), v) for p in coarse)]
    log("plane span u {:.1f}..{:.1f} v {:.1f}..{:.1f}".format(
        span_u[0], span_u[1], span_v[0], span_v[1]))

    # Fine pass along the plane normal.
    lift = 200.0
    depths = []
    for row in range(SAMPLES_V):
        cv = span_v[0] + (span_v[1] - span_v[0]) * (row + 0.5) / SAMPLES_V
        line = []
        for col in range(SAMPLES_U):
            cu = span_u[0] + (span_u[1] - span_u[0]) * (col + 0.5) / SAMPLES_U
            base = (centre[0] + u[0] * cu + v[0] * cv,
                    centre[1] + u[1] * cu + v[1] * cv,
                    centre[2] + u[2] * cu + v[2] * cv)
            origin = unreal.Vector(base[0] + normal[0] * lift,
                                   base[1] + normal[1] * lift,
                                   base[2] + normal[2] * lift)
            hit = unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
                mesh, bvh, origin,
                unreal.Vector(-normal[0], -normal[1], -normal[2]), options)[1]
            line.append(hit.ray_parameter - lift if hit.hit else None)
        depths.append(line)

    flat = sorted(d for line in depths for d in line if d is not None)
    log("depth below plane: min={:.1f} p50={:.1f} p95={:.1f} max={:.1f} ({} hits)".format(
        flat[0], flat[len(flat) // 2], flat[int(len(flat) * 0.95)], flat[-1], len(flat)))
    # The wells are shallow and separated by raised walls, so the depth field is bimodal:
    # walls sit above the fitted plane, well floors about 5 below it.
    threshold = WELL_DEPTH_THRESHOLD
    log("hole threshold = {:.1f}".format(threshold))

    inside = [[depths[r][c] is not None and depths[r][c] >= threshold
               for c in range(SAMPLES_U)] for r in range(SAMPLES_V)]
    seen = [[False] * SAMPLES_U for _ in range(SAMPLES_V)]
    step_u = (span_u[1] - span_u[0]) / SAMPLES_U
    step_v = (span_v[1] - span_v[0]) / SAMPLES_V

    holes = []
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
            holes.append({
                "u": sum(us) / len(us),
                "v": sum(vs) / len(vs),
                "width": max(us) - min(us) + step_u,
                "height": max(vs) - min(vs) + step_v,
                "depth": max(depths[r][cc] for r, cc in members),
                "samples": len(members),
            })

    # Group into rows so the reading order matches how a player scans the pan.
    holes.sort(key=lambda h: -h["v"])
    rows = []
    for hole in holes:
        if rows and abs(rows[-1][0]["v"] - hole["v"]) <= max(hole["height"], 40.0) * 0.6:
            rows[-1].append(hole)
        else:
            rows.append([hole])
    for band in rows:
        band.sort(key=lambda h: h["u"])

    log("found {} hole(s) in {} row(s)".format(len(holes), len(rows)))

    # Overlay the detected centres on the depth field so the extraction can be eyeballed.
    marks = {}
    alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    ordered = [hole for band in rows for hole in band]
    for order, hole in enumerate(ordered):
        col = int((hole["u"] - span_u[0]) / step_u)
        row = int((hole["v"] - span_v[0]) / step_v)
        marks[(row // 2, col)] = alphabet[order % len(alphabet)]

    glyphs = ((-3.0, "#"), (0.0, "+"), (2.5, "-"), (4.5, "."), (7.0, "o"))
    for row in reversed(range(0, SAMPLES_V, 2)):
        line = ""
        for col in range(SAMPLES_U):
            mark = marks.get((row // 2, col))
            if mark is not None:
                line += mark
                continue
            depth = depths[row][col]
            if depth is None:
                line += " "
                continue
            glyph = "@"
            for limit, symbol in glyphs:
                if depth <= limit:
                    glyph = symbol
                    break
            line += glyph
        log("|{}|".format(line))

    index = 0
    for band_index, band in enumerate(rows):
        for hole in band:
            world = (centre[0] + u[0] * hole["u"] + v[0] * hole["v"],
                     centre[1] + u[1] * hole["u"] + v[1] * hole["v"],
                     centre[2] + u[2] * hole["u"] + v[2] * hole["v"])
            log("  hole {:>2} row {} u={:>8.1f} v={:>8.1f} d={:>5.1f} size={:>5.1f}x{:<5.1f} "
                "world=({:>8.1f}, {:>8.1f}, {:>8.1f})".format(
                    index, band_index, hole["u"], hole["v"], hole["depth"],
                    hole["width"], hole["height"], world[0], world[1], world[2]))
            index += 1


main()
