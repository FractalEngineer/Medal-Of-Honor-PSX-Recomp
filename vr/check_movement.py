"""Bounded desktop action controls. Synthetic data is not headset evidence.

Run after slot-load readiness against a windowed process with PSX_VR_MOVEMENT=1.
Each case reloads slot 3 and records producer-bound native level RT/TR before,
during and after release. Bulk images and full traces belong in analysis/vr-proof.
"""
import argparse
import json
import time
from pathlib import Path
from capture_stereo import command, save


def wait_frames(n):
    first = command('frame')['frame']
    deadline = time.monotonic() + 30
    while (frame := command('frame')['frame']) < first + n:
        if time.monotonic() > deadline:
            raise TimeoutError('guest frames did not advance')
        time.sleep(.04)
    return {'first': first, 'last': frame}


def load(slot):
    command('clear_input')
    command('openxr_input_override', clear=1)
    before = command('savestate_status')
    command('savestate', slot=slot, op='load')
    deadline = time.monotonic() + 30
    while True:
        s = command('savestate_status')
        if s['generation'] > before['generation'] and not s['pending']:
            if not s['last_ok'] or s['last_slot'] != slot:
                raise RuntimeError(s)
            break
        if time.monotonic() > deadline:
            raise TimeoutError('slot load')
        time.sleep(.05)
    wait_frames(60)  # let the measured post-load input guard expire


def snapshot(out, name):
    raw = command('gte_ring_dump', render=0, count=4096)
    save(out, name + '-native-ring.json', raw)
    entries = [e for e in raw['entries'] if e['ra'] == '0x8008BF84']
    if not entries:
        raise RuntimeError('no native level producer in ring')
    e = max(entries, key=lambda e:e['seq'])
    result = {'frame': e['frame'], 'seq': e['seq'], 'ra': e['ra'],
              'RT': e['RT'], 'TR': e['TR'], 'H': e['H'],
              'pad': command('pad_status'), 'actions': command('openxr_input')}
    # Player/camera object bytes are retained as raw evidence, without guessing fields.
    pointer = command('read_ram', addr='0x8009d654', len=4)
    addr = int.from_bytes(bytes.fromhex(pointer['hex']), 'little')
    if 0x80000000 <= addr < 0x801ff000:
        save(out, name + '-camera.json', command('read_ram', addr=hex(addr), len=1024))
    command('screenshot_file', path=(out / (name + '.png')).resolve().as_posix())
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--slot', type=int, default=3)
    p.add_argument('--native', action='store_true', help='Native PSX axes, bypass action mapper')
    args = p.parse_args()
    out = args.directory.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out/'controls.json').exists():
        raise FileExistsError('Use a fresh directory')
    controls = ([('native-lx-right', {'lx':255}), ('native-ly-forward', {'ly':0}),
                 ('native-rx-right', {'rx':255}), ('native-lx-half', {'lx':192})]
                if args.native else
                [('neutral', {}), ('forward', {'ly':1000}), ('backward', {'ly':-1000}),
                 ('strafe-right', {'lx':1000}), ('strafe-left', {'lx':-1000}),
                 ('turn-right', {'rx':1000}), ('turn-left', {'rx':-1000}),
                 ('turn-half', {'rx':600}), ('deadzone', {'lx':150,'ly':150,'rx':150}),
                 ('unfocused', {'lx':1000,'ly':1000,'rx':1000,'focused':0}),
                 ('inactive', {'lx':1000,'ly':1000,'rx':1000,'left_active':0,'right_active':0})])
    results = {}
    try:
        for name, kwargs in controls:
            load(args.slot)
            before = snapshot(out, name+'-before')
            if args.native:
                command('set_input', buttons='0xffff', **kwargs)
            else:
                command('openxr_input_override', **kwargs)
            interval = wait_frames(30)
            during = snapshot(out, name+'-during')
            command('clear_input');command('openxr_input_override', clear=1)
            released = wait_frames(12)
            after = snapshot(out, name+'-released')
            if after['pad']['slot0']['sticks'] != [128]*4:
                raise RuntimeError('held axis after release')
            results[name]={'request':kwargs, 'interval':interval,'release_interval':released,
                           'before':before,'during':during,'released':after}
            save(out,'controls.json',{'synthetic':not args.native,'native':args.native,'controls':results})
            print(name, 'pad',during['pad']['slot0']['sticks'],
                  'RT',during['RT'],'TR',during['TR'],flush=True)
    finally:
        command('clear_input');command('openxr_input_override',clear=1)
    save(out,'stereo_stats.json',command('stereo_stats'))
    save(out,'render_pass_stats.json',command('render_pass_stats'))


if __name__=='__main__':
    main()
