#!/usr/bin/env python3
"""Compiler launcher for the AArch64 game library: give static data the alignment x86-64 GCC gives it.

Optimising x86-64 GCC aligns every static array or struct of 32 bytes or more to 32 bytes
(ix86_data_alignment). The game has come to rely on that: buffers the console DMAs into are asserted
to be 32-byte aligned (HSD_DevComRequest) and addresses are rounded with & ~31, yet most are declared
without an alignment of their own. AArch64 GCC aligns them to 8, and its cc1 takes no plugins.

The AArch64 build compiles with -fdata-sections -fno-section-anchors, so every static object sits in
a section of its own and no code depends on where its neighbours are. After the compiler has written
the object file, this raises the alignment of each such data section of 32 bytes or more to 32,
which is where the linker then places it. Merged constant and string sections hold many small items
and are left alone. Used as CMAKE_C_COMPILER_LAUNCHER (sourceport/game/CMakeLists.txt).
SPDX-License-Identifier: GPL-2.0-or-later
"""
import struct
import subprocess
import sys

ALIGN = 32
SHF_WRITE, SHF_ALLOC, SHF_MERGE, SHF_STRINGS = 0x1, 0x2, 0x10, 0x20
PREFIXES = ('.data.', '.bss.', '.rodata.', '.data.rel.ro.', '.data.rel.')


def raise_alignment(path):
    with open(path, 'r+b') as f:
        data = bytearray(f.read())
        if data[:4] != b'\x7fELF' or data[4] != 2 or data[5] != 1:
            return 0
        shoff, = struct.unpack_from('<Q', data, 0x28)
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 0x3A)
        if not shoff or shentsize != 64:
            return 0
        names_off = struct.unpack_from('<Q', data, shoff + shstrndx * 64 + 0x18)[0]
        changed = 0
        for i in range(shnum):
            at = shoff + i * 64
            name, kind, flags, _, _, size, _, _, align = struct.unpack_from('<IIQQQQIIQ', data, at)
            end = data.index(0, names_off + name)
            section = data[names_off + name:end].decode('ascii', 'replace')
            if (flags & SHF_ALLOC) and not (flags & (SHF_MERGE | SHF_STRINGS)) and size >= ALIGN and align < ALIGN \
                    and section.startswith(PREFIXES):
                struct.pack_into("<Q", data, at + 0x30, ALIGN)   # sh_addralign
                changed += 1
        if changed:
            f.seek(0)
            f.write(data)
        return changed


def main():
    command = sys.argv[1:]
    result = subprocess.run(command)
    if result.returncode:
        return result.returncode
    if '-c' in command and '-o' in command:
        out = command[command.index('-o') + 1]
        if out.endswith(('.o', '.obj')):
            raise_alignment(out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
