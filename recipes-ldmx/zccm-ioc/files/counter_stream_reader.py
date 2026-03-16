#!/usr/bin/env python3
"""
Counter stream DMA reader.

Reads 10 kHz counter frames from /dev/axi_stream_dma_0 and pushes them into
softIocPVX via loopback CA using pyepics.  Each frame contains
SAMPLES_PER_FRAME uint32 values; the frame rate is 10 Hz.
"""

import os
import struct
import epics

SAMPLES_PER_FRAME = 1000
WORD_BYTES        = 16    # 128-bit DMA bus = 16 bytes per AXI Stream word
FRAME_BYTES       = SAMPLES_PER_FRAME * WORD_BYTES
DMA_DEV           = '/dev/axi_stream_dma_0'
PV_SAMPLES        = 'ZCCM:COUNTER:SAMPLES'
PV_FRAME_COUNT    = 'ZCCM:COUNTER:FRAME_COUNT'

fd = os.open(DMA_DEV, os.O_RDWR)
frame_count = 0

print(f"Counter stream reader started — {SAMPLES_PER_FRAME} samples/frame")

while True:
    buf = os.read(fd, FRAME_BYTES)
    if len(buf) != FRAME_BYTES:
        print(f"Short read: {len(buf)} bytes (expected {FRAME_BYTES})")
        continue

    # Extract the 32-bit counter from the low word of each 128-bit AXI Stream word
    words = struct.unpack(f'<{SAMPLES_PER_FRAME * 4}I', buf)  # 4 uint32 per 128-bit word
    samples = list(words[i * 4] for i in range(SAMPLES_PER_FRAME))  # low 32 bits only
    frame_count += 1

    epics.caput(PV_SAMPLES,     samples,     wait=False)
    epics.caput(PV_FRAME_COUNT, frame_count, wait=False)

    if frame_count % 100 == 0:
        print(f"Frame {frame_count}: first={samples[0]:#010x} last={samples[-1]:#010x}")
