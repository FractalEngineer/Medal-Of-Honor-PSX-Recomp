"""Record adjacent TCP action/pad snapshots during a worn-controller test.

Action and pad queries are separate requests; their rows are not an atomic
producer/delivery comparison. Synthetic samples are explicitly identified.
"""
import argparse
import time
from pathlib import Path
from capture_stereo import command,save


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path)
    p.add_argument('--seconds',type=float,default=30)
    args=p.parse_args()
    if not 0<args.seconds<=60:p.error('seconds must be >0 and <=60')
    out=args.directory.resolve();out.mkdir(parents=True,exist_ok=True)
    if (out/'input_samples.json').exists():raise FileExistsError('Use a fresh directory')
    rows=[];errors=[];end=time.monotonic()+args.seconds
    while time.monotonic()<end:
        try:
            rows.append({'frame':command('frame')['frame'],
                         'actions':command('openxr_input'),'pad':command('pad_status')})
        except (OSError,RuntimeError) as e:
            errors.append(str(e))
        time.sleep(.1)
    save(out,'input_samples.json',{'atomic':False,'samples':rows,'errors':errors})
    for cmd in ('openxr_stats','openxr_views','stereo_stats','pad_status'):
        save(out,cmd+'.json',command(cmd))
    print(f'{len(rows)} adjacent action/pad snapshots; {len(errors)} errors')


if __name__=='__main__':main()
