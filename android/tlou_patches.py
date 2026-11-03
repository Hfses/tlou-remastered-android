"""Verified CUSA00552 executable profiles; every change still requires original bytes.

The eboot with entry 0x56d9f0 was supplied by the user on 2026-11-03.
Its profile is selected by complete SELF SHA-256, not an assumed APP_VER.
No player binary or game content is included here.
"""
import hashlib
import struct

BASE_SELF_SHA256 = "e5f0aa322f60bd90637cc10d83de08c77885b1921084021c630e04ed3d6febf7"


def rip_instruction(prefix, address, target):
    return (bytes.fromhex(prefix) + struct.pack("<i", target - address - 8)).hex()


BASE_PATCHES = {
    # WaitOnContextDone's compare and diagnostic both read 20 seconds rather than
    # 2. Reuse existing immutable constants; do not alter a shared constant pool.
    "frame-timeout": [
        (0x99285A, "c5f82e05b6e47900", rip_instruction("c5f82e05", 0x99285A, 0xEFDD88)),
        (0x992907, "c5fb100511e47900", rip_instruction("c5fb1005", 0x992907, 0x107CB00)),
        (0xEFDD88, "0000a041", None),
        (0x107CB00, "0000000000003440", None),
        (0x992862, "775a", None),  # retains the real timeout failure branch
    ],
    # Worker spin -> existing condition-variable wait after 1000 iterations;
    # keep the submit path broadcasting that same condition variable.
    "job-sleep": [
        (0xAACA3A, "7504", "eb04"),
        (0xAACA65, "7422", "9090"),
        (0xAABF91, "742d", "9090"),
        (0xAACA36, "807f5900", None),
        (0xAACA40, "4981f8e8030000", None),
        (0xAACA61, "80785900", None),
        (0xAACA75, "ba0a000000", None),
        (0xAABF8D, "807f5900", None),
        (0xAABFAF, "e86ca6ffff", None),
    ],
}


def select(eboot, fallback):
    digest = hashlib.sha256(eboot.read_bytes()).hexdigest()
    if digest == BASE_SELF_SHA256:
        print("Perfil TLOU verificado: entrada 0x56d9f0; patches próprios do executável.", flush=True)
        return dict(BASE_PATCHES)
    return fallback

# Adapted from Bachata S4+ P36 performance XMLs. Addresses are ELF-relative
# (Bachata guest base 0x400000 removed). Entire instruction guards were checked
# against the supplied SELF. 360p uses the same int->float conversion as 540p;
# obsolete XML bytes overwriting VMOVSS with packed dimensions are not used.
PERFORMANCE_EFFECTS = {'ssao': [[9724771, '01', '00'], [9724765, 'c7834b01000001010101', None]],
 'motion_blur': [[9724898, '01', '00'],
                 [9725988, '01', '00'],
                 [9724892, 'c6836d01000001', None],
                 [9725982, 'c6830e04000001', None]],
 'depth_of_field': [[9724926, '01', '00'], [9724920, 'c6837101000001', None]],
 'bloom': [[9725092, 'c783b401000001010101', 'c783b401000001000101']],
 'sunlight_shadows': [[9370914, '0f8583030000', 'e98403000090'],
                      [9370900, '488d05d595fa0080b89c000000000f8583030000', None]],
 'subsurface': [[9381008, '0f84320b0000', 'e9330b000090'],
                [9380998, '488b0080b8942e0000000f84320b0000', None]]}
PERFORMANCE_RESOLUTIONS = {'720': [[9504253, '7578', 'eb78'], [9504250, '83fe017578', None]],
 '540': [[9504253, '7578', 'eb78'],
         [9504250, '83fe017578', None],
         [9504378, '00050000', 'c0030000'],
         [9504385, 'd0020000', '1c020000'],
         [9504399, '00050000', 'c0030000'],
         [9504406, 'd0020000', '1c020000'],
         [9504454, '00050000', 'c0030000'],
         [9504461, 'd0020000', '1c020000'],
         [9504482, 'a0000000', '78000000'],
         [9504489, 'b4000000', '87000000'],
         [9504419, '00050000d0020000', 'c00300001c020000'],
         [9504427, 'c5fa1005554a8100', '0f57c0c5fa2a4708'],
         [9504435, 'c5fa100d514a8100', '0f57c9c5f22a4f0c'],
         [9504375, 'c7470800050000c7470cd0020000', None],
         [9504417, '48b800050000d0020000c5fa1005554a8100c5fa100d514a8100', None]],
 '360': [[9504253, '7578', 'eb78'],
         [9504250, '83fe017578', None],
         [9504378, '00050000', '80020000'],
         [9504385, 'd0020000', '68010000'],
         [9504399, '00050000', '80020000'],
         [9504406, 'd0020000', '68010000'],
         [9504454, '00050000', '80020000'],
         [9504461, 'd0020000', '68010000'],
         [9504482, 'a0000000', '50000000'],
         [9504489, 'b4000000', '5a000000'],
         [9504419, '00050000d0020000', '8002000068010000'],
         [9504427, 'c5fa1005554a8100', '0f57c0c5fa2a4708'],
         [9504435, 'c5fa100d514a8100', '0f57c9c5f22a4f0c'],
         [9504375, 'c7470800050000c7470cd0020000', None],
         [9504417, '48b800050000d0020000c5fa1005554a8100c5fa100d514a8100', None]]}


# The constructor is used before loading saved graphics settings. The old
# branch-only patch left it at 1080p, and modes 2/3 bypassed that branch entirely.
# Keep the selected dimensions coherent across initialization and every enum.
def complete_resolution_paths(changes, width, height):
    changes = [c for c in changes if c[0] not in (0x9105fd, 0x9105fa)]
    changes += [
        (0x9104f2, "83fe030f8484000000", "e98001000090909090"),
        (0x9104ec, "8988f4000000", None),  # retain mode bookkeeping
    ]
    for address, original, selected in [
        (0x9103bb,1920,width),(0x9103c2,1080,height),
        (0x9103d0,1920,width),(0x9103d7,1080,height),
        (0x9103e5,1920,width),(0x9103ec,1080,height),
        (0x91042b,1920,width),(0x910432,1080,height),
        (0x910401,240,width//8),(0x910408,270,height//4),
    ]:
        changes.append((address,struct.pack("<I",original).hex(),struct.pack("<I",selected).hex()))
    for address, original in [
        (0x9103b8,"c7470880070000c7470c38040000"),
        (0x9103cd,"c7471880070000c7471c38040000"),
        (0x9103e2,"c7472480070000c7472838040000"),
        (0x910428,"c7473880070000c7473c38040000"),
        (0x9103fe,"c74740f0000000c747440e010000"),
    ]:
        changes.append((address,original,None))
    return changes

for _name, (_width, _height) in {"720":(1280,720),"540":(960,540),"360":(640,360)}.items():
    PERFORMANCE_RESOLUTIONS[_name] = complete_resolution_paths(PERFORMANCE_RESOLUTIONS[_name],_width,_height)


def selected_options(options):
    options = options or {}
    if not isinstance(options, dict):
        raise ValueError("Configuração de patches inválida")
    resolution = options.get("resolution", "original")
    if not isinstance(resolution, str) or resolution not in {"original", "720", "540", "360"}:
        raise ValueError("Resolução não suportada")
    selected = {}
    if resolution != "original":
        selected["resolution-" + resolution] = PERFORMANCE_RESOLUTIONS[resolution]
    for name, changes in PERFORMANCE_EFFECTS.items():
        if type(options.get(name, False)) is not bool:
            raise ValueError("Opção de patch inválida: " + name)
        if options.get(name, False):
            selected["disable-" + name] = changes
    return selected


def write_selected(source, prepared, options):
    """Atomically publish launch patches; never change the player's SELF or linked image."""
    import importlib.util
    import json
    from pathlib import Path
    prepared, source = Path(prepared), Path(source)
    requested = selected_options(options)
    game_self = prepared / "tlou-view/eboot.bin"
    if not game_self.is_file():
        if requested:
            raise ValueError("Esta pasta preparada não contém o eboot original para validar os patches")
        return
    if hashlib.sha256(game_self.read_bytes()).hexdigest() != BASE_SELF_SHA256:
        if requested:
            raise ValueError("Patches disponíveis apenas para o eboot CUSA00552 1.00 validado; restaure o perfil Original")
        return
    spec = importlib.util.spec_from_file_location("tlou_patch_writer", source / "tools/probe_tlou.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    elf_path = prepared / "eboot.elf"
    if not elf_path.is_file():
        # Imported prepared folders may contain only the linked image. Rebuild
        # the inspection ELF from the validated SELF without relinking the game.
        spec = importlib.util.spec_from_file_location("tlou_owned_self", source / "gow3_pc/scripts/prepare.py")
        inspector = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(inspector)
        raw, _, _, _, _ = inspector.parse_self(game_self.read_bytes())
        inspection_temp = prepared / "eboot.elf.new"
        inspection_temp.write_bytes(raw)
        inspection_temp.replace(elf_path)
    elf = elf_path.read_bytes()
    phoff = struct.unpack_from("<Q", elf, 0x20)[0]
    size, count = struct.unpack_from("<HH", elf, 0x36)
    segments = [struct.unpack_from("<IIQQQQQQ", elf, phoff + i*size) for i in range(count)]
    def read(at, length):
        for kind, _, offset, start, _, filesz, _, _ in segments:
            if kind == 1 and start <= at and at + length <= start + filesz:
                return elf[offset + at-start:offset + at-start+length]
        return b""
    patches = dict(BASE_PATCHES)
    patches.update(requested)
    for name, changes in patches.items():
        for at, expected, _ in changes:
            if read(at, len(bytes.fromhex(expected))) != bytes.fromhex(expected):
                raise ValueError("Assinatura divergente no patch " + name + " em " + hex(at))
    temp = prepared / "patches.bin.new"
    module.image_patches(elf_path, temp, patches)
    temp.replace(prepared / "patches.bin")
    report = {"profile": BASE_SELF_SHA256, "selected": list(requested),
              "resolution": (options or {}).get("resolution", "original"), "validated": True}
    (prepared / "patch-validation.json").write_text(json.dumps(report, indent=2)+"\n")
    print("Patches validados: " + (", ".join(requested) or "Original; correções de estabilidade"), flush=True)
