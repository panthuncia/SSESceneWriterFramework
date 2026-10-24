"""Program-wide scan of an unpacked SkyrimSE.exe for the store families' sites.

    python store_scan.py <SkyrimSE.exe.unpacked.exe> [hidden|fade]

Disassembles .text linearly (capstone), restarting at every .pdata function start, so code no unwind entry names (leaf functions)
is covered too. Prints table entries (src/tables/*.cpp's HiddenStoreSite / StoreValueSite rows; the comments name the instruction
and the function's start, which the tables replace with a name where one is known):

- hidden: every write to `[reg + 0xf4]` (NiAVObject::flags) that can change bit 0: an `or` that may set it, an `and` that may
  clear it, a `mov` of any width. RSP-based stores are left out (the stub's call moves RSP), as are `add`/`inc` counters.
- fade: every `mov dword [reg + 0x130], value` (BSFadeNode::currentFade) followed, within four instructions, by an `or`/`and` of
  bit 14 of `[reg + 0xf4]` on the same register: the inline fade resets. One of them is the fade update's own (hooked by the fade
  calls family), which the table leaves out.
"""
import bisect
import struct
import sys

import capstone
from capstone import x86


class Image:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        e = struct.unpack_from('<I', self.d, 0x3C)[0]
        count = struct.unpack_from('<H', self.d, e + 6)[0]
        s = e + 24 + struct.unpack_from('<H', self.d, e + 20)[0]
        self.sections = []
        for _ in range(count):
            name = self.d[s:s + 8].rstrip(b'\0').decode()
            vsize, va, rsize, raw = struct.unpack_from('<IIII', self.d, s + 8)
            self.sections.append((name, va, max(vsize, rsize), raw, rsize))
            s += 40

    def read(self, rva, n):
        for _, va, size, raw, rsize in self.sections:
            if va <= rva < va + size:
                o = rva - va
                return self.d[raw + o:raw + min(o + n, rsize)]
        return b''


class Functions:
    """Function starts: the .pdata entry holding an address (a chained entry followed to its primary one), or for a leaf function
    the 16-aligned address after the int3 padding before it."""

    def __init__(self, img):
        s = [x for x in img.sections if x[0] == '.pdata'][0]
        raw = img.read(s[1], s[2])
        self.rows = sorted({struct.unpack_from('<III', raw, i) for i in range(0, len(raw) - 11, 12)} - {(0, 0, 0)})
        self.begins = [r[0] for r in self.rows]
        self.img = img

    def _primary(self, row):
        for _ in range(16):
            header = self.img.read(row[2], 4)
            if not header or not (header[0] >> 3) & 4:
                break
            row = struct.unpack('<III', self.img.read(row[2] + 4 + ((header[2] + 1) & ~1) * 2, 12))
        return row[0]

    def start(self, off):
        i = bisect.bisect_right(self.begins, off) - 1
        if i >= 0 and self.rows[i][0] <= off < self.rows[i][1]:
            return self._primary(self.rows[i])
        code = self.img.read(off - 0x800, 0x800)
        for p in range(len(code) - 1, 0, -1):
            if (off - 0x800 + p) % 16 == 0 and code[p - 1] == 0xCC and code[p] != 0xCC:
                return off - 0x800 + p
        return off


def instructions(img, functions):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    md.skipdata = True
    for name, va, size, raw, rsize in img.sections:
        if name != '.text':
            continue
        code = img.d[raw:raw + min(size, rsize)]
        cuts = sorted({va} | {b for b in functions.begins if va <= b < va + len(code)}) + [va + len(code)]
        for lo, hi in zip(cuts, cuts[1:]):
            for ins in md.disasm(code[lo - va:hi - va], lo):
                if ins.id:
                    yield ins


def store_to(ins, disp):
    op = ins.operands[0] if ins.operands else None
    return op is not None and op.type == x86.X86_OP_MEM and op.mem.disp == disp and op.mem.base not in (0, x86.X86_REG_RIP)


def hidden(ins):
    if not store_to(ins, 0xF4) or ins.operands[0].mem.base == x86.X86_REG_RSP:
        return False
    if ins.mnemonic == 'mov':
        return True
    if ins.mnemonic not in ('or', 'and'):
        return False
    src = ins.operands[1]
    imm = src.imm & 0xFFFFFFFF if src.type == x86.X86_OP_IMM else None
    return (ins.mnemonic == 'or' and (imm is None or imm & 1)) or (ins.mnemonic == 'and' and (imm is None or not imm & 1))


def register(ins, reg):
    return 'Operand::' + ins.reg_name(reg).upper()


def main():
    img = Image(sys.argv[1])
    kind = sys.argv[2] if len(sys.argv) > 2 else 'hidden'
    functions = Functions(img)
    window = []
    for ins in instructions(img, functions):
        if kind == 'hidden' and hidden(ins):
            print('\t\t\t{ %#x, { %s }, %d, %s, %s },  // FUN_%x: %s %s' % (ins.address, ', '.join('0x%02x' % b for b in ins.bytes),
                ins.size, register(ins, ins.operands[0].mem.base), 'true' if ins.mnemonic in ('or', 'and') else 'false',
                0x140000000 + functions.start(ins.address), ins.mnemonic.upper(), ins.op_str))
        if kind == 'fade':
            window = (window + [ins])[-5:]
            if ins.mnemonic in ('or', 'and') and store_to(ins, 0xF4) and ins.operands[1].type == x86.X86_OP_IMM and \
                    ins.operands[1].imm & 0xFFFFFFFF in (0x4000, 0xFFFFBFFF):
                for p in window[:-1]:
                    if p.mnemonic == 'mov' and store_to(p, 0x130) and p.operands[0].size == 4 and \
                            p.operands[0].mem.base == ins.operands[0].mem.base:
                        print('\t\t\t{ %#x, { %s }, %d, ... },  // FUN_%x: %s %s' % (p.address, ', '.join('0x%02x' % b for b in p.bytes),
                            p.size, 0x140000000 + functions.start(p.address), p.mnemonic.upper(), p.op_str))


if __name__ == '__main__':
    main()
