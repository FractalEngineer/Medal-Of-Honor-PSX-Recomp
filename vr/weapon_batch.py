"""Movement-enabled per-weapon controls; default ten tracked runs of 30 seconds.

Uses isolated copied saves and TCP 4372. Desktop mode supplies synthetic hands;
headset mode uses real tracking. Neither mode records user acceptance automatically.
"""
import argparse
from datetime import datetime
from hashlib import sha256
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


def save(path, data):
    path.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weapons', nargs='+', default=['all'])
    parser.add_argument('--mode', choices=['tracked','native','compare'], default='tracked')
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--internal-resolution', default='1080p',
                        choices=['native','720p','1080p','1440p','4k','5k','8k','display'])
    parser.add_argument('--build-directory', type=Path, default=Path('build-vr-weapons'))
    parser.add_argument('--disc', type=Path, default=ROOT/'Input/medal-of-honor/medal-of-honor.cue')
    parser.add_argument('--port', type=int, default=4372)
    parser.add_argument('--desktop', action='store_true')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--directory', type=Path)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('--seconds must be between 1 and 300')
    manifest = json.loads((ROOT/'vr/weapon_controls.json').read_text(encoding='utf-8'))
    known = {w['key']: w for w in manifest['weapons']}
    if args.weapons == ['all']:
        weapons = manifest['weapons']
    else:
        if any(k not in known for k in args.weapons) or len(set(args.weapons)) != len(args.weapons):
            parser.error('Choose unique weapon keys from '+', '.join(known))
        weapons = [known[k] for k in args.weapons]
    modes = ['native','tracked'] if args.mode == 'compare' else [args.mode]
    for w in weapons:
        print(f"{w['key']:14} slot {w['slot']}, {w['switches']} native switches, {args.seconds}s per {args.mode} control", flush=True)
    if args.list:
        return
    exe = (ROOT/args.build_directory/'Medal_of_Honor__Recompiled.exe').resolve()
    disc = args.disc.resolve()
    if not exe.is_file() or not disc.is_file():
        raise FileNotFoundError(f'Candidate executable or disc missing: {exe}; {disc}')
    with socket.socket() as probe:
        if probe.connect_ex(('127.0.0.1', args.port)) == 0:
            raise RuntimeError(f'TCP {args.port} is occupied; no existing game will be touched')
    sources = sorted((ROOT/'saves/openbios').glob('*.pst'))
    hashes = {str(p):sha256(p.read_bytes()).hexdigest() for p in sources}
    for w in weapons:
        if not any(p.name.endswith(f"_slot{w['slot']:02}.pst") for p in sources):
            raise FileNotFoundError(f"Original slot {w['slot']} missing")
    out = args.directory or ROOT/'analysis/weapon-capture'/('headset-batch-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    receipt = {'manifest':manifest, 'executable':str(exe), 'executable_sha256':sha256(exe.read_bytes()).hexdigest(),
               'desktop_synthetic':args.desktop, 'seconds_per_case':args.seconds, 'internal_resolution':args.internal_resolution,
               'verify':args.verify, 'source_saves':hashes, 'cases':[], 'complete':False, 'headset_acceptance':False}
    print(f'Receipt directory: {out}', flush=True)
    print('Right trigger: fire/release grenade. Left stick: move. Right stick: turn. Left X: reload/use.', flush=True)
    print('Assess grip/size, barrel direction, both eyes, walking, firing and reload. Original saves remain untouched.', flush=True)

    def command(name, **kw):
        with connect(port=args.port, timeout=20) as sock:
            result = send_cmd(sock, {'cmd':name, **kw})
        if not result.get('ok'):
            raise RuntimeError(result)
        return result

    def refresh():
        if args.desktop and timed:
            for pose in ('grip','aim'):
                command('openxr_hands_override', hand='right', pose=pose, px_mm=150, py_mm=-150, pz_mm=-400)

    def frames(n):
        first = command('frame')['frame']
        deadline = time.monotonic()+30
        while command('frame')['frame'] < first+n:
            refresh()
            if time.monotonic()>deadline:
                raise TimeoutError('Guest frame progress')
            time.sleep(.04)

    def state(weapon):
        data = bytes.fromhex(command('read_ram', addr='0x80000000', len=0x200000)['hex'])
        def word(a):
            if not 0x80000000 <= a <= 0x801ffffc:
                raise ValueError(f'Invalid native pointer {a:08x}')
            return struct.unpack_from('<I',data,a-0x80000000)[0]
        player = word(0x8009943c if weapon.get('mode','multiplayer')=='multiplayer' else 0x8009d654)
        if word(player+8)!=1:
            raise RuntimeError('Expected multiplayer player one')
        inp = word(player+904)
        index, wid = data[inp-0x80000000+84:inp-0x80000000+86]
        entity = word(player+256)
        faces, nodes = word(entity+124), word(entity+128)
        observed = {'weapon_id':wid, 'faces':word(faces+4), 'nodes':word(nodes+4)}
        if wid!=weapon['id'] or observed['faces']!=weapon['faces'] or observed['nodes']!=weapon['nodes']:
            raise RuntimeError(f'Native equipped weapon/model mismatch: expected {weapon["key"]}, got {observed}')
        return {'player':hex(player),'input':hex(inp),'entity':hex(entity),'weapon_slot':index,**observed,
                'clip_address':hex(inp+102+index*2),'reserve_address':hex(inp+86+index*2)}

    try:
        for weapon in weapons:
            for mode in modes:
                timed = False
                case_dir = out/(weapon['key']+'-'+mode)
                session = case_dir/'session'
                (session/'saves/openbios').mkdir(parents=True)
                shutil.copyfile(ROOT/'game.toml',session/'game.toml')
                for p in sources:
                    shutil.copyfile(p,session/'saves/openbios'/p.name)
                env = {k:v for k,v in os.environ.items() if not k.startswith('PSX_')}
                env.update(PSX_OPENXR=str(int(not args.desktop)), PSX_VR_OPENXR=str(int(not args.desktop)),
                           PSX_VR_STEREO='1', PSX_VR_MOVEMENT='1', PSX_VR_HEAD_FRUSTUM='1',
                           PSX_VR_WEAPON_MP_CONTROL=str(int(weapon.get('mode','multiplayer')=='multiplayer')),
                           PSX_VR_WEAPON_POSE=str(int(mode=='tracked')), PSX_VR_WEAPON_AIM=str(int(mode=='tracked')),
                           PSX_VR_WORLD_SCALE='3', PSX_VR_IPD_MM='67', PSX_VR_WEAPON_MODEL_UNITS_PER_METER='850',
                           PSX_VR_WEAPON_PIVOT='80,150,100', PSX_VR_WEAPON_PROJECTION_SCALE='16',
                           PSX_VR_DESKTOP_FOV=str(int(args.desktop)), PSX_INTERNAL_RESOLUTION=args.internal_resolution,
                           PSX_RENDER_PASS_VERIFY=str(int(args.verify)), PSX_DEV_INPUT='0', PSX_VSYNC='0')
                case = {'weapon':weapon,'mode':mode,'ready':False,'complete':False}
                receipt['cases'].append(case)
                process = subprocess.Popen([str(exe),'--no-launcher','--game','game.toml','--disc',str(disc),'--debug-port',str(args.port)],
                                           cwd=session, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
                try:
                    deadline = time.monotonic()+40
                    while True:
                        if process.poll() is not None:
                            raise RuntimeError(f'Game startup exit {process.returncode}')
                        try:
                            command('ping')
                            break
                        except OSError:
                            if time.monotonic()>deadline:
                                raise
                            time.sleep(.2)
                    # Keep preparation neutral. Fresh synthetic hands are used
                    # only in timed desktop controls; headset controls clear both
                    # overrides before the clock and use live runtime tracking.
                    command('openxr_input_override',focused=0)
                    command('openxr_hands_override',hand='right',pose='grip',focused=0)
                    case['preparation_neutral'] = True
                    before = command('savestate_status')
                    command('savestate',op='load',slot=weapon['slot'])
                    deadline = time.monotonic()+40
                    while True:
                        status = command('savestate_status')
                        if status['generation']>before['generation'] and not status['pending']:
                            if not status['last_ok'] or status['last_slot']!=weapon['slot']:
                                raise RuntimeError(status)
                            break
                        if time.monotonic()>deadline:
                            raise TimeoutError('Save load')
                        time.sleep(.1)
                    frames(60)
                    for unused in range(weapon['switches']):
                        command('press',buttons=0xffff^0x2000,frames=4)
                        frames(75)
                    case['model_wait'] = []
                    deadline = time.monotonic()+10
                    while True:
                        try:
                            case['state'] = state(weapon)
                            break
                        except (RuntimeError,ValueError) as error:
                            case['model_wait'].append(str(error))
                            if time.monotonic()>deadline:
                                raise
                            frames(8)
                    deadline = time.monotonic()+30
                    cooldown = int(case['state']['input'],16)+72
                    while int.from_bytes(bytes.fromhex(command('read_ram',addr=hex(cooldown),len=4)['hex']),'little',signed=True)>0:
                        if time.monotonic()>deadline:
                            raise TimeoutError('Native weapon cooldown')
                        frames(8)
                    command('clear_input')
                    frames(16)
                    case['ammo_changes'] = []
                    for field, amount in (('clip_address',1),('reserve_address',20)):
                        addr = case['state'][field]
                        ammo = int.from_bytes(bytes.fromhex(command('read_ram',addr=addr,len=2)['hex']),'little',signed=True)
                        if ammo==0:
                            attempts = 0
                            deadline = time.monotonic()+10
                            while ammo==0:
                                attempts += 1
                                command('write_ram',addr=addr,val=hex(amount))
                                command('write_ram',addr=hex(int(addr,16)+1),val='0x00')
                                frames(4)
                                ammo = int.from_bytes(bytes.fromhex(command('read_ram',addr=addr,len=2)['hex']),'little',signed=True)
                                if time.monotonic()>deadline and ammo==0:
                                    raise TimeoutError('Ammo edit did not survive a guest frame')
                            case['ammo_changes'].append({'field':field,'before':0,'after':ammo,'attempts':attempts})
                    command('clear_input')
                    command('openxr_input_override',clear=1)
                    command('openxr_hands_override',clear=1)
                    timed = True
                    refresh()
                    frames(4)
                    case['stereo_before'] = command('stereo_stats')
                    case['restore_before'] = command('render_pass_stats')
                    if not args.desktop:
                        case['xr_before'] = command('openxr_stats')
                        if not case['xr_before'].get('running') or not case['xr_before'].get('submitted'):
                            raise RuntimeError('No active headset submissions; reconnect VDXR before this batch')
                    case['ready'] = True
                    print(f"START {weapon['label']} / {mode}: {args.seconds} seconds, movement enabled", flush=True)
                    started = time.monotonic()
                    samples = []
                    while time.monotonic()-started < args.seconds:
                        refresh()
                        if process.poll() is not None:
                            raise RuntimeError(f'Game exit {process.returncode}')
                        if not samples or time.monotonic()-started-samples[-1]['seconds']>=1:
                            samples.append({'seconds':round(time.monotonic()-started,3), 'frame':command('frame'),
                                            'stereo':command('stereo_stats'),'hands':command('openxr_hands')})
                        time.sleep(.04)
                    case['elapsed_seconds'] = round(time.monotonic()-started,3)
                    case['samples'] = samples
                    case['stereo_after'] = command('stereo_stats')
                    case['restore_after'] = command('render_pass_stats')
                    if not args.desktop:
                        case['xr_after'] = command('openxr_stats')
                    if case['stereo_after']['pairs']<=case['stereo_before']['pairs']:
                        raise RuntimeError('No fresh stereo pairs during timed control')
                    if args.verify and (case['restore_after']['verify_mismatch'] or
                       case['restore_after']['verify_checks']<=case['restore_before']['verify_checks']):
                        raise RuntimeError('Stereo rollback verification failed or did not run')
                    command('screenshot_file',path=str(case_dir/'native.png'))
                    case['complete'] = True
                    print(f"END {weapon['label']} / {mode}",flush=True)
                finally:
                    if process.poll() is None:
                        try:
                            command('quit')
                        except (OSError,RuntimeError):
                            pass
                        try:
                            process.wait(timeout=8)
                        except subprocess.TimeoutExpired:
                            process.terminate()
                            process.wait(timeout=8)
                    case['exit_code'] = process.returncode
                    save(case_dir/'receipt.json',case)
                    save(out/'receipt.json',receipt)
        receipt['complete'] = True
    finally:
        receipt['source_saves_unchanged'] = all(sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
        save(out/'receipt.json',receipt)
        if not receipt['source_saves_unchanged']:
            raise RuntimeError('An original save changed during the batch')


if __name__=='__main__':
    main()
