#!/usr/bin/env python3
"""Convierte los assets del juego web a los formatos binarios que lee el port de 3DS.
   Uso: python3 convert_assets.py <carpeta_juego_web> <carpeta_romfs>"""
import sys, os, json, struct
import numpy as np
from PIL import Image, ImageDraw, ImageFont

SRC = sys.argv[1] if len(sys.argv) > 1 else '/home/claude/game'
DST = sys.argv[2] if len(sys.argv) > 2 else '/home/claude/port/romfs'
os.makedirs(DST, exist_ok=True)
CHARS = [  # id, altura (de data.js), escenario
    ('sanchez', 425, 'moncloa'), ('feijoo', 410, 'senado'), ('ayuso', 395, 'sol'),
    ('almeida', 355, 'ayuntamiento'), ('rajoy', 420, 'cibeles')]
POSES = ['idle','idle2','walk','w1','w2','w3','w4','crouch','jump','block','hit','fall','ko','getup',
         'win','punch','kick','low','air','sp1','sp2','fa5','super','proj']
WORLD = 1/3.0                      # 1280x720 del juego web -> 400x240 de la 3DS

s = open(f'{SRC}/js/sprites-meta.js').read()
a = s.index('window.SPRITE_META = ') + len('window.SPRITE_META = '); b = s.index(';\nwindow.PROP_META')
META = json.loads(s[a:b])

def scale_sprite(im, f):
    """reescala con alfa premultiplicado y alfa duro (sin halos)"""
    im = im.convert('RGBA'); w, h = im.size; nw, nh = max(1, round(w*f)), max(1, round(h*f))
    arr = np.asarray(im).astype(np.float32); al = arr[..., 3:4]/255.0
    pm = Image.fromarray(np.concatenate([arr[..., :3]*al, arr[..., 3:4]], 2).astype(np.uint8), 'RGBA')
    pm = pm.resize((nw, nh), Image.LANCZOS if f < 1 else Image.BICUBIC)
    p = np.asarray(pm).astype(np.float32); A = p[..., 3:4]/255.0
    rgb = np.where(A > 0.02, p[..., :3]/np.maximum(A, 0.02), 0).clip(0, 255)
    out = np.concatenate([rgb, (A > 0.5)*255.0], 2).astype(np.uint8)
    return Image.fromarray(out, 'RGBA')

blob = bytearray(b'SPR1' + struct.pack('<I', len(CHARS)))
for cid, height, stage in CHARS:
    m = META[cid]; k = height / m['idle']['h']; f = k * 4 * WORLD     # píxeles del PNG -> píxeles 3DS
    poses = []
    for p in POSES:
        fp = f'{SRC}/assets/chars/{cid}/{p}.webp'
        if not os.path.exists(fp) or p not in m: continue
        im = Image.open(fp).convert('RGBA')
        if p == 'proj': sc = scale_sprite(im, 34.0/max(im.size)); ax = sc.width//2
        else: sc = scale_sprite(im, f); ax = round(m[p]['ax']/4*f)
        poses.append((p, sc, ax))
    poses.append(('face', scale_sprite(Image.open(f'{SRC}/assets/chars/{cid}/face.webp'), 32/44.0), 16))
    blob += struct.pack('<I', len(poses))
    for name, im, ax in poses:
        w, h = im.size; blob += name.encode().ljust(12, b'\0') + struct.pack('<HHhH', w, h, ax, 0)   # cabecera de 20 bytes (alineada a 4)
        arr = np.asarray(im); bgra = np.stack([arr[..., 2], arr[..., 1], arr[..., 0], arr[..., 3]], 2)
        blob += bgra[::-1].transpose(1, 0, 2).copy().tobytes()      # [x][y de abajo a arriba]: igual que el framebuffer
    print(cid, len(poses), 'poses, escala', round(f, 3))
open(f'{DST}/sprites.bin', 'wb').write(blob); print('sprites.bin', len(blob)//1024, 'KB')

# fondo: un unico escenario (Cibeles), columna a columna, de abajo arriba, BGR (formato nativo del framebuffer 3DS)
im = Image.open(f'{SRC}/assets/stages/cibeles.png').convert('RGB'); w = round(im.width*240/im.height)
im = im.resize((w, 240), Image.LANCZOS); arr = np.asarray(im)
fb = arr[::-1, :, ::-1].transpose(1, 0, 2).copy()
open(f'{DST}/stage.bin', 'wb').write(struct.pack('<HH', w, 240) + fb.tobytes()); print('stage cibeles', w)

# fuente 8x12 monoespaciada (ASCII 32..126), 1 byte por fila
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf', 11)
fb = bytearray()
for c in range(32, 127):
    g = Image.new('L', (8, 12), 0); ImageDraw.Draw(g).text((0, -1), chr(c), fill=255, font=font)
    for row in np.asarray(g) > 110: fb.append(sum(1 << (7-i) for i in range(8) if row[i]))
open(f'{DST}/font.bin', 'wb').write(bytes(fb))

# icono 48x48, banner 256x128 y audio mudo para el CIA
G = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gfx'); os.makedirs(G, exist_ok=True)
face = Image.open(f'{SRC}/assets/chars/sanchez/face.webp').convert('RGBA').resize((48, 48), Image.NEAREST)
ic = Image.new('RGBA', (48, 48), (20, 18, 46, 255)); ic.alpha_composite(face); ic.save(f'{G}/icon.png')
bn = Image.new('RGBA', (256, 128), (20, 18, 46, 255)); d = ImageDraw.Draw(bn)
for i, (cid, _, _) in enumerate(CHARS):
    fc = Image.open(f'{SRC}/assets/chars/{cid}/face.webp').convert('RGBA').resize((44, 44), Image.NEAREST); bn.alpha_composite(fc, (10+i*47, 76))
ft = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf', 24)
d.text((14, 12), "MONCLOA'S", fill=(255, 200, 40, 255), font=ft); d.text((14, 40), 'FIGHTER 3DS', fill=(255, 90, 40, 255), font=ft); bn.save(f'{G}/banner.png')
import wave
w = wave.open(f'{G}/audio.wav', 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728); w.writeframes(b'\0\0\0\0'*32728*2); w.close()
print('ok')
