"""Check compiled weapon profiles against captured native RAM and faces."""
import argparse
import ctypes as c
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


class Profile(c.Structure):
    _fields_ = [('id',c.c_uint8),('gun_node',c.c_uint8),('node_count',c.c_uint8),
                ('faces',c.c_uint16),('gun_faces',c.c_uint16),('vertices',c.c_uint16*6)]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory',type=Path)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--compiler',default='C:/Strawberry/c/bin/gcc.exe')
    args=parser.parse_args()
    out=args.directory.resolve()
    out.mkdir(parents=True,exist_ok=False)
    wrapper=out/'profiles.c'
    wrapper.write_text('#include "moh_vr_weapons.h"\n'
        '__declspec(dllexport) const MOHVRWeaponProfile *profile(unsigned id) {return moh_vr_weapon_profile(id);}\n'
        '__declspec(dllexport) int face(unsigned id,const uint8_t *ns,const uint8_t *vs) {\n'
        ' const MOHVRWeaponProfile *p=moh_vr_weapon_profile(id); return p?moh_vr_weapon_face(p,ns,vs):-1; }\n',newline='\n')
    dll=out/'profiles.dll'
    subprocess.run([args.compiler,'-std=c11','-Wall','-Wextra','-Werror','-shared',
                    '-I',str(ROOT/'vr'),str(wrapper),'-o',str(dll)],check=True)
    lib=c.CDLL(str(dll))
    lib.profile.argtypes=[c.c_uint]
    lib.profile.restype=c.POINTER(Profile)
    lib.face.argtypes=[c.c_uint,c.POINTER(c.c_uint8),c.POINTER(c.c_uint8)]
    triple=c.c_uint8*3
    assert not lib.profile(255)
    results={}
    count=0
    for path in sorted(args.inventory.glob('slot-*/weapon-*/snapshot.json')):
        snapshot=json.loads(path.read_text())
        ram=(path.parent/'ram.bin').read_bytes()
        def word(addr):return struct.unpack_from('<I',ram,addr-0x80000000)[0]
        for row in snapshot['players']:
            wid=row['weapon_id']
            ptr=lib.profile(wid)
            assert ptr, f'No profile for native weapon {wid}'
            p=ptr.contents
            entity=int(row['held_mesh']['entity'],16)
            header=word(entity+124)
            faces,n=word(header),word(header+4)
            assert n==p.faces, (wid,n,p.faces)
            nodes_header=word(entity+128)
            nodes,total=word(nodes_header),word(nodes_header+4)
            assert total==p.node_count, (wid,total,p.node_count)
            for node in range(p.gun_node,p.node_count):
                assert word(nodes+node*8+4)==p.vertices[node-p.gun_node]
            selected=0
            for i in range(n):
                f=ram[faces-0x80000000+i*28:faces-0x80000000+(i+1)*28]
                ns,vs=triple(*f[23:28:2]),triple(*f[22:27:2])
                keep=lib.face(wid,ns,vs)
                assert keep>=0, (wid,i,list(ns),list(vs))
                selected+=keep
            assert selected==p.gun_faces,(wid,selected,p.gun_faces)
            ns=triple(p.gun_node,p.gun_node,p.gun_node)
            bad=triple(p.vertices[0],0,0)
            assert lib.face(wid,ns,bad)==-1, 'Invalid gun vertex accepted'
            results[wid]={'faces':n,'selected_faces':selected,'gun_node':p.gun_node,
                          'node_count':p.node_count,'captured_source':str(path)}
            count+=1
    assert set(results)=={1,2,3,4,5,6,8,10,11,12},results.keys()
    (out/'receipt.json').write_text(json.dumps({'complete':True,'native_meshes_checked':count,
        'profiles':results,'scope':'Captured layouts and malformed-index rejection; no live pose or damage claim'},indent=2)+'\n',newline='\n')
    print(f'PASS: {count} captured native meshes, all 10 profiles, malformed gun indices rejected')


if __name__=='__main__':
    main()
