# Retarget the Day presenter's camera while PIE runs, so canguan framing can be tried at
# different angles without editing SDayBoardPresentation.cpp.
#
# Usage (editor console):
#   py "<this file>"              -> defaults below
#   py "<this file>" 70 965       -> pitch 70 (sign ignored), OrthoWidth 965
#
# The presenter applies its portrait framing once in BeginPlay, so a live override sticks.
# Reflection writes to RelativeLocation/RelativeRotation do not refresh the component
# transform; the component setters must be called instead.
#
# Framing maths. The camera is orthographic, yaw 90, so screen width maps 1:1 to world X
# and screen height maps to world Y foreshortened by sin(pitch):
#   x_coverage = ortho_width
#   y_coverage = ortho_width * (16/9) / sin(pitch)
# The board is 900 wide, so ortho_width must stay >= ~900 or the play area gets cropped.
# Shallower pitch buys more Y coverage, which is the room the shopfront needs.

import math
import sys

import unreal


DEFAULT_PITCH = 55.0
DEFAULT_ORTHO_WIDTH = 950.0

YAW = 90.0
DISTANCE = 2200.0
# Shipping output is a 1440x3200 portrait panel; keep this in step with the presenter.
ASPECT = 1440.0 / 3200.0

# Content to keep in frame: board frame bottom face through the top of the shopfront.
CONTENT_Y_MIN = -350.0
CONTENT_Y_MAX = 1475.0
# Board frame half width; used only to warn about cropping.
BOARD_HALF_WIDTH = 450.0


def log(message):
    unreal.log("[TiltDayCamera] {}".format(message))


def read_args():
    pitch = DEFAULT_PITCH
    ortho = DEFAULT_ORTHO_WIDTH
    args = [a for a in sys.argv[1:] if a.strip()]
    try:
        if len(args) >= 1:
            pitch = abs(float(args[0]))
        if len(args) >= 2:
            ortho = float(args[1])
    except ValueError:
        log("could not parse args {}, using defaults".format(args))
    return pitch, ortho


def find_presenter(world):
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if "SDayBoardPresenter" in actor.get_class().get_name():
            return actor
    return None


def main():
    pitch, ortho = read_args()

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        unreal.log_error("[TiltDayCamera] no PIE world; start PIE first")
        return

    presenter = find_presenter(world)
    if presenter is None:
        unreal.log_error("[TiltDayCamera] presenter not found in PIE world")
        return

    camera = presenter.get_component_by_class(unreal.CameraComponent)
    if camera is None:
        unreal.log_error("[TiltDayCamera] presenter has no CameraComponent")
        return

    pitch_rad = math.radians(pitch)
    y_coverage = (ortho / ASPECT) / math.sin(pitch_rad)
    look_at_y = (CONTENT_Y_MIN + CONTENT_Y_MAX) * 0.5

    # Walk back along the view direction so look_at stays centred in frame.
    yaw_rad = math.radians(YAW)
    forward = (
        math.cos(-pitch_rad) * math.cos(yaw_rad),
        math.cos(-pitch_rad) * math.sin(yaw_rad),
        math.sin(-pitch_rad),
    )
    location = unreal.Vector(
        -forward[0] * DISTANCE,
        look_at_y - forward[1] * DISTANCE,
        -forward[2] * DISTANCE,
    )

    camera.set_relative_location(location, False, False)
    camera.set_relative_rotation(
        unreal.Rotator(roll=0.0, pitch=-pitch, yaw=YAW), False, False
    )
    camera.set_editor_property("ortho_width", ortho)

    log(
        "pitch=-{:.0f} ortho={:.0f} -> frame X +/-{:.0f}, Y {:.0f}..{:.0f} (need {:.0f}..{:.0f}), "
        "location=({:.0f}, {:.0f}, {:.0f})".format(
            pitch, ortho,
            ortho * 0.5,
            look_at_y - y_coverage * 0.5, look_at_y + y_coverage * 0.5,
            CONTENT_Y_MIN, CONTENT_Y_MAX,
            location.x, location.y, location.z,
        )
    )
    if ortho * 0.5 < BOARD_HALF_WIDTH:
        log("WARNING board is cropped horizontally; raise ortho above {:.0f}".format(
            BOARD_HALF_WIDTH * 2.0))


main()
