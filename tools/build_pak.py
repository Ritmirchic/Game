"""SPACE MINER — asset pack builder.

Packs generated textures, sprite sheets, font atlases and PCM audio into a
single compressed blob that the game embeds in the executable.

Container: 'SMPK' + version + entry count + entries + compressed data.
Each payload is zlib-compressed (real deflate, decoded by src/core/inflate.c).
"""
import json
import os
import struct
import sys
import zlib
import wave

MAGIC = 0x4B504D53  # 'SMPK'
VERSION = 2
NAME_LEN = 48
PAK_IMAGE, PAK_PCM, PAK_BLOB = 0, 1, 2


def read_png_rgba(path):
    from PIL import Image
    im = Image.open(path)
    im = im.convert('RGBA')
    return im


def image_entry(path, name, frames=1):
    im = read_png_rgba(path)
    w, h = im.size
    assert frames >= 1
    assert w % frames == 0, f'{path}: width {w} not divisible by {frames}'
    fw = w // frames
    raw = im.tobytes()
    return dict(name=name, kind=PAK_IMAGE, w=fw, h=h, frames=frames, raw=raw)


def audio_entry(path, name, loop_ok=0):
    w = wave.open(path, 'rb')
    ch = w.getnchannels()
    assert w.getsampwidth() == 2, 'only 16-bit PCM supported'
    raw = w.readframes(w.getnframes())
    w.close()
    return dict(name=name, kind=PAK_PCM, w=ch, h=loop_ok, frames=1, raw=raw)


def blob_entry(name, data):
    if isinstance(data, str):
        data = data.encode('utf-8')
    return dict(name=name, kind=PAK_BLOB, w=0, h=0, frames=1, raw=data)


def write_pak(entries, out_path, manifest_path=None, level=9):
    entries = sorted(entries, key=lambda e: e['name'])
    blob = bytearray()
    table = []
    for e in entries:
        comp = zlib.compress(e['raw'], level)
        table.append((e, comp))
        blob += comp
    header = struct.pack('<III', MAGIC, VERSION, len(entries))
    ent_bytes = bytearray()
    offset = 0
    for e, comp in table:
        nm = e['name'].encode('utf-8')[:NAME_LEN - 1]
        nm = nm + b'\0' * (NAME_LEN - len(nm))
        ent_bytes += struct.pack('<48sIIIIIII', nm, e['w'], e['h'], e['frames'], e['kind'],
                                 len(e['raw']), len(comp), offset)
        offset += len(comp)
    with open(out_path, 'wb') as f:
        f.write(header)
        f.write(bytes(ent_bytes))
        f.write(bytes(blob))
    if manifest_path:
        with open(manifest_path, 'w') as f:
            for e, comp in table:
                f.write(f"{e['name']:<28} {e['kind']:<2} {e['w']:>5}x{e['h']:<5} frames={e['frames']:<3} "
                        f"raw={len(e['raw']):>9} comp={len(comp):>9}\n")
    total = os.path.getsize(out_path)
    print(f'packed {len(entries)} entries -> {out_path} ({total/1048576:.2f} MB)')
    return total


def collect(gen_dir, audio_dir):
    """Assemble the full asset list from the generator output."""
    E = []

    def img(name, frames=1, file=None):
        p = os.path.join(gen_dir, (file or name) + '.png')
        if not os.path.exists(p):
            print('  missing', p)
            return
        E.append(image_entry(p, name, frames))

    # --- ships: 32 baked rotation frames ---
    for i, n in enumerate(['ship_prospector', 'ship_boreas', 'ship_vanguard', 'ship_chimera']):
        img(n, 32)
    for n in ['enemy_scout', 'enemy_raider', 'enemy_heavy', 'enemy_hunter', 'enemy_sentinel']:
        img(n, 32)
    for n in ['drone_harvester', 'drone_excavator']:
        img(n, 32)
    # --- asteroids: 16 frames each ---
    for i in range(8):
        img(f'ast_{i}', 16)
    for i in range(3):
        img(f'ast_crack_{i}')
    # --- backgrounds ---
    for i in range(5):
        img(f'bg_nebula{i}')
        img(f'sky_{i}')
    for i in range(5):
        img(f'planet_{i}')
    img('station')
    # --- fx ---
    img('fx_glow'); img('fx_dust'); img('fx_smoke', 12); img('fx_spark', 6)
    img('fx_flame_blue', 10); img('fx_flame_orange', 10); img('fx_flame_green', 10); img('fx_flame_violet', 10)
    img('fx_chunk')
    img('proj_bolt'); img('proj_plasma'); img('proj_enemy'); img('proj_heavy')
    img('proj_lance'); img('proj_missile')
    # --- resources / icons ---
    res = ['res_iron', 'res_copper', 'res_nickel', 'res_ice', 'res_titanium', 'res_silver',
           'res_salvage', 'res_gold', 'res_iridium', 'res_platinum', 'res_uranium', 'res_crystal',
           'res_plasma', 'res_antimatter', 'res_voidcrystal', 'res_darkmatter']
    for n in res:
        img(n)
    glyphs = ['upg_laser', 'upg_speed', 'upg_cargo', 'upg_engine', 'upg_accel', 'upg_hull',
              'upg_shield', 'upg_energy', 'upg_scanner', 'upg_drone', 'upg_weapon', 'upg_tractor',
              'ui_drone', 'ui_ship', 'ui_cargo', 'ui_mining', 'ui_shield', 'ui_energy', 'ui_skull',
              'ui_star', 'ui_arrow', 'ui_lock', 'ui_cross', 'ui_warn', 'ui_ring', 'ui_glow']
    for n in glyphs:
        img(n)
    img('logo')
    # --- fonts ---
    for n in ['font_tiny', 'font_small', 'font_body', 'font_head', 'font_title', 'font_big']:
        img(n)
        with open(os.path.join(gen_dir, n + '.txt')) as f:
            E.append(blob_entry(n + '_meta', f.read()))
    # --- audio ---
    if os.path.isdir(audio_dir):
        for fn in sorted(os.listdir(audio_dir)):
            if not fn.endswith('.wav'):
                continue
            n = fn[:-4]
            loop_ok = 1 if (n.startswith('music_') or n in ('sfx_mine', 'sfx_thruster', 'sfx_drone')) else 0
            E.append(audio_entry(os.path.join(audio_dir, fn), n, loop_ok))
    return E


if __name__ == '__main__':
    gen = sys.argv[1] if len(sys.argv) > 1 else 'assets/gen'
    out = sys.argv[2] if len(sys.argv) > 2 else 'assets/assets.pak'
    entries = collect(gen, os.path.join(gen, 'audio'))
    write_pak(entries, out, os.path.join(os.path.dirname(out) or '.', 'pak_manifest.txt'))
