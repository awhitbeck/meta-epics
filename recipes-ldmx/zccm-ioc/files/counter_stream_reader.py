#!/usr/bin/env python3
"""
Counter stream DMA reader.

Runs dmaFrameStream as a subprocess.  That binary reads raw DMA frames
and writes each one to stdout as a binary record:

    [ uint8_t  dest             ]  -- tDest (stream index 0-3)
    [ uint8_t  pad[3]           ]  -- alignment padding
    [ uint32_t nsamples         ]  -- number of samples
    [ uint32_t sample[0..N-1]   ]  -- sample values

This script reads those binary records and routes each frame to
per-stream EPICS PVs:
    ZCCM:COUNTER:SAMPLES:0..3     waveform
    ZCCM:COUNTER:FRAME_COUNT:0..3 longin
"""

import os
import struct
import subprocess
import ctypes

EPICS_LIB = '/opt/epics/epics-base/lib/linux-aarch64'
os.environ.setdefault('PYEPICS_LIBCA', f'{EPICS_LIB}/libca.so')

# Pre-load EPICS dependencies so dlopen can resolve them
ctypes.CDLL(f'{EPICS_LIB}/libCom.so', mode=ctypes.RTLD_GLOBAL)
ctypes.CDLL(f'{EPICS_LIB}/libca.so',  mode=ctypes.RTLD_GLOBAL)

import epics

NUM_STREAMS = 4
PV_SAMPLES     = [f'ZCCM:COUNTER:SAMPLES:{i}'     for i in range(NUM_STREAMS)]
PV_FRAME_COUNT = [f'ZCCM:COUNTER:FRAME_COUNT:{i}' for i in range(NUM_STREAMS)]

proc = subprocess.Popen(
    ['dmaFrameStream'],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE)

pipe = proc.stdout
frame_counts = [0] * NUM_STREAMS

print("Counter stream reader started", flush=True)

while True:
    # Read 8-byte header: dest (1 byte) + pad (3 bytes) + nsamples (4 bytes)
    hdr = pipe.read(8)
    if len(hdr) < 8:
        print("dmaFrameStream closed stdout, exiting", flush=True)
        break

    dest = hdr[0]
    nsamples, = struct.unpack_from('<I', hdr, 4)

    # Read nsamples * 4 bytes of sample data
    nbytes = nsamples * 4
    data = pipe.read(nbytes)
    if len(data) < nbytes:
        print("Short read on frame data, exiting", flush=True)
        break

    samples = list(struct.unpack(f'<{nsamples}I', data))

    # Route to per-stream PVs; ignore unexpected dest values
    if dest < NUM_STREAMS:
        frame_counts[dest] += 1
        epics.caput(PV_SAMPLES[dest],     samples,             wait=False)
        epics.caput(PV_FRAME_COUNT[dest], frame_counts[dest],  wait=False)

        if frame_counts[dest] % 1000 == 0:
            print(f"Stream {dest}  frame_count={frame_counts[dest]}  nsamples={nsamples}", flush=True)
