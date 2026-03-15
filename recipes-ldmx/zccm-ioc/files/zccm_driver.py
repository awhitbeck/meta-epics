#!/usr/bin/env python3
"""
ZCCM register driver.

Reads registers from /dev/axi_memory_map via ioctl and pushes values into
softIocPVX via loopback CA using pyepics.  Also monitors writable PVs and
mirrors writes back to hardware.

Register map base: 0x04_8000_0000  (ZccmApplication in _ZccmRoot.py)
ioctl interface: DMA_Read_Register (0x100B) / DMA_Write_Register (0x100A)
struct: uint64_t address (absolute physical), uint32_t data
"""

import fcntl
import os
import struct
import time
import epics  # pyepics

# ---------------------------------------------------------------------------
# AXI register access via aximemorymap ioctl interface
# ---------------------------------------------------------------------------
#
# The aximemorymap driver exposes registers via ioctl, not read/write/mmap.
# Ioctl commands (from DmaDriver.h):
#   DMA_Write_Register = 0x100A
#   DMA_Read_Register  = 0x100B
# Argument struct (DmaRegisterData):
#   uint64_t address  (absolute physical address)
#   uint32_t data
# Total: 12 bytes, packed.

AXI_BASE           = 0x04_8000_0000
DMA_WRITE_REGISTER = 0x100A
DMA_READ_REGISTER  = 0x100B
_REG_FMT           = '=QI'   # little-endian uint64 + uint32, no padding

class AxiRegMap:
    def __init__(self, dev='/dev/axi_memory_map'):
        self.fd = os.open(dev, os.O_RDWR | os.O_SYNC)

    def read32(self, page_offset, word_offset):
        addr = AXI_BASE + page_offset + word_offset
        buf = bytearray(struct.calcsize(_REG_FMT))
        struct.pack_into(_REG_FMT, buf, 0, addr, 0)
        fcntl.ioctl(self.fd, DMA_READ_REGISTER, buf)
        _, data = struct.unpack_from(_REG_FMT, buf, 0)
        return data

    def write32(self, page_offset, word_offset, value):
        addr = AXI_BASE + page_offset + word_offset
        buf = bytearray(struct.pack(_REG_FMT, addr, value & 0xFFFFFFFF))
        fcntl.ioctl(self.fd, DMA_WRITE_REGISTER, buf)

    def rmw32(self, page_offset, word_offset, mask, value):
        r = self.read32(page_offset, word_offset)
        r = (r & ~mask) | (value & mask)
        self.write32(page_offset, word_offset, r)

    def close(self):
        os.close(self.fd)


# ---------------------------------------------------------------------------
# Write-back callbacks: network PV write -> hardware register
# ---------------------------------------------------------------------------

def make_bc0_mode_cb(regs):
    def cb(value, **kw):
        regs.write32(0x0B_0000, 0x0014, int(value) & 0x1)
    return cb

def make_rm1_pen_cb(regs):
    def cb(value, **kw):
        regs.rmw32(0x09_0000, 0x0000, mask=0x2, value=(1 if value else 0) << 1)
    return cb

def make_rm1_reset_cb(regs):
    def cb(value, **kw):
        regs.rmw32(0x09_0000, 0x0000, mask=0x4, value=(1 if value else 0) << 2)
    return cb

def make_rm2_pen_cb(regs):
    def cb(value, **kw):
        regs.rmw32(0x0A_0000, 0x0000, mask=0x2, value=(1 if value else 0) << 1)
    return cb

def make_rm2_reset_cb(regs):
    def cb(value, **kw):
        regs.rmw32(0x0A_0000, 0x0000, mask=0x4, value=(1 if value else 0) << 2)
    return cb


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    regs = AxiRegMap()

    # Monitor writable PVs and mirror to hardware
    epics.camonitor('ZCCM:BC0GEN:BC0_MODE', callback=make_bc0_mode_cb(regs))
    epics.camonitor('ZCCM:RM1:PEN',         callback=make_rm1_pen_cb(regs))
    epics.camonitor('ZCCM:RM1:RESET',       callback=make_rm1_reset_cb(regs))
    epics.camonitor('ZCCM:RM2:PEN',         callback=make_rm2_pen_cb(regs))
    epics.camonitor('ZCCM:RM2:RESET',       callback=make_rm2_reset_cb(regs))

    print("ZCCM driver running — polling hardware at 1 Hz")

    while True:
        try:
            # BC0 generator counters (read-only)
            epics.caput('ZCCM:BC0GEN:TRIG_COUNT',   regs.read32(0x0B_0000, 0x0000))
            epics.caput('ZCCM:BC0GEN:BC0_COUNT',    regs.read32(0x0B_0000, 0x0004))
            epics.caput('ZCCM:BC0GEN:BC0_FC_COUNT', regs.read32(0x0B_0000, 0x0008))
            epics.caput('ZCCM:BC0GEN:BC0_COUNTER',  regs.read32(0x0B_0000, 0x000C))
            epics.caput('ZCCM:BC0GEN:LED_COUNT',    regs.read32(0x0B_0000, 0x0010))

            # RM1 power/reset status (read-only bits)
            r1 = regs.read32(0x09_0000, 0x0000)
            epics.caput('ZCCM:RM1:PGOOD', int(bool(r1 & 0x1)))

            # RM2 power/reset status
            r2 = regs.read32(0x0A_0000, 0x0000)
            epics.caput('ZCCM:RM2:PGOOD', int(bool(r2 & 0x1)))

        except Exception as e:
            print(f"Driver poll error: {e}")

        time.sleep(1.0)


if __name__ == '__main__':
    main()
