# Dump local bounds for every mesh in the canguan art folder.
#
# If the FBX baked its scene transforms, each origin encodes the prop's authored position,
# which means one shared actor transform reassembles the artist's layout. Widely spread
# origins prove baking; origins clustered near zero mean every prop must be placed by hand.

import unreal


ART_DIR = "/Game/Day/Art/canguan"


def main():
    paths = sorted(unreal.EditorAssetLibrary.list_assets(ART_DIR, recursive=False))

    rows = []
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3

    for path in paths:
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue

        bounds = asset.get_bounds()
        origin = bounds.origin
        extent = bounds.box_extent
        rows.append(
            "{:<14} origin=({:>9.2f},{:>9.2f},{:>9.2f}) extent=({:>8.2f},{:>8.2f},{:>8.2f})".format(
                asset.get_name(),
                origin.x, origin.y, origin.z,
                extent.x, extent.y, extent.z,
            )
        )
        for axis, (o, e) in enumerate(
            ((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))
        ):
            lo[axis] = min(lo[axis], o - e)
            hi[axis] = max(hi[axis], o + e)

    for row in rows:
        unreal.log("[Bounds] " + row)

    unreal.log(
        "[Bounds] SCENE x {:.1f}..{:.1f}  y {:.1f}..{:.1f}  z {:.1f}..{:.1f}  size=({:.1f}, {:.1f}, {:.1f})".format(
            lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
            hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2],
        )
    )


main()
