#!/usr/bin/env python3
"""Check that the installable APK really contains executable ARM64 preparation tools."""
import io
import struct
import sys
import zipfile


def arm64(data, name):
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError(f'{name} is not an ARM64 ELF')
    if b'/lib/ld-linux' in data or b'GLIBC_' in data:
        raise ValueError(f'{name} contains glibc references')


with zipfile.ZipFile(sys.argv[1]) as apk:
    arm64(apk.read('lib/arm64-v8a/libpython3.13.so'), 'libpython')
    with zipfile.ZipFile(io.BytesIO(apk.read('assets/tools.zip'))) as tools:
        for name in ('python/bin/python3.13', 'bin/orbis-pkg-util'):
            arm64(tools.read(name), name)
        for name in ('python/lib/python3.13/encodings/__init__.py',
                     'source/android/device_prepare.py', 'source/android/tlou_bionic.py',
                     'source/gow3_pc/scripts/prepare.py', 'source/gow3_pc/scripts/link_modules.py',
                     'source/gow3_pc/scripts/link_libc.py', 'source/gow3_pc/scripts/content_profile.py',
                     'source/tools/probe_tlou.py'):
            if not tools.read(name):
                raise ValueError(f'Missing preparation component {name}')
        for module in ('_struct', '_posixsubprocess', '_sha2'):
            if not any('/lib-dynload/'+module+'.' in name for name in tools.namelist()):
                raise ValueError(f'Missing target Python extension {module}')
print('APK contains Android ARM64 PKG extractor, CPython, required stdlib/modules and TLOU preparation scripts')
