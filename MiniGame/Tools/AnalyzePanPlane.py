"""Fit the SDay.Board surface plane and look for hole depressions inside it.

The pan is a steeply tilted ellipse, so a global "highest point" test cannot separate the
slope from the holes. This fits a least-squares plane to the sampled surface, reports the
plane basis the presenter needs, and clusters whatever sits below that plane.
"""

import math

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
BOARD_TAG = "SDay.Board"
COLUMNS = 61
ROWS = 47
DEPRESSION_THRESHOLD = 8.0


def log(message):
    unreal.log("[PanPlane] " + message)


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = None
    for candidate in subsystem.get_all_level_actors():
        if BOARD_TAG in [str(tag) for tag in candidate.get_editor_property("tags")]:
            actor = candidate
            break
    if actor is None:
        unreal.log_error("[PanPlane] no actor tagged " + BOARD_TAG)
        return

    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = unreal.DynamicMesh()
    mesh, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        component.static_mesh, mesh,
        unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())
    mesh = unreal.GeometryScript_MeshTransforms.transform_mesh(
        mesh, component.get_world_transform())

    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    lo, hi = box.min, box.max
    bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh)
    if isinstance(bvh, tuple):
        bvh = bvh[-1]
    options = unreal.GeometryScriptSpatialQueryOptions()
    start_z = hi.z + 50.0
    down = unreal.Vector(0.0, 0.0, -1.0)

    samples = []
    grid = []
    for row in range(ROWS):
        y = lo.y + (hi.y - lo.y) * (row + 0.5) / ROWS
        line = []
        for col in range(COLUMNS):
            x = lo.x + (hi.x - lo.x) * (col + 0.5) / COLUMNS
            hit = unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
                mesh, bvh, unreal.Vector(x, y, start_z), down, options)[1]
            if hit.hit:
                point = (x, y, hit.hit_position.z)
                samples.append(point)
                line.append(point)
            else:
                line.append(None)
        grid.append(line)

    # Least-squares fit of z = a*x + b*y + c over the sampled surface.
    n = float(len(samples))
    sx = sum(p[0] for p in samples)
    sy = sum(p[1] for p in samples)
    sz = sum(p[2] for p in samples)
    sxx = sum(p[0] * p[0] for p in samples)
    syy = sum(p[1] * p[1] for p in samples)
    sxy = sum(p[0] * p[1] for p in samples)
    sxz = sum(p[0] * p[2] for p in samples)
    syz = sum(p[1] * p[2] for p in samples)
    m = [[sxx, sxy, sx], [sxy, syy, sy], [sx, sy, n]]
    rhs = [sxz, syz, sz]
    for i in range(3):
        pivot = max(range(i, 3), key=lambda r: abs(m[r][i]))
        m[i], m[pivot] = m[pivot], m[i]
        rhs[i], rhs[pivot] = rhs[pivot], rhs[i]
        for r in range(i + 1, 3):
            factor = m[r][i] / m[i][i]
            for c in range(i, 3):
                m[r][c] -= factor * m[i][c]
            rhs[r] -= factor * rhs[i]
    coeff = [0.0, 0.0, 0.0]
    for i in reversed(range(3)):
        total = rhs[i] - sum(m[i][c] * coeff[c] for c in range(i + 1, 3))
        coeff[i] = total / m[i][i]
    a, b, c = coeff
    log("plane z = {:.5f}*x + {:.5f}*y + {:.2f} from {} samples".format(a, b, c, len(samples)))

    length = math.sqrt(a * a + b * b + 1.0)
    normal = (-a / length, -b / length, 1.0 / length)
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, normal[2]))))
    log("plane normal = ({:.4f}, {:.4f}, {:.4f}); surface tilt from horizontal = {:.1f} deg".format(
        normal[0], normal[1], normal[2], 90.0 - pitch))
    log("a camera aimed straight at the pan would use pitch = {:.1f}, yaw = {:.1f}".format(
        -pitch, math.degrees(math.atan2(-normal[1], -normal[0]))))

    residuals = [[None if grid[r][cc] is None else
                  (a * grid[r][cc][0] + b * grid[r][cc][1] + c) - grid[r][cc][2]
                  for cc in range(COLUMNS)] for r in range(ROWS)]
    flat = [v for line in residuals for v in line if v is not None]
    flat.sort()
    log("residual min={:.1f} p50={:.1f} p90={:.1f} p99={:.1f} max={:.1f}".format(
        flat[0], flat[len(flat) // 2], flat[int(len(flat) * 0.9)],
        flat[int(len(flat) * 0.99)], flat[-1]))

    for row in reversed(range(ROWS)):
        line = ""
        for col in range(COLUMNS):
            value = residuals[row][col]
            if value is None:
                line += " "
            elif value >= 40:
                line += "@"
            elif value >= DEPRESSION_THRESHOLD:
                line += "o"
            elif value <= -DEPRESSION_THRESHOLD:
                line += "^"
            else:
                line += "."
        log("|{}|".format(line))

    # Plane basis: u follows world X (screen horizontal), v runs up the slope.
    ux, uy, uz = 1.0, 0.0, -a / 1.0
    ulen = math.sqrt(ux * ux + uy * uy + uz * uz)
    u = (ux / ulen, uy / ulen, uz / ulen)
    v = (normal[1] * u[2] - normal[2] * u[1],
         normal[2] * u[0] - normal[0] * u[2],
         normal[0] * u[1] - normal[1] * u[0])
    centre = (sx / n, sy / n, sz / n)
    log("plane centre = ({:.1f}, {:.1f}, {:.1f})".format(*centre))
    log("basis u = ({:.4f}, {:.4f}, {:.4f})  v = ({:.4f}, {:.4f}, {:.4f})".format(*(u + v)))

    us = [sum((p[i] - centre[i]) * u[i] for i in range(3)) for p in samples]
    vs = [sum((p[i] - centre[i]) * v[i] for i in range(3)) for p in samples]
    log("in-plane extent u {:.1f}..{:.1f} ({:.1f} wide)  v {:.1f}..{:.1f} ({:.1f} tall)".format(
        min(us), max(us), max(us) - min(us), min(vs), max(vs), max(vs) - min(vs)))


main()
