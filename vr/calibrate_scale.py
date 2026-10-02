"""Calculate a metric mapping from an explicit physical reference assumption."""
import argparse,json,math
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--game-units',type=float,required=True)
p.add_argument('--meters',type=float,required=True)
p.add_argument('--reference',required=True,help='Identify measured span and physical-size assumption')
p.add_argument('--ipd-mm',type=float,default=67)
a=p.parse_args()
if not all(math.isfinite(x) and x>0 for x in [a.game_units,a.meters,a.ipd_mm]):p.error('values must be finite and positive')
u=a.game_units/a.meters
print(json.dumps({'reference':a.reference,'game_units':a.game_units,'assumed_meters':a.meters,
 'units_per_meter':u,'ipd_mm':a.ipd_mm,'half_eye_offset_units':u*a.ipd_mm/2000,
 'note':'Reference assumption; not measured physical scale unless reference size is known. Use WORLD_SCALE=1 for this mapping.'},indent=2))
