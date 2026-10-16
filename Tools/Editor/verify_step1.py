"""Exercise the existing movement handlers in PIE on both Step 1 maps.

Run with UnrealEditor -ExecutePythonScript=<this file>. This opens its own
editor instance, ends its PIE sessions, writes a report, and closes that instance.
"""
import json
import time
import traceback
from pathlib import Path

import unreal


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ROOT = Path(unreal.Paths.project_dir()).resolve()
REPORT = ROOT / "Saved/Validation/step1-play-check.json"
MAPS = ["/Game/ThirdPerson/Lvl_ThirdPerson", "/Game/Ninja/Maps/L_TestArena"]
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {"phase": "load", "index": 0, "checks": [], "deadline": time.monotonic() + 150}
callback = None


def finish(status, error=None):
    unreal.unregister_slate_post_tick_callback(callback)
    if levels.is_in_play_in_editor():
        levels.editor_request_end_play()
    result = {
        "status": status,
        "checks": state["checks"],
        "method": "PIE with Character DoMove/DoLook/DoJumpStart handlers; not physical keyboard input",
    }
    if error:
        result["error"] = error
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("STEP1_PLAY_CHECK_" + status.upper() + " " + str(REPORT))
    unreal.SystemLibrary.quit_editor()


def tick(_delta):
    try:
        if time.monotonic() > state["deadline"]:
            raise RuntimeError("PIE check timed out in phase " + state["phase"])
        phase = state["phase"]
        if phase == "load":
            if not levels.load_level(MAPS[state["index"]]):
                raise RuntimeError("Could not load map")
            levels.editor_request_begin_play()
            state["phase"] = "wait_pie"
            return
        if phase == "wait_stop":
            if levels.is_in_play_in_editor() or editor.get_game_world() is not None:
                return
            state["index"] += 1
            if state["index"] == len(MAPS):
                finish("passed")
            else:
                state["phase"] = "load"
            return

        world = editor.get_game_world()
        if world is None:
            return
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        if pawn is None:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if phase == "wait_pie":
            if now < 1.0 or pawn.character_movement.is_falling():
                return
            state["start"] = pawn.get_actor_location()
            state["phase_time"] = now
            controller = pawn.get_controller()
            before = controller.get_control_rotation()
            pawn.do_look(15.0, 5.0)
            state["look_before"] = before
            state["phase"] = "move"
        elif phase == "move":
            pawn.do_move(0.0, 1.0)
            if now - state["phase_time"] < 0.8:
                return
            pos = pawn.get_actor_location()
            start = state["start"]
            distance = ((pos.x - start.x) ** 2 + (pos.y - start.y) ** 2) ** 0.5
            rotation = pawn.get_controller().get_control_rotation()
            yaw_delta = abs(rotation.yaw - state["look_before"].yaw)
            if distance < 30:
                raise RuntimeError("Movement distance too small: " + str(distance))
            if yaw_delta < 0.1:
                raise RuntimeError("Camera look handler did not update control rotation")
            state["distance"] = distance
            state["yaw_delta"] = yaw_delta
            state["jump_base"] = pos.z
            state["max_z"] = pos.z
            state["phase_time"] = now
            pawn.do_jump_start()
            state["phase"] = "jump"
        elif phase == "jump":
            state["max_z"] = max(state["max_z"], pawn.get_actor_location().z)
            elapsed = now - state["phase_time"]
            if elapsed > 0.15:
                pawn.do_jump_end()
            if elapsed < 1.5 or pawn.character_movement.is_falling():
                return
            height = state["max_z"] - state["jump_base"]
            if height < 20:
                raise RuntimeError("Jump did not leave ground: " + str(height))
            state["checks"].append({
                "map": MAPS[state["index"]],
                "pawn_class": pawn.get_class().get_path_name(),
                "movement_cm": round(state["distance"], 2),
                "camera_yaw_change_degrees": round(state["yaw_delta"], 2),
                "jump_height_cm": round(height, 2),
                "landed": True,
            })
            levels.editor_request_end_play()
            state["phase"] = "wait_stop"
    except Exception:
        finish("failed", traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
