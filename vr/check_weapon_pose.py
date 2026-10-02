"""Synthetic rifle mesh controls; launch with -WeaponPoseDiagnostic -DesktopFov.

Fresh pairs are awaited through complete PNG decoding. This validates desktop
pose response, not controller/grip alignment in a headset. Keep output ignored.
"""
import argparse
import json
import time
from pathlib import Path
from PIL import Image
from capture_stereo import command, save
from check_movement import load, wait_frames


def capture(out, name, pose):
    directory = out / name
    directory.mkdir()
    command('openxr_hands_override', clear=1)
    if pose is not None:
        command('openxr_hands_override', hand='right', pose='grip',
                px_mm=pose.get('px_mm', 150), py_mm=-150, pz_mm=-400,
                focused=pose.get('focused', 1))
        command('openxr_hands_override', hand='right', pose='aim',
                px_mm=150, py_mm=-150, pz_mm=-400,
                qy=pose.get('qy', 0), qw=pose.get('qw', 1000000),
                focused=pose.get('focused', 1))
    wait_frames(8)
    before = command('stereo_stats')['last_pair_id']
    hands = command('openxr_hands')
    command('stereo_dump', path=directory.resolve().as_posix(), count=1)
    deadline = time.monotonic() + 30
    while True:
        manifests = [p for p in directory.glob('p*.json') if p.stem[1:].isdigit()]
        if manifests:
            path = manifests[0]
            try:
                manifest = json.loads(path.read_text(encoding='utf-8-sig'))
                assert manifest['pair_id'] > before, 'stale pair captured'
                for eye in ('left', 'right'):
                    with Image.open(path.with_name(path.stem + '_' + eye + '.png')) as img:
                        img.load()
                        assert img.size == (manifest['width'], manifest['height'])
                break
            except (OSError, json.JSONDecodeError):
                pass
        if time.monotonic() > deadline:
            raise TimeoutError('fresh complete pair: ' + name)
        time.sleep(.05)
    ring = command('gte_ring_dump', render=1, count=4096)
    save(directory, 'gte.json', ring)
    vertices = [e for e in ring['entries'] if e['ra'] == '0x80080F2C']
    if not vertices:
        raise RuntimeError('no held-weapon RTPS producer')
    vertex = max(vertices, key=lambda e: e['seq'])
    save(directory, 'hands.json', hands)
    return {'case': name, 'request': pose, 'before_pair': before,
            'manifest': manifest, 'hands': hands,
            'producer': {k: vertex[k] for k in ('seq', 'frame', 'ra', 'V0', 'RT', 'TR', 'H', 'S2')}}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--slot', type=int, default=0)
    args = p.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    receipt = {'synthetic': True, 'slot': args.slot, 'complete': False, 'controls': []}
    load(args.slot)
    try:
        for name, pose in [('native', None), ('straight', {}),
                           ('right', {'px_mm': 350}),
                           ('left45', {'qy': 382683, 'qw': 923880}),
                           ('unfocused', {'focused': 0})]:
            row = capture(args.directory, name, pose)
            receipt['controls'].append(row)
            save(args.directory, 'controls.json', receipt)
            print(name, row['manifest']['pair_id'], row['producer']['V0'], flush=True)
        rows = {r['case']: r for r in receipt['controls']}
        assert rows['native']['producer']['V0'] == rows['unfocused']['producer']['V0']
        assert rows['straight']['producer']['V0'] != rows['right']['producer']['V0']
        assert rows['straight']['producer']['V0'] != rows['left45']['producer']['V0']
        receipt['stereo'] = command('stereo_stats')
        receipt['restore'] = command('render_pass_stats')
        assert receipt['restore']['verify_mismatch'] == 0
        receipt['complete'] = True
    finally:
        command('openxr_hands_override', clear=1)
        save(args.directory, 'controls.json', receipt)


if __name__ == '__main__':
    main()
