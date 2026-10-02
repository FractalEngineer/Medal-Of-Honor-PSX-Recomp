"""Capture a 96-frame post-load window with held synthetic movement and turn.

Run twice in fresh identical desktop processes with stereo OFF/ON, then use
compare_frame_fingerprints.py. The default source route retains the host-time load guard and cannot assume
identical delivery frames. --native holds the measured mapper output through
TCP priority, bypassing that guard to isolate stereo replay with moving inputs.
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path
from capture_stereo import command, save


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--executable', type=Path, required=True)
    p.add_argument('--native', action='store_true',
                   help='Use matching native pad values to bypass the host-time load guard')
    args = p.parse_args()
    deadline = time.monotonic() + 30
    while True:
        try:
            command('ping'); break
        except OSError:
            if time.monotonic() > deadline: raise
            time.sleep(.1)
    try:
        if args.native:
            command('set_input', buttons='0xffff', lx=216, ly=7, rx=243, ry=128)
        else:
            command('openxr_input_override', lx=707, ly=707, rx=600)
        subprocess.run([sys.executable, str(Path(__file__).with_name('capture_stereo.py')),
                        str(args.directory), '--slot', '3', '--pairs', '0',
                        '--executable', str(args.executable)], check=True)
        pad = command('pad_status')
        save(args.directory, 'held_pad.json', pad)
        save(args.directory, 'held_actions.json', command('openxr_input'))
        if pad['slot0']['sticks'] == [128]*4:
            raise RuntimeError('held route did not reach SIO')
    finally:
        command('clear_input'); command('openxr_input_override', clear=1)


if __name__=='__main__': main()
