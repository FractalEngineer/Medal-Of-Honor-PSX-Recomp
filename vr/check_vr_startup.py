"""Read-only launch check: require real OpenXR submissions, without loading a save."""
import json
import time
from capture_stereo import command


def main():
    deadline = time.monotonic() + 30
    last = None
    while time.monotonic() < deadline:
        try:
            last = command("openxr_stats")
        except (ConnectionError, OSError):
            time.sleep(.2)
            continue
        if last.get("running") and last.get("submitted", 0) > 0:
            print("VR active:", last.get("runtime"), "submissions:", last["submitted"], flush=True)
            return
        if last.get("failures", 0):
            break
        time.sleep(.1)
    raise RuntimeError("No live OpenXR submission: " + json.dumps(last))


if __name__ == "__main__":
    main()
