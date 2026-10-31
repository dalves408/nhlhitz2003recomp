"""Project-local pipeline adapter; leave the xboxrecomp checkout unchanged.

The bounded Capstone CFG walk is adapted from the working NHL Hitz Pro
project. Game-specific instruction hooks are deliberately derived separately.
"""
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/xboxrecomp'))
from tools.recomp.disasm import Disassembler
from tools.recomp.lifter import Lifter

ORIGINAL_LIFT = Lifter.lift_instruction
ORIGINAL_LOOP = Lifter._lift_loop
MV30_COPY_LOOPS = (0x0004BC37, 0x0004BC67, 0x0004BCF9, 0x0004BD1D, 0x0004BD41)


def checked_loop(self, insn, zf_expr=None):
    # Basic-block lifting calls _lift_loop directly, bypassing
    # lift_instruction. These verified copy loops must retain every row.
    if insn.address in MV30_COPY_LOOPS:
        if insn.bytes_hex != 'e0ea' or insn.mnemonic not in ('loopne', 'loopnz'):
            raise ValueError('Unexpected MV30 block-copy loop instruction')
        zf_expr = '1'
    return ORIGINAL_LOOP(self, insn, zf_expr)


def checked_lift(self, insn):
    # Same request layout and instruction sequence as Pro's loader, verified
    # in Hitz 2003 sub_0013A320. Completion can run inline and free the request,
    # so publish its parse bounds before issuing the read, not afterwards.
    if insn.address == 0x0013A416:
        if insn.bytes_hex != 'e8a5630a00':
            raise ValueError('Unexpected Hitz 2003 resource-read instruction')
        pre = ['{ uint32_t _hdr = MEM32(esi + 0x218) + edi - 8;',
               '  MEM32(esi + 0x21C) = _hdr; MEM32(_hdr) = 0xFFFFFFFFu; MEM32(_hdr + 4) = ebx; }']
        return pre + ORIGINAL_LIFT(self, insn)
    if insn.address in (0x0013A41B, 0x0013A421, 0x0013A425,
                        0x0013A42B, 0x0013A431, 0x0013A43A):
        return ['/* request bounds published before the read; completion may have freed it */']
    if insn.mnemonic == 'wbinvd':
        return ['{ extern void hitz_guest_wbinvd(void); hitz_guest_wbinvd(); }']
    if any(op.type == 'reg' and op.reg and len(op.reg) == 3
           and op.reg[:2] in ('cr', 'dr') and op.reg[2].isdigit()
           for op in insn.operands):
        return self._unimplemented(insn, insn.mnemonic, ' (privileged CPU register)')
    result = ORIGINAL_LIFT(self, insn)
    # XDK 4531: device+0x34 is the completion word (not Pro's device+0x30).
    # Verified against the allocation and D3D fence readers at 0x0021B58C.
    if insn.address == 0x0021A873:
        if insn.bytes_hex != '894134':
            raise ValueError('Unexpected D3D semaphore binding instruction')
        result.append('{ extern void nv2a_pb_set_semaphore_target(uint32_t); nv2a_pb_set_semaphore_target(eax); }')
    return result


def iter_instructions(decoder, raw_bytes, start_va, end_va, entry):
    cursor = entry
    batch_count = 64
    while cursor < end_va:
        offset = cursor - start_va
        chunk = raw_bytes[offset:min(end_va - start_va, offset + 15 * batch_count)]
        count = 0
        for insn in decoder._cs.disasm(chunk, cursor, count=batch_count):
            count += 1
            cursor = insn.address + insn.size
            yield insn
        if count < batch_count:
            return


def bounded_cfg(self, raw_bytes, start_va, end_va, entry_points,
                stop_addresses=None, stop_mnemonics=()):
    if end_va <= start_va or end_va - start_va > len(raw_bytes):
        return []
    decoded = {}
    stop_addresses = set(stop_addresses or ())
    worklist = list(entry_points)
    while worklist:
        entry = worklist.pop()
        if not start_va <= entry < end_va or entry in decoded:
            continue
        for cs_insn in iter_instructions(self, raw_bytes, start_va, end_va, entry):
            if cs_insn.address != entry and cs_insn.address in stop_addresses:
                break
            if cs_insn.address >= end_va or cs_insn.address in decoded:
                break
            insn = self._decode_instruction(cs_insn)
            decoded[insn.address] = insn
            if (insn.is_cond_jump and insn.jump_target is not None
                    and start_va <= insn.jump_target < end_va):
                worklist.append(insn.jump_target)
            if insn.is_jump:
                if insn.jump_target is not None and start_va <= insn.jump_target < end_va:
                    worklist.append(insn.jump_target)
                break
            if insn.is_ret or insn.mnemonic in stop_mnemonics:
                break
    return [decoded[addr] for addr in sorted(decoded)]


if __name__ == '__main__':
    Disassembler.disassemble_cfg = bounded_cfg
    Lifter.lift_instruction = checked_lift
    Lifter._lift_loop = checked_loop
    module = sys.argv.pop(1)
    sys.argv[0] = module
    runpy.run_module(module, run_name='__main__')
