# Measures the sync anchors of the hero action clips, so animation play rates can be derived from
# clip content instead of guessed.
#
# Jump and attack anchor differently:
#   jump   - the whole airborne arc maps to the traversal: takeoff = leaving the stone,
#            landing = arriving. Anything after landing is settle and belongs in the grace window.
#            Anchor = the frame where the hips return to their starting height.
#   attack - only the contact instant needs to land on arrival; the recovery may trail past it.
#            Anchor = the frame of peak hand speed.
#
# Note on bone sampling: get_bone_pose_for_time returns the bone's transform relative to its
# PARENT, so for anything below the pelvis the translation is just a constant bone length and
# looks motionless. Hand speed therefore has to be measured in component space, which means
# walking the parent chain and composing the local transforms.
#
# Run from the editor: py "Tools/MeasureAnimAnchors.py"

import math

import unreal

BASE = "/Game/Night/Character/Anims/"

# Traversal budgets that the clips have to line up with, from DA_Course:
# JumpGapCm 420 / AdvanceSpeed 1400 = 300ms, KillGapCm 160 / 1400 = 114ms.
JUMP_TRAVERSAL_MS = 300.0
ATTACK_TRAVERSAL_MS = 114.0

JUMP_CLIPS = ["Jump", "Jump_noknife2", "Jump_noknife2_fast"]
ATTACK_CLIPS = ["Slash", "Slash_fast_Armature_Armature_kan"]

# Pairs to test for "is the short one just the long one retimed?". If the poses line up at
# matching fractions of each clip, a play-rate change reproduces one from the other and we do
# not need both assets; if they diverge, the artist retimed the action and both are worth keeping.
COMPARE_PAIRS = [
    ("Slash", "Slash_fast_Armature_Armature_kan"),
    ("Jump_noknife2", "Jump_noknife2_fast"),
]
RETIME_POS_TOLERANCE_CM = 2.0
RETIME_ANGLE_TOLERANCE_DEG = 5.0
COMPARE_BONES = ("Hips", "RightHand", "LeftHand")

# Parent chains for SK_Hero_Skeleton, root first. Needed to reach component space.
CHAINS = {
    "Hips": ["Hips"],
    "RightHand": ["Hips", "Spine", "Spine1", "Spine2", "RightShoulder", "RightArm",
                  "RightForeArm", "RightHand"],
    "LeftHand": ["Hips", "Spine", "Spine1", "Spine2", "LeftShoulder", "LeftArm",
                 "LeftForeArm", "LeftHand"],
}

SAMPLES = 40
# Hips counts as back "on the ground" once within this many cm of its starting height.
LANDED_TOLERANCE_CM = 1.0
# The blade rides KnifeSocket on RightHand, so contact is that hand's fastest moment. Picking
# whichever hand happens to move faster would sometimes anchor on the off hand's follow-through.
WEAPON_BONE = "RightHand"


def log(msg):
    unreal.log_warning(msg)


def load(name):
    asset = unreal.EditorAssetLibrary.load_asset("{}{}.{}".format(BASE, name, name))
    if not asset:
        log("missing clip: " + name)
    return asset


def component_space_pos(anim, bone, t):
    accumulated = None
    for link in CHAINS[bone]:
        local = unreal.AnimationLibrary.get_bone_pose_for_time(anim, link, t, False)
        accumulated = local if accumulated is None \
            else unreal.MathLibrary.compose_transforms(local, accumulated)
    return accumulated.translation


def sample(anim, bone):
    """Returns [(time, component-space FVector)] sampled evenly across the clip."""
    length = anim.get_play_length()
    return [(length * i / float(SAMPLES),
             component_space_pos(anim, bone, length * i / float(SAMPLES)))
            for i in range(SAMPLES + 1)]


def speed_curve(samples):
    """Finite-difference speed, timestamped at the midpoint of each interval."""
    out = []
    for i in range(1, len(samples)):
        t0, p0 = samples[i - 1]
        t1, p1 = samples[i]
        dt = t1 - t0
        dist = ((p1.x - p0.x) ** 2 + (p1.y - p0.y) ** 2 + (p1.z - p0.z) ** 2) ** 0.5
        out.append(((t0 + t1) * 0.5, dist / dt if dt > 0 else 0.0))
    return out


def suggest_rate(anchor_ms, traversal_ms):
    if traversal_ms <= 0.0:
        return
    rate = anchor_ms / traversal_ms
    log("  -> anchor {:.0f}ms vs traversal {:.0f}ms: play rate {:.2f} to align "
        "({} by {:.0f}ms at rate 1.0)".format(
            anchor_ms, traversal_ms, rate,
            "early" if anchor_ms < traversal_ms else "late",
            abs(anchor_ms - traversal_ms)))


def report_jump(anim):
    length_ms = anim.get_play_length() * 1000.0
    log("=== {}  len={:.0f}ms ===".format(anim.get_name(), length_ms))

    samples = sample(anim, "Hips")
    z_start = samples[0][1].z
    apex_t, apex_z = max(((t, p.z) for t, p in samples), key=lambda pair: pair[1])
    log("  hips rise {:.1f}cm, apex at {:.0f}ms".format(apex_z - z_start, apex_t * 1000))

    landing_ms = None
    for t, p in samples:
        if t > apex_t and p.z - z_start <= LANDED_TOLERANCE_CM:
            landing_ms = t * 1000.0
            break

    if landing_ms is None:
        log("  landing: hips never return to start height -> clip ends airborne")
        return

    log("  landing at {:.0f}ms = {:.0f}% of clip, leaving {:.0f}ms of settle after it".format(
        landing_ms, landing_ms / length_ms * 100, length_ms - landing_ms))
    suggest_rate(landing_ms, JUMP_TRAVERSAL_MS)


def report_attack(anim):
    length_ms = anim.get_play_length() * 1000.0
    log("=== {}  len={:.0f}ms ===".format(anim.get_name(), length_ms))

    anchor_ms = None
    for bone in ("RightHand", "LeftHand"):
        curve = speed_curve(sample(anim, bone))
        peak_t, peak_v = max(curve, key=lambda pair: pair[1])
        log("  {} peak {:.0f}cm/s at {:.0f}ms = {:.0f}% of clip".format(
            bone, peak_v, peak_t * 1000, peak_t / anim.get_play_length() * 100))
        for t, v in curve[::4]:
            log("    {:5.0f}ms  {:8.0f}cm/s".format(t * 1000, v))
        if bone == WEAPON_BONE:
            anchor_ms = peak_t * 1000.0

    if anchor_ms is not None:
        log("  contact anchor: {} (weapon hand) at {:.0f}ms".format(WEAPON_BONE, anchor_ms))
        suggest_rate(anchor_ms, ATTACK_TRAVERSAL_MS)


def quat_angle_deg(a, b):
    """Shortest rotation between two orientations, in degrees."""
    dot = abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
    return math.degrees(2.0 * math.acos(min(1.0, dot)))


def report_retime(long_name, short_name):
    slow = load(long_name)
    fast = load(short_name)
    if not slow or not fast:
        return

    log("=== retime check: {} vs {} ===".format(long_name, short_name))
    worst_pos = 0.0
    worst_angle = 0.0
    for index in range(SAMPLES + 1):
        fraction = index / float(SAMPLES)
        t_slow = slow.get_play_length() * fraction
        t_fast = fast.get_play_length() * fraction
        for bone in COMPARE_BONES:
            p_slow = component_space_pos(slow, bone, t_slow)
            p_fast = component_space_pos(fast, bone, t_fast)
            worst_pos = max(worst_pos, (p_slow - p_fast).length())

            r_slow = unreal.AnimationLibrary.get_bone_pose_for_time(slow, bone, t_slow, False).rotation
            r_fast = unreal.AnimationLibrary.get_bone_pose_for_time(fast, bone, t_fast, False).rotation
            worst_angle = max(worst_angle, quat_angle_deg(r_slow, r_fast))

    log("  worst position gap {:.2f}cm, worst angle gap {:.1f} deg (sampled at matching fractions)".format(
        worst_pos, worst_angle))
    if worst_pos <= RETIME_POS_TOLERANCE_CM and worst_angle <= RETIME_ANGLE_TOLERANCE_DEG:
        log("  -> same choreography: the short clip is the long one retimed, a play rate "
            "reproduces either from the other")
    else:
        log("  -> different choreography: the short clip was re-keyed, not just sped up")


for clip_name in JUMP_CLIPS:
    clip = load(clip_name)
    if clip:
        report_jump(clip)

for clip_name in ATTACK_CLIPS:
    clip = load(clip_name)
    if clip:
        report_attack(clip)

for long_name, short_name in COMPARE_PAIRS:
    report_retime(long_name, short_name)

log("=== MeasureAnimAnchors done ===")
