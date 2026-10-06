"""Inventory native weapon meshes from copied saves in an isolated flat process.

No source saves or ammo are written. Raw RAM/model captures remain in ignored
output directories. A separate TCP port avoids interacting with a user's game.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'psxrecomp/tools'))
from debug_client import connect, send_cmd


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--port', type=int, default=4372)
    parser.add_argument('--slots', type=int, nargs='+', default=[1, 2, 3, 4, 5, 6, 9])
    parser.add_argument('--only-weapon-ids', type=int, nargs='+', help='Restrict optional pose/aim controls, retaining inventory')
    parser.add_argument('--aim-check', action='store_true', help='Native shot/physics controls for each selected weapon')
    parser.add_argument('--pose-check', action='store_true', help='Synthetic stereo controls for each selected weapon')
    parser.add_argument('--multiplayer-controls', action='store_true', help='Enable the opt-in player-one multiplayer test view')
    parser.add_argument('--unique-controls', action='store_true', help='Run optional controls only once per weapon ID')
    parser.add_argument('--stereo-fault',type=int,choices=[1,2,3],default=0,help='One-shot stereo fault control')
    parser.add_argument('--switches', type=int, default=12)
    args = parser.parse_args()
    out = args.directory.resolve()
    out.mkdir(parents=True, exist_ok=False)
    exe = args.executable.resolve()
    session = out / 'session'
    (session / 'saves/openbios').mkdir(parents=True)
    shutil.copyfile(ROOT / 'game.toml', session / 'game.toml')
    source_files = list((ROOT / 'saves/openbios').glob('*.pst'))
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files}
    for p in source_files:
        shutil.copyfile(p, session / 'saves/openbios' / p.name)
    try:
        with socket.create_connection(('127.0.0.1', args.port), timeout=.2):
            raise RuntimeError('Diagnostic TCP port is occupied')
    except (ConnectionRefusedError, TimeoutError):
        pass
    env = {k:v for k,v in os.environ.items() if not k.startswith('PSX_')}
    env.update(PSX_OPENXR='0', PSX_VR_OPENXR='0', PSX_VR_STEREO='0',
               PSX_VR_WEAPON_POSE='0', PSX_VR_WEAPON_AIM='0', PSX_DEV_INPUT='0', PSX_VSYNC='0')
    if args.pose_check or args.aim_check:
        env.update(PSX_VR_STEREO='1', PSX_VR_WEAPON_POSE='1', PSX_VR_WEAPON_AIM='1',
                   PSX_VR_MOVEMENT='1', PSX_VR_WORLD_SCALE='3', PSX_VR_DESKTOP_FOV='1', PSX_RENDER_PASS_VERIFY='1')
    if args.multiplayer_controls:
        env['PSX_VR_WEAPON_MP_CONTROL'] = '1'
    if args.stereo_fault:
        env['PSX_VR_STEREO_FAULT'] = str(args.stereo_fault)
    receipt = {'executable': str(exe), 'exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
               'port': args.port, 'synthetic_pose_check': args.pose_check, 'synthetic_aim_check':args.aim_check, 'source_saves': hashes, 'slots': [], 'complete': False}

    def command(name, **kw):
        with connect(port=args.port, timeout=20) as sock:
            result = send_cmd(sock, {'cmd':name, **kw})
        if not result.get('ok'):
            raise RuntimeError(result)
        return result

    def save(path, result):
        path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')

    def frames(count):
        start = command('frame')['frame']
        deadline = time.monotonic() + 25
        while command('frame')['frame'] - start < count:
            if time.monotonic() > deadline:
                raise TimeoutError('Guest frames stopped advancing')
            time.sleep(.05)

    def snapshot(directory):
        data = bytes.fromhex(command('read_ram', addr='0x80000000', len=0x200000)['hex'])
        (directory / 'ram.bin').write_bytes(data)
        def valid(addr, size=4):
            return 0x80000000 <= addr <= 0x80200000-size
        def word(addr):
            return struct.unpack_from('<I', data, addr-0x80000000)[0]
        def byte(addr):
            return data[addr-0x80000000]
        def mesh(entity):
            if not valid(entity, 132):
                return None
            header = word(entity+124)
            if not valid(header, 8):
                return {'entity':hex(entity), 'header':hex(header)}
            faces, count = word(header), word(header+4)
            result = {'entity':hex(entity), 'header':hex(header), 'faces':hex(faces), 'face_count':count}
            if count <= 4096 and valid(faces, count*28):
                nodes = Counter()
                mixed = 0
                maxima = {}
                for i in range(count):
                    a = faces+i*28
                    group = tuple(byte(a+j) for j in (23,25,27))
                    if len(set(group)) != 1:
                        mixed += 1
                    nodes[str(group)] += 1
                    for v, node in zip((22,24,26), group):
                        maxima[node] = max(maxima.get(node, 0), byte(a+v))
                result.update(node_groups=dict(nodes), mixed_faces=mixed, node_vertex_max=maxima)
            return result
        players = []
        single_player = valid(word(0x8009d654), 908)
        owner = word(0x8009d654 if single_player else 0x80099428)
        entity = word(0x8009d64c if single_player else 0x80099418)
        visited = set()
        while valid(entity, 908) and entity not in visited and len(visited) < 512:
            visited.add(entity)
            if word(entity+8) == 1:
                inp = word(entity+904)
                row = {'player':hex(entity), 'camera_owner':entity==owner, 'input':hex(inp)}
                if valid(inp, 920):
                    row.update(weapon_slot=byte(inp+84), input_hex=data[inp-0x80000000:inp-0x80000000+784].hex(),
                               weapon_id=byte(inp+85), held_mesh=mesh(word(entity+256)))
                players.append(row)
            entity = word(entity+4)
        if not any(p['player']==hex(owner) for p in players) and valid(owner, 908) and word(owner+8)==1:
            inp = word(owner+904)
            if valid(inp, 920):
                players.append({'player':hex(owner), 'camera_owner':True, 'input':hex(inp),
                                'weapon_slot':byte(inp+84), 'weapon_id':byte(inp+85), 'held_mesh':mesh(word(owner+256))})
        command('screenshot_file', path=str(directory/'flat.png'))
        result = {'frame':command('frame'), 'camera_owner':hex(owner), 'players':players,
                  'single_player':single_player, 'controlled_player':hex(word(0x8009d654 if single_player else 0x8009943c)),
                  'list_entities':len(visited), 'pad':command('pad_status')}
        save(directory/'snapshot.json', result)
        save(directory/'native_gte.json', command('gte_ring_dump', render=0, count=4096))
        return result

    process = subprocess.Popen([str(exe), '--no-launcher', '--game', 'game.toml', '--disc',
        str(ROOT/'Input/medal-of-honor/medal-of-honor.cue'),
        '--memcard-dir',str(session/'saves'), '--debug-port', str(args.port)],
        cwd=session, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline = time.monotonic()+40
        while True:
            if process.poll() is not None:
                raise RuntimeError(f'Diagnostic startup exit {process.returncode}')
            try:
                command('ping')
                break
            except OSError:
                if time.monotonic()>deadline:
                    raise
                time.sleep(.2)
        controlled_ids = set()
        for slot in args.slots:
            directory = out / f'slot-{slot:02}'
            directory.mkdir()
            before = command('savestate_status')
            command('savestate', op='load', slot=slot)
            deadline = time.monotonic()+40
            while True:
                status = command('savestate_status')
                if status['generation']>before['generation'] and not status['pending']:
                    if not status['last_ok'] or status['last_slot']!=slot:
                        raise RuntimeError(status)
                    break
                if time.monotonic()>deadline:
                    raise TimeoutError('State did not load')
                time.sleep(.1)
            save(directory/'load.json', status)
            frames(60)
            from weapon_state import resume_if_paused
            resumed = resume_if_paused(command, frames)
            row = {'slot':slot, 'resumed_pause':resumed, 'captures':[]}
            seen = set()
            for index in range(args.switches+1):
                capture = directory/f'weapon-{index:02}'
                capture.mkdir()
                state = snapshot(capture)
                signature = tuple((p['player'],p.get('weapon_slot')) for p in state['players'])
                row['captures'].append({'index':index, 'players':state['players']})
                if index and signature and signature in seen:
                    break
                seen.add(signature)
                controlled = next((p for p in state['players'] if p['player']==state['controlled_player']), None)
                selected_control = controlled is not None and (
                    not args.only_weapon_ids or controlled.get('weapon_id') in args.only_weapon_ids)
                mp = not state['single_player']
                can_control = state['single_player'] or args.multiplayer_controls
                if args.unique_controls and controlled and controlled.get('weapon_id') in controlled_ids:
                    selected_control = False
                if args.pose_check and can_control and selected_control:
                    import check_weapon_pose, check_movement
                    check_weapon_pose.command = check_movement.command = command
                    controls = []
                    for label, pose in [('native', None), ('straight', {}), ('right', {'px_mm':350}),
                                        ('left45', {'qy':382683,'qw':923880}), ('unfocused', {'focused':0})]:
                        controls.append(check_weapon_pose.capture(capture,label,pose,multiplayer=mp,
                            input_override={'right_trigger':1000} if controlled['weapon_id']==7 else None))
                    rows = {r['case']:r for r in controls}
                    assert rows['straight']['producer_vertices'] != rows['right']['producer_vertices']
                    assert rows['straight']['producer_vertices'] != rows['left45']['producer_vertices']
                    command('openxr_hands_override',clear=1)
                    command('openxr_input_override',clear=1)
                    restore = command('render_pass_stats')
                    assert restore['verify_mismatch'] == 0, restore
                    save(capture/'pose_controls.json', {'synthetic':True,'controls':controls,'restore':restore})
                    print('Pose controls passed:',controlled['weapon_id'],flush=True)
                # Passport has native activation but no damage actor to aim.
                if args.aim_check and can_control and selected_control and controlled['weapon_id'] != 7:
                    import check_weapon_aim, check_movement
                    check_weapon_aim.command = check_movement.command = command
                    expected_weapon = controlled['weapon_id']
                    def equip():
                        resume_if_paused(command, frames)
                        for unused in range(index):
                            command('press',buttons=0xffff^0x2000,frames=4)
                            frames(75)
                        owner = int.from_bytes(bytes.fromhex(command('read_ram',addr='0x8009943c' if mp else '0x8009d654',len=4)['hex']),'little')
                        inp = int.from_bytes(bytes.fromhex(command('read_ram',addr=hex(owner+904),len=4)['hex']),'little')
                        fields = bytes.fromhex(command('read_ram',addr=hex(inp+84),len=24)['hex'])
                        assert fields[1] == expected_weapon, fields.hex()
                        clip = inp+102+fields[0]*2
                        ammo = int.from_bytes(bytes.fromhex(command('read_ram',addr=hex(clip),len=2)['hex']),'little',signed=True)
                        if ammo == 0:
                            command('write_ram',addr=hex(clip),val='0x01')
                            command('write_ram',addr=hex(clip+1),val='0x00')
                        deadline = time.monotonic()+30
                        while int.from_bytes(bytes.fromhex(command('read_ram',addr=hex(inp+72),len=4)['hex']),'little',signed=True)>0:
                            if time.monotonic()>deadline:
                                raise TimeoutError('Native weapon cooldown')
                            frames(8)
                        command('clear_input')
                        frames(16)
                    controls=[]
                    for label, pose in [('native',None),('straight',{}),('left45',{'qy':382683,'qw':923880}),
                                        ('unfocused',{'focused':0})]:
                        controls.append(check_weapon_aim.control(capture,slot,label,pose,0,equip,release_frames=60 if expected_weapon in (3,10) else 12,arena_end=0x800ef000 if expected_weapon in (3,10) else 0x80150000,multiplayer=mp))
                    save(capture/'aim_controls.json',{'synthetic':True,'weapon_id':expected_weapon,'controls':controls})
                    rows = {c['case']:c for c in controls}
                    assert sum(a*b for a,b in zip(rows['straight']['unit_direction'],rows['left45']['unit_direction'])) < .95
                    if expected_weapon == 4:
                        assert all(len(c['constructors']) >= 6 for c in controls), 'Missing shotgun pellets'
                    print('Aim controls passed:',expected_weapon,flush=True)
                    check_movement.load(slot)
                    equip()
                if selected_control and can_control and (args.pose_check or args.aim_check):
                    controlled_ids.add(controlled['weapon_id'])
                command('press', buttons=0xffff^0x2000, frames=4)
                frames(75)
            receipt['slots'].append(row)
            save(out/'inventory.json', receipt)
            print(f'Slot {slot}: {len(row["captures"])} weapon snapshots', flush=True)
        receipt['complete'] = True
    finally:
        if process.poll() is None:
            for name in ('stereo_stats', 'render_pass_stats', 'openxr_input', 'openxr_hands'):
                try:
                    receipt[name] = command(name)
                except (OSError, RuntimeError):
                    pass
            try:
                command('quit')
            except (OSError, RuntimeError):
                pass
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=8)
        receipt['exit_code'] = process.returncode
        for path, value in hashes.items():
            if hashlib.sha256(Path(path).read_bytes()).hexdigest() != value:
                raise RuntimeError('A source save changed: '+path)
        save(out/'inventory.json', receipt)


if __name__ == '__main__':
    main()
