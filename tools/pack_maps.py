#!/usr/bin/env python3
"""Pack SDK-exported tiles and metadata from git-tracked assets/legs/*.tmx."""
from tmx_helpers import *
a=options(); maps=load_maps('legs/leg[0-9].tmx','LEG_COUNT')
names=['Pigeon','Stamp','Beacon','Mate','Zip','Speed']
lines=['#include "game.h"','const leg_t LEGS[LEG_COUNT] = {']
for i,m in enumerate(maps):
    if m.h!=15 or m.w%40 or not 40<=m.w<=240: raise ValueError(f'{m.path}: invalid course dimensions')
    write(a.out/f'leg{i}.ztm',m.tiles(a,screen=(40,15)))
    objects=m.named_objects(['Player']+names)
    if sum(o['kind']=='Zip' for o in objects)>constant('MAX_ZIPS'):
        raise ValueError(f'{m.path}: too many clotheslines for MAX_ZIPS')
    players=[o for o in objects if o['kind']=='Player']
    if len(players)!=1: raise ValueError(f'{m.path}: exactly one Player required')
    spawns=[(names.index(o['kind'])+1,o['x'],int(o['y']*4) if o['kind']=='Zip' else o['y'],int(o['props'].get('arg',0))) for o in objects if o['kind']!='Player']
    spawns.sort(key=lambda o:(o[1],o[2]))
    if len(spawns)>constant('MAX_SPAWNS'): raise ValueError(f'{m.path}: too many spawns')
    write(a.out/f'leg{i}.spn',bytes(v for o in spawns for v in o))
    p=players[0]
    lines.append('    { %d, %d, %d, %d, %d, %d, %s },' % (len(spawns),m.w//40,p['x'],p['y'],m.props['speed'],m.props['sky'],c_string(m.props['name'])))
lines.append('};');write(a.out/'map_data.c','\n'.join(lines)+'\n')
write(a.out/'sky.ztm',Map(ROOT/'assets/legs/sky.tmx',(80,15)).tiles(a))
