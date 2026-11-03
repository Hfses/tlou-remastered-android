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
