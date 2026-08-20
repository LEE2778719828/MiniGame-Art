# Locates the moments inside the action clips that are worth marking with a notify.
#
# A notify only earns its keep when the moment it marks is INSIDE the clip and cannot be derived
# at the call site. Anything at t=0 is already known the instant PlayHeroAction runs, so marking
# it adds an asset dependency for nothing. The question this script answers is therefore not
# "when does the jump leave the ground" but "does it leave the ground later than frame 0".
#
# Jump   - hips height relative to frame 0. A wind-up dips first, so takeoff is the lowest point
#          before the rise; without a dip the clip already starts airborne.
# Slash  - weapon hand speed in component space. A blade trail needs a start and an end, which
#          are the frames where speed crosses in and out of a fraction of its peak.
#
# Bone sampling note (same trap as MeasureAnimAnchors.py): get_bone_pose_for_time returns the
# transform relative to the PARENT, so anything below the pelvis looks motionless. The hand has
# to be composed up its parent chain to reach component space.
#
# Run from the editor: py "D:/myProject/MiniGame/MiniGame/Tools/ProbeAnimNotifyPoints.py"

import unreal

BASE = "/Game/Night/Character/Anims/"

CHAINS = {
    "Hips": ["Hips"],
    "RightHand": ["Hips", "Spine", "Spine1", "Spine2", "RightShoulder", "RightArm",
                  "RightForeArm", "RightHand"],
    "LeftToe": ["Hips", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase"],
    "RightToe": ["Hips", "RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"],
}

# A toe this far above its resting height counts as off the ground. Small enough to catch the
# actual break of contact, large enough to ignore rig noise in the planted frames.
AIRBORNE_TOLERANCE_CM = 2.0

# Fraction of peak hand speed that counts as "the blade is swinging", for trail start / end.
TRAIL_THRESHOLD = 0.25


def log(msg):
    unreal.log_warning("[NotifyProbe] " + msg)


def load(name):
    asset = unreal.EditorAssetLibrary.load_asset("{}{}.{}".format(BASE, name, name))
    if not asset:
        log("missing clip: " + name)
    return asset


def component_space_pos(anim, bone, time):
    accumulated = None
    for link in CHAINS[bone]:
        local = unreal.AnimationLibrary.get_bone_pose_for_time(anim, link, time, False)
        accumulated = local if accumulated is None \
            else unreal.MathLibrary.compose_transforms(local, accumulated)
    return accumulated.translation


def frame_times(anim):
    """One sample per authored frame, so reported times land on real keys."""
    length = anim.get_play_length()
    count = int(round(length * 30.0))
    return [(index, length * index / float(count)) for index in range(count + 1)]


def report_jump(name):
    anim = load(name)
    if not anim:
        return

    log("=== {} : hips and toe height per frame (30fps, {:.0f}ms) ===".format(
        name, anim.get_play_length() * 1000.0))

    samples = []
    for index, time in frame_times(anim):
        hips = component_space_pos(anim, "Hips", time).z
        toe = min(component_space_pos(anim, "LeftToe", time).z,
                  component_space_pos(anim, "RightToe", time).z)
        samples.append((index, time, hips, toe))

    # Both baselines come from frame 0, where the character is standing planted. The clip's global
    # toe minimum is NOT the ground: the last frame points the toe down below the planted pose,
    # and using it as the reference makes every frame look airborne.
    hips_base = samples[0][2]
    toe_base = samples[0][3]
    for index, time, hips, toe in samples:
        log("  f{:<3d} {:5.0f}ms  hips {:+7.2f}cm  toe {:+7.2f}cm{}".format(
            index, time * 1000.0, hips - hips_base, toe - toe_base,
            "  AIRBORNE" if toe - toe_base > AIRBORNE_TOLERANCE_CM else ""))

    apex = max(samples, key=lambda sample: sample[2])
    # The global minimum is the landing squash, so the wind-up has to be searched before the apex.
    before_apex = [sample for sample in samples if sample[0] < apex[0]]
    crouch = min(before_apex, key=lambda sample: sample[2]) if before_apex else samples[0]

    airborne = [sample for sample in samples
                if sample[3] - toe_base > AIRBORNE_TOLERANCE_CM]

    log("  apex   f{} at {:.0f}ms ({:+.2f}cm)".format(apex[0], apex[1] * 1000.0,
                                                      apex[2] - hips_base))
    log("  crouch f{} at {:.0f}ms ({:+.2f}cm) - deepest point BEFORE the apex".format(
        crouch[0], crouch[1] * 1000.0, crouch[2] - hips_base))

    if not airborne:
        log("  -> toes never leave the ground: the clip is not a real jump arc")
        return

    log("  toes off the ground f{} ({:.0f}ms) through f{} ({:.0f}ms)".format(
        airborne[0][0], airborne[0][1] * 1000.0,
        airborne[-1][0], airborne[-1][1] * 1000.0))

    if airborne[0][0] <= 1:
        log("  -> takeoff is at frame 0: a takeoff notify would sit at t=0 and tell us nothing "
            "that PlayHeroAction does not already know at the call site")
    else:
        log("  -> takeoff happens {:.0f}ms into the clip, after a wind-up: not derivable at the "
            "call site, so it earns its own notify".format(airborne[0][1] * 1000.0))


def report_slash(name):
    anim = load(name)
    if not anim:
        return

    log("=== {} : weapon hand speed per frame ===".format(name))

    samples = [(index, time, component_space_pos(anim, "RightHand", time))
               for index, time in frame_times(anim)]

    speeds = []
    for slot in range(1, len(samples)):
        previous_index, previous_time, previous_pos = samples[slot - 1]
        index, time, position = samples[slot]
        delta = time - previous_time
        distance = (position - previous_pos).length()
        speeds.append((index, time, distance / delta if delta > 0.0 else 0.0))

    peak = max(speed for _, _, speed in speeds)
    for index, time, speed in speeds:
        marker = "#" * int(round(speed / peak * 30.0))
        log("  f{:<3d} {:5.0f}ms  {:7.0f}cm/s  {}".format(index, time * 1000.0, speed, marker))

    swinging = [(index, time) for index, time, speed in speeds
                if speed >= peak * TRAIL_THRESHOLD]
    if swinging:
        log("  swing spans f{} ({:.0f}ms) to f{} ({:.0f}ms) at {:.0f}% of peak".format(
            swinging[0][0], swinging[0][1] * 1000.0,
            swinging[-1][0], swinging[-1][1] * 1000.0,
            TRAIL_THRESHOLD * 100.0))


# The _fast variants are not wired up right now, but JumpAnim / AttackAnim on BP_NightHero can be
# pointed at them, and a swap would silently lose every notify. Measure them too so the marks can
# be placed before that happens rather than after someone wonders where the effects went.
for clip_name in ("Jump_noknife2", "Jump_noknife2_fast"):
    report_jump(clip_name)

for clip_name in ("Slash", "Slash_fast_Armature_Armature_kan"):
    report_slash(clip_name)

log("done")
