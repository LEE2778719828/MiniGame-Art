"""Log imported box animation bone transforms for scale and motion validation."""

import unreal


ROOT = "/Game/Day/Art/canguan/animation"


def vec(value):
    return "({:.3f}, {:.3f}, {:.3f})".format(value.x, value.y, value.z)


for index in range(1, 6):
    animation = unreal.load_asset(f"{ROOT}/box{index}_Anim")
    skeleton = unreal.load_asset(f"{ROOT}/box{index}_Skeleton")
    if animation is None or skeleton is None:
        unreal.log_error(f"[VegetableBoxValidate] Missing box{index} assets")
        continue

    bones = unreal.AnimationLibrary.get_animation_track_names(animation)
    unreal.log(
        "[VegetableBoxValidate] box{} length={:.3f}s bones={}".format(
            index, animation.get_editor_property("sequence_length"), len(bones)))
    moving_bones = 0
    for bone in bones:
        start = unreal.AnimationLibrary.get_bone_pose_for_time(
            animation, bone, 0.0, False)
        middle = unreal.AnimationLibrary.get_bone_pose_for_time(
            animation,
            bone,
            animation.get_editor_property("sequence_length") * 0.5,
            False)
        end = unreal.AnimationLibrary.get_bone_pose_for_time(
            animation,
            bone,
            animation.get_editor_property("sequence_length"),
            False)
        translation_delta = (middle.translation - start.translation).length()
        rotation_delta = abs(middle.rotation.angular_distance(start.rotation))
        scale_delta = (middle.scale3d - start.scale3d).length()
        if translation_delta > 0.0001 or rotation_delta > 0.0001 or scale_delta > 0.0001:
            moving_bones += 1
        unreal.log(
            "[VegetableBoxValidate] box{} {} start T{} S{} middle T{} S{} end T{} S{}".format(
                index,
                bone,
                vec(start.translation),
                vec(start.scale3d),
                vec(middle.translation),
                vec(middle.scale3d),
                vec(end.translation),
                vec(end.scale3d)))
    unreal.log(
        "[VegetableBoxValidate] box{} moving bones at midpoint: {}/{}".format(
            index, moving_bones, len(bones)))
