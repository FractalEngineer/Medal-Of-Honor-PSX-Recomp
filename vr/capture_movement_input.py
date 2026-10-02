"""Record adjacent TCP action/pad snapshots during a worn-controller test.

Action and pad queries are separate requests; their rows are not an atomic
producer/delivery comparison. Synthetic samples are explicitly identified.
"""
import argparse
from datetime import datetime, timezone
import sys
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
    rows=[];errors=[];started=time.monotonic();end=started+args.seconds
    started_utc=datetime.now(timezone.utc).isoformat()
    while time.monotonic()<end:
        try:
            rows.append({'frame':command('frame')['frame'],
                         'actions':command('openxr_input'),'pad':command('pad_status')})
        except (OSError,RuntimeError) as e:
            errors.append(str(e))
        time.sleep(.1)
    save(out,'input_samples.json',{'atomic':False,'samples':rows,'errors':errors,
         'started_utc':started_utc,'requested_seconds':args.seconds,
         'elapsed_seconds':time.monotonic()-started})
    diagnostic_errors={}
    for cmd in ('openxr_stats','openxr_views','stereo_stats','pad_status'):
        try:
            save(out,cmd+'.json',command(cmd))
        except (OSError,RuntimeError) as e:
            diagnostic_errors[cmd]=str(e)
    save(out,'capture_status.json',{'complete':not errors and not diagnostic_errors,
         'diagnostic_errors':diagnostic_errors})
    print(f'{len(rows)} adjacent action/pad snapshots; {len(errors)} errors')
    if errors or diagnostic_errors:
        print('Partial capture retained; inspect input_samples.json and capture_status.json')
        return 1
    return 0


if __name__=='__main__':sys.exit(main())
