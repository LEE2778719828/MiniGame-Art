"""Sample the SDay.Board mesh from above to locate its top surface and its holes.

The plate in the concept art is a hole-punched pan, and the runtime cells must land in
those holes. Instead of eyeballing the model, this raycasts a grid straight down onto the
placed actor and reports a height map plus the centre of every depression it finds.
"""

import unreal


LEVEL_PATH = "/Game/Day/Maps/L_S_DayWhitebox"
BOARD_TAG = "SDay.Board"
COLUMNS = 61
ROWS = 47

# A cell counts as inside a hole when it sits this far below the pan's rim plane.
HOLE_DEPTH_THRESHOLD = 12.0
# Rim plane = the highest sampled hit, minus tolerance for shading/bevel noise.
RIM_TOLERANCE = 6.0


def log(message):
    unreal.log("[PanProbe] " + message)


def find_board():
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in subsystem.get_all_level_actors():
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        if BOARD_TAG in tags:
            return actor
    return None


def world_mesh(actor):
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    static_mesh = component.static_mesh
    mesh = unreal.DynamicMesh()
    mesh, _ = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        static_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(),
        unreal.GeometryScriptMeshReadLOD())
    mesh = unreal.GeometryScript_MeshTransforms.transform_mesh(
        mesh, component.get_world_transform())
    return mesh


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actor = find_board()
    if actor is None:
        unreal.log_error("[PanProbe] no actor tagged " + BOARD_TAG)
        return

    mesh = world_mesh(actor)
    box = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    if isinstance(box, tuple):
        box = box[-1]
    lo, hi = box.min, box.max
    log("world box x {:.1f}..{:.1f} y {:.1f}..{:.1f} z {:.1f}..{:.1f}".format(
        lo.x, hi.x, lo.y, hi.y, lo.z, hi.z))

    bvh = unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh)
    if isinstance(bvh, tuple):
        bvh = bvh[-1]
    options = unreal.GeometryScriptSpatialQueryOptions()
    start_z = hi.z + 50.0
    down = unreal.Vector(0.0, 0.0, -1.0)

    heights = []
    logged_shape = False
    for row in range(ROWS):
        y = lo.y + (hi.y - lo.y) * (row + 0.5) / ROWS
        line = []
        for col in range(COLUMNS):
            x = lo.x + (hi.x - lo.x) * (col + 0.5) / COLUMNS
            result = unreal.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(
                mesh, bvh, unreal.Vector(x, y, start_z), down, options)
            hit_result = result[1]
            if not logged_shape:
                log("ray hit fields = {}".format(
                    [n for n in dir(hit_result) if not n.startswith("_")]))
                logged_shape = True
            line.append(hit_result.hit_position.z if hit_result.hit else None)
        heights.append(line)

    hits = [z for line in heights for z in line if z is not None]
    if not hits:
        unreal.log_error("[PanProbe] no ray hits")
        return
    rim = max(hits)
    log("rim z = {:.1f} (of {} hits)".format(rim, len(hits)))

    glyphs = {0: "#", 1: "=", 2: "-", 3: ":", 4: "."}
    for row in reversed(range(ROWS)):
        line = ""
        for col in range(COLUMNS):
            z = heights[row][col]
            if z is None:
                line += " "
                continue
            drop = rim - z
            if drop <= RIM_TOLERANCE:
                line += glyphs[0]
            elif drop <= 40:
                line += glyphs[1]
            elif drop <= 120:
                line += glyphs[2]
            elif drop <= 400:
                line += glyphs[3]
            else:
                line += glyphs[4]
        log("|{}|".format(line))

    # Flood fill the cells that sit below the rim to recover individual holes.
    low = [[heights[r][c] is not None and (rim - heights[r][c]) >= HOLE_DEPTH_THRESHOLD
            and (rim - heights[r][c]) <= 400
            for c in range(COLUMNS)] for r in range(ROWS)]
    seen = [[False] * COLUMNS for _ in range(ROWS)]
    clusters = []
    for row in range(ROWS):
        for col in range(COLUMNS):
            if not low[row][col] or seen[row][col]:
                continue
            stack = [(row, col)]
            seen[row][col] = True
            members = []
            while stack:
                r, c = stack.pop()
                members.append((r, c))
                for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < ROWS and 0 <= nc < COLUMNS and low[nr][nc] and not seen[nr][nc]:
                        seen[nr][nc] = True
                        stack.append((nr, nc))
            clusters.append(members)

    step_x = (hi.x - lo.x) / COLUMNS
    step_y = (hi.y - lo.y) / ROWS
    clusters.sort(key=len, reverse=True)
    log("found {} depression cluster(s)".format(len(clusters)))
    for index, members in enumerate(clusters):
        if len(members) < 3:
            continue
        xs = [lo.x + (c + 0.5) * step_x for _, c in members]
        ys = [lo.y + (r + 0.5) * step_y for r, _ in members]
        zs = [heights[r][c] for r, c in members]
        log("  hole {:>2} cells={:>3} centre=({:>7.1f}, {:>7.1f}) floorZ={:>7.1f} "
            "spanX={:>6.1f} spanY={:>6.1f}".format(
                index, len(members),
                sum(xs) / len(xs), sum(ys) / len(ys), max(zs),
                max(xs) - min(xs) + step_x, max(ys) - min(ys) + step_y))


main()
