"""Slot-3 native heading-writer and calibrated-axis checks (synthetic actions).

Records the actual SW at 8007FC10, not a screenshot-derived turn rate. The
input-object address is the slot-3 value established by write tracing; refuse
other player objects rather than silently measuring an unrelated producer.
"""
import argparse
import math
from pathlib import Path
from capture_stereo import command, save
from check_movement import load, wait_frames


def signed_delta(entry):
    return (int(entry['new'], 16) - int(entry['old'], 16) + 2**31) % 2**32 - 2**31


def decoded_axes(pad):
    table = bytes.fromhex(command('read_ram', addr='0x800b74b0', len=160)['hex'])
    curve = bytes.fromhex(command('read_ram', addr='0x8009e3ac', len=128)['hex'])
    values = []
    for byte, semantic in zip(pad[:3], [3, 0, 1]):
        row = table[semantic*40:(semantic+1)*40]
        limit = row[14]
        factor = int.from_bytes(row[20:24] if byte < limit else row[24:28], 'little')
        index = ((limit-byte if byte < limit else byte-limit) * factor) >> 8
        values.append((-1 if byte < limit else 1) * curve[index]
                      if byte < limit or byte >= 166 else 0)
    return values, table.hex(), curve.hex()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--slot', type=int, default=3)
    args = parser.parse_args()
    out = args.directory.resolve(); out.mkdir(parents=True, exist_ok=True)
    if (out/'response.json').exists(): raise FileExistsError('Use a fresh directory')
    cases = [('turn-left', {'rx':-1000}), ('turn-right', {'rx':1000}),
             ('turn-half-left', {'rx':-600}), ('turn-half-right', {'rx':600}),
             ('forward', {'ly':1000}), ('strafe', {'lx':1000}),
             ('diagonal', {'lx':707, 'ly':707}),
             ('diagonal-back', {'lx':-707, 'ly':-707}),
             ('move-and-turn', {'lx':707, 'ly':707, 'rx':1000})]
    results = {}
    try:
        for name, axes in cases:
            load(args.slot)
            ptr = command('read_ram', addr='0x8009d654', len=4)['hex']
            if int.from_bytes(bytes.fromhex(ptr), 'little') != 0x800ee860:
                raise RuntimeError('slot-3 player object changed; remeasure writer address')
            command('wtrace_range', lo='0x800eea6c', hi='0x800eedb4')
            command('wtrace_clear')
            command('openxr_input_override', **axes)
            interval = wait_frames(20)
            pad = command('pad_status')['slot0']['sticks']
            trace = command('wtrace_dump', count=1024)
            save(out, name+'-writes.json', trace)
            heading = [e for e in trace['entries'] if e['pc']=='0x8007FC10'
                       and e['addr']=='0x000EEDB0' and e['s0']=='0x800EEC2C']
            if not heading: raise RuntimeError('heading producer missing')
            values, table, curve = decoded_axes(pad)
            command('openxr_input_override', clear=1)
            wait_frames(12)
            released = command('pad_status')['slot0']['sticks']
            if released != [128]*4: raise RuntimeError('axis held after release')
            results[name] = {'axes':axes, 'interval':interval, 'pad':pad,
                'heading_deltas':sorted(set(signed_delta(e) for e in heading)),
                'heading_writes':len(heading), 'curve_units':values,
                'movement_magnitude':math.hypot(values[1], values[2])/255,
                'released':released, 'calibration_hex':table, 'curve_hex':curve}
            print(name, results[name]['heading_deltas'], values, flush=True)
        for prefix in ('turn-', 'turn-half-'):
            left = [v for v in results[prefix+'left']['heading_deltas'] if v]
            right = [v for v in results[prefix+'right']['heading_deltas'] if v]
            assert len(left)==len(right)==1 and left[0]==-right[0], (left,right)
        assert [v for v in results['move-and-turn']['heading_deltas'] if v] == [v for v in results['turn-right']['heading_deltas'] if v]
        for name in ('diagonal', 'diagonal-back'):
            assert abs(results[name]['movement_magnitude'] - 1) < .03
        save(out, 'response.json', {'synthetic':True, 'controls':results})
        save(out, 'render_pass_stats.json', command('render_pass_stats'))
        save(out, 'stereo_stats.json', command('stereo_stats'))
    finally:
        command('clear_input'); command('openxr_input_override', clear=1)


if __name__=='__main__': main()
