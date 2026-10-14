"""Verify isolated native HUD coverage and exact dual-matte reconstruction."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image


def verify(directory):
    black=np.frombuffer((directory/"hud-black.rgba").read_bytes(),dtype=np.uint8).reshape(240,512,4)[:,:,:3].astype(int)
    white=np.frombuffer((directory/"hud-white.rgba").read_bytes(),dtype=np.uint8).reshape(240,512,4)[:,:,:3].astype(int)
    union=np.zeros((240,512),dtype=bool);results=[]
    for name,(x,y,w,h) in zip(("left","right"),((16,12,96,96),(374,8,128,36))):
        rgba=np.frombuffer((directory/f"hud-{name}.rgba").read_bytes(),dtype=np.uint8).reshape(h,w,4)[::-1].astype(int)
        union[y:y+h,x:x+w]=True
        a=rgba[:,:,3:4]
        on_black=(rgba[:,:,:3]*a+127)//255
        on_white=(rgba[:,:,:3]*a+255*(255-a)+127)//255
        error=max(int(abs(on_black-black[y:y+h,x:x+w]).max()),int(abs(on_white-white[y:y+h,x:x+w]).max()))
        if error>2: raise RuntimeError(f"{name}: matte reconstruction error {error}")
        covered=int((a>128).sum());transparent=int((a==0).sum())
        if covered<20 or not transparent: raise RuntimeError(f"{name}: expected cutout graphics with transparent surroundings")
        Image.fromarray(rgba.astype(np.uint8),"RGBA").save(directory/f"hud-{name}.png")
        results.append({"side":name,"size":[w,h],"covered_pixels":covered,"transparent_pixels":transparent,"max_reconstruction_error":error})
    alpha=255-(white-black).mean(axis=2)
    if np.count_nonzero((alpha>2)&~union):raise RuntimeError("Isolated HUD draw contains content outside owned HUD regions")
    result={"passed":True,"panels":results,"native_producer_output":True}
    (directory/"native_hud_verification.json").write_text(json.dumps(result,indent=2)+"\n")
    return result


if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path)
    print(json.dumps(verify(parser.parse_args().directory),indent=2))
