"""Slot-3 desktop combat controls; synthetic actions are not headset evidence.

Uses native ammo/stance writers and the equipped-weapon bytes. Menu screenshots
are supplementary observations; action and pad snapshots are separate queries.
"""
import argparse
from pathlib import Path
from capture_stereo import command, save
from check_movement import load, wait_frames

BASE = 0x800eec2c


def ram(addr, size):
    return bytes.fromhex(command('read_ram', addr=hex(addr), len=size)['hex'])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path)
    args = p.parse_args()
    out = args.directory.resolve(); out.mkdir(parents=True, exist_ok=True)
    if (out/'combat.json').exists(): raise FileExistsError('Use a fresh directory')
    results = {}

    def sample(name, fields, pressed, hold=12):
        command('openxr_input_override', **fields)
        interval = wait_frames(hold)
        pad = command('pad_status')['slot0']
        actions = command('openxr_input')
        assert int(pad['buttons'], 16) == 0xffff ^ pressed, (name, pad)
        assert actions['synthetic'] == 1
        results[name] = {'fields':fields, 'interval':interval, 'pad':pad, 'actions':actions}

    def release():
        command('openxr_input_override', clear=1); wait_frames(6)
        pad = command('pad_status')['slot0']
        assert pad['buttons'] == '0xFFFF' and pad['sticks'] == [128]*4, pad
        return pad

    def trace(name):
        tr = command('wtrace_dump', count=2048)
        save(out, name+'-writes.json', tr)
        return [e for e in tr['entries'] if e['old'] != e['new']]

    try:
        load(3)
        assert int.from_bytes(ram(0x8009d654,4),'little') == 0x800ee860
        required = ram(0x800b7790,36); forbidden = ram(0x800b77e0,36)
        assert [int.from_bytes(required[i:i+4],'little') for i in range(0,36,4)] == [0x40,0x10,0x20,1,0x80,0x80,0x800,0x200,2]
        assert forbidden == bytes(36)
        results['native_tables'] = {'required_hex':required.hex(), 'forbidden_hex':forbidden.hex()}
        assert ram(BASE+84,2) == bytes([1,3]), 'slot-3 grenade selection changed'
        # Circle's measured native switch goes from grenade id3 to firearm id12.
        command('wtrace_range', lo=hex(BASE+84), hi=hex(BASE+108)); command('wtrace_clear')
        sample('weapon-cycle', {'right_buttons':2}, 0x2000)
        release(); wait_frames(60)
        assert ram(BASE+84,2) == bytes([0,12])
        results['weapon-cycle']['writers'] = trace('weapon-cycle')
        results['weapon-cycle']['equipped_hex'] = ram(BASE+84,4).hex()
        before = int.from_bytes(ram(BASE+102,2),'little'); assert before == 8
        command('wtrace_clear')
        sample('fire', {'right_trigger':1000}, 0x4000)
        release(); wait_frames(30)
        after = int.from_bytes(ram(BASE+102,2),'little'); assert after == 7
        writes = trace('fire')
        assert any(e['pc']=='0x8007DC24' and int(e['addr'],16)==(BASE+102)&0x1fffffff
                   and int(e['old'],16)==8 and int(e['new'],16)==7 for e in writes)
        results['fire'].update(clip_before=before,clip_after=after,writers=writes)
        reserve = int.from_bytes(ram(BASE+86,2),'little')
        command('wtrace_clear')
        sample('reload', {'left_buttons':1}, 0x8000)
        release(); wait_frames(120)
        reloaded = int.from_bytes(ram(BASE+102,2),'little'); assert reloaded == 8
        remaining = int.from_bytes(ram(BASE+86,2),'little'); assert remaining == reserve-1
        writes = trace('reload')
        assert any(int(e['addr'],16)==(BASE+102)&0x1fffffff and int(e['old'],16)==7
                   and int(e['new'],16)==8 for e in writes)
        results['reload'].update(clip_before=after,clip_after=reloaded,
                                reserve_before=reserve,reserve_after=remaining,writers=writes)
        load(3)
        command('wtrace_range',lo=hex(BASE),hi=hex(BASE+4)); command('wtrace_clear')
        sample('crouch', {'left_buttons':8}, 0x100)
        release()
        writes = trace('crouch')
        assert any(e['pc']=='0x8007ACD8' and (int(e['new'],16)^int(e['old'],16))&2 for e in writes)
        results['crouch']['writers'] = writes
        sample('crouch-second-press', {'left_buttons':8}, 0x100)
        release(); assert not int.from_bytes(ram(BASE,4),'little')&2
        load(3)
        sample('jump', {'left_buttons':2}, 0x1000); release()
        sample('use', {'right_buttons':1}, 0x8000); release()
        sample('native-aim', {'right_squeeze':1000}, 0x200); release()
        sample('combined', {'right_trigger':1000,'lx':707,'ly':707,'rx':1000}, 0x4000)
        assert results['combined']['pad']['sticks'] == [241,7,243,128]
        # Inactivity applies per action, independently of sticks; focus releases all.
        sample('unfocused', {'right_trigger':1000,'right_buttons':3,'left_buttons':15,
                            'right_squeeze':1000,'focused':0}, 0)
        sample('inactive', {'right_trigger':1000,'right_buttons':3,'left_buttons':15,
                           'right_squeeze':1000,'right_trigger_active':0,
                           'right_squeeze_active':0,'left_buttons_active':0,'right_buttons_active':0}, 0)
        release()
        sample('buttons-with-inactive-sticks', {'right_trigger':1000,'left_active':0,'right_active':0}, 0x4000)
        release()
        load(3)
        sample('pause', {'left_buttons':4}, 8)
        release(); wait_frames(6)
        command('screenshot_file',path=(out/'pause.png').as_posix())
        # Menu must resume through another native Start press, not a RAM edit.
        sample('resume', {'left_buttons':4}, 8); release()
        command('screenshot_file',path=(out/'resumed.png').as_posix())
        save(out,'combat.json',{'synthetic':True,'atomic_action_pad_queries':False,'results':results})
        save(out,'render_pass_stats.json',command('render_pass_stats'))
        save(out,'stereo_stats.json',command('stereo_stats'))
        print('Combat routing, native fire/reload/crouch writers and releases passed.')
    finally:
        command('clear_input'); command('openxr_input_override',clear=1)


if __name__ == '__main__': main()
