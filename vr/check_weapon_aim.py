"""Bounded synthetic weapon aiming controls; no headset alignment claim.

Launch run_vr.ps1 -Desktop -MovementDiagnostic -WeaponAimDiagnostic -Slot 0.
Bulk traces stay in the supplied ignored directory. Requires framework PC filters.
"""
import argparse
import math
import time
from pathlib import Path
from capture_stereo import command, save
from check_movement import load, wait_frames


def signed(value):
    v = int(value, 16)
    return v - 2**32 if v >= 2**31 else v


def control(out, slot, name, pose, turn, equip=None, release_frames=12, arena_end=0x800ef000, multiplayer=False):
    command('openxr_hands_override', clear=1)
    load(slot)
    if equip is not None:
        equip()
    if turn:
        command('openxr_input_override', rx=turn)
        wait_frames(20)
        command('openxr_input_override', clear=1)
        wait_frames(4)
    if pose is not None:
        command('openxr_hands_override', hand='right', pose='aim', **pose)
    hands = command('openxr_hands')
    player = int.from_bytes(bytes.fromhex(command('read_ram', addr='0x8009943c' if multiplayer else '0x8009d654', len=4)['hex']), 'little')
    seam = (0x80045900, 0x80045910) if multiplayer else (0x80045514, 0x80045524)
    motion_pcs = [0x80067e10,0x80067e2c,0x80067e48] if multiplayer else [0x8006cc10,0x8006cc2c,0x8006cc48]
    filters = [('seam', *seam), ('motion', motion_pcs[0], motion_pcs[-1]+4)]
    if not multiplayer:
        filters.append(('damage', 0x8004acc0, 0x8004acc4))
    collected = {label:{} for label,lo,hi in filters}
    cursors = {label:{'total':0,'frame':-1} for label,lo,hi in filters}
    windows = {label:[] for label,lo,hi in filters}
    def collect():
        for label,lo,hi in filters:
            cursor = cursors[label]
            trace = command('wtrace_dump',pc_lo=hex(lo),pc_hi=hex(hi),count=2048,
                            frame_lo=max(-1,cursor['frame']-1))
            if trace['total']-trace['available']>cursor['total'] or trace['emitted']>=2048:
                raise RuntimeError(f'{name}/{label}: incomplete streaming trace')
            for e in trace['entries']:
                collected[label][e['seq']] = e
                cursor['frame'] = max(cursor['frame'],e['frame'])
            cursor['total'] = trace['total']
            windows[label].append({k:trace[k] for k in ('total','available','emitted')})
    command('wtrace_clear')
    # Shot allocation varies, including after a body turn. Covers the measured
    # actor arena, excluding GPU packet memory and stack traffic.
    command('wtrace_range', lo='0x800b0000', hi='0x801ff000' if multiplayer else hex(arena_end))
    command('openxr_input_override', right_trigger=1000)
    def wait_with_pose(n):
        if pose is None and not multiplayer:
            return wait_frames(n)
        first = command('frame')['frame']
        deadline = time.monotonic()+30
        while (frame := command('frame')['frame']) < first+n:
            if time.monotonic()>deadline:
                raise TimeoutError('Fresh synthetic pose window')
            if pose is not None:
                command('openxr_hands_override',hand='right',pose='aim',**pose)
            if multiplayer:
                collect()
            time.sleep(.04)
        return {'first':first,'last':frame}
    interval = wait_with_pose(60 if multiplayer else 24)
    save(out, name+'-firing-input.json', {'pad':command('pad_status'), 'actions':command('openxr_input'),
        'player':command('read_ram',addr=hex(player),len=1024),
        'input':command('read_ram',addr=hex(int.from_bytes(bytes.fromhex(command('read_ram',addr=hex(player+904),len=4)['hex']),'little')),len=2048)})
    command('openxr_input_override', clear=1)
    wait_with_pose(release_frames)
    command('wtrace_range', lo='0', hi='0')
    traces = {'damage': []}
    if multiplayer:
        collect()
    for label, lo, hi in filters:
        trace = ({'complete':True,'streaming':True,'windows':windows[label],
                  'entries':[collected[label][k] for k in sorted(collected[label])]} if multiplayer else
                 command('wtrace_dump', pc_lo=hex(lo), pc_hi=hex(hi), count=2048))
        save(out, name+'-'+label+'.json', trace)
        if not multiplayer and (trace['total'] != trace['available'] or trace['emitted'] >= 2048):
            raise RuntimeError(f'{name}/{label}: incomplete trace')
        traces[label] = trace['entries']
    creations = [e for e in traces['seam'] if int(e['s2'], 16) == player]
    if not creations:
        raise RuntimeError(name+': no observed player constructor; no aim conclusion')
    expected = pose is not None and pose.get('focused',1) and (pose.get('flags',15)&3)==3
    constructors = [e for e in creations if int(e['pc'],16)==seam[0]]
    assert constructors, name+': no native constructor marker'
    constructor_stores = []
    first_stores = []
    for index, marker in enumerate(constructors):
        actor = int(marker['s0'],16)
        addresses = {(actor+i)&0x1fffffff for i in (524,528,532,552,556,560)}
        end_seq = constructors[index+1]['seq'] if index+1<len(constructors) else float('inf')
        stores = [e for e in creations if marker['seq']<=e['seq']<end_seq and
                  int(e['s0'],16)==actor and int(e['addr'],16) in addresses]
        assert len(stores)==(6 if expected else 0), (name,hex(actor),len(stores))
        constructor_stores.append({'shot':hex(actor),'sequence':marker['seq'],'pose_store_count':len(stores)})
        if index==0:
            first_stores = stores
    shot = int(constructors[0]['s0'],16)
    overrides = first_stores
    for e in traces['seam']:
        if int(e['s2'],16) != player:
            assert int(e['addr'],16) not in {(int(e['s0'],16)+i)&0x1fffffff for i in (524,528,532,552,556,560)}
    motion = [e for e in traces['motion'] if int(e['s0'],16) == shot]
    if not motion:
        raise RuntimeError(name+': constructor observed, no recorded motion')
    frame = min(e['frame'] for e in motion)
    first = [e for e in motion if e['frame']==frame][:3]
    assert [int(e['pc'],16) for e in first] == motion_pcs
    delta = [(signed(e['new'])-signed(e['old']))/65536 for e in first]
    norm = math.sqrt(sum(v*v for v in delta))
    assert norm > 0
    compact = lambda e:{k:e[k] for k in ('frame','pc','addr','old','new','s0','s1','s2')}
    return {'case':name,'request':pose,'body_turn':turn,'interval':interval,
            'hands':hands,'shot':hex(shot),'pose_stores':[compact(e) for e in overrides],
            'first_motion':[compact(e) for e in first],'delta_world':delta,
            'unit_direction':[v/norm for v in delta],
            'damage':[compact(e) for e in traces['damage']],
            'constructors':constructor_stores,
            'multiplayer':multiplayer,
            'npc_constructors_observed':sum(int(e['s2'],16)!=player and int(e['pc'],16)==seam[0] for e in traces['seam'])}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    p.add_argument('--slot', type=int, default=0)
    args = p.parse_args()
    out = args.directory.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if (out/'controls.json').exists():
        raise FileExistsError('Use a fresh directory')
    receipt = {'synthetic':True,'slot':args.slot,'controls':[],'complete':False}
    cases = [('native',None,0),('straight',{},0),
             ('left45',{'qy':382683,'qw':923880},0),
             ('right45',{'qy':-382683,'qw':923880},0),
             ('translated',{'px_mm':200,'py_mm':-100,'pz_mm':-300},0),
             ('partial',{'flags':1},0),('unfocused',{'focused':0},0),
             ('body-turn',{},1000)]
    try:
        for name, pose, turn in cases:
            row=control(out,args.slot,name,pose,turn)
            receipt['controls'].append(row)
            save(out,'controls.json',receipt)
            print(name,row['unit_direction'],'damage writes',len(row['damage']),flush=True)
        rows={r['case']:r for r in receipt['controls']}
        for n in ('left45','right45','body-turn'):
            assert sum(a*b for a,b in zip(rows['straight']['unit_direction'],rows[n]['unit_direction'])) < .95
        baseline=[signed(e['new']) for e in rows['straight']['pose_stores'][:3]]
        translated=[signed(e['new']) for e in rows['translated']['pose_stores'][:3]]
        assert sum((a-b)**2 for a,b in zip(baseline,translated)) > (10*65536)**2
        receipt['complete']=True
        receipt['stereo']=command('stereo_stats')
        receipt['restore']=command('render_pass_stats')
    finally:
        command('openxr_input_override',clear=1)
        command('openxr_hands_override',clear=1)
        command('wtrace_range',lo='0',hi='0')
        save(out,'controls.json',receipt)


if __name__=='__main__':
    main()
