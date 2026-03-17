#!/usr/bin/env python3
"""
Counter stream DMA reader.

Runs dmaFrameStream as a subprocess.  That binary reads raw DMA frames
and writes each one to stdout as a binary record:

    [ uint32_t nsamples ][ uint32_t sample[0] ] ... [ uint32_t sample[N-1] ]

This script reads those binary records, pushes the waveform into
ZCCM:COUNTER:SAMPLES and increments ZCCM:COUNTER:FRAME_COUNT.
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

PV_SAMPLES     = 'ZCCM:COUNTER:SAMPLES'
PV_FRAME_COUNT = 'ZCCM:COUNTER:FRAME_COUNT'

proc = subprocess.Popen(
    ['dmaFrameStream'],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE)

pipe = proc.stdout
frame_count = 0

print("Counter stream reader started", flush=True)

while True:
    # Read the 4-byte sample count header
    hdr = pipe.read(4)
    if len(hdr) < 4:
        print("dmaFrameStream closed stdout, exiting", flush=True)
        break

    nsamples, = struct.unpack('<I', hdr)

    # Read nsamples * 4 bytes of sample data
    nbytes = nsamples * 4
    data = pipe.read(nbytes)
    if len(data) < nbytes:
        print("Short read on frame data, exiting", flush=True)
        break

    samples = list(struct.unpack(f'<{nsamples}I', data))

    frame_count += 1
    epics.caput(PV_SAMPLES,     samples,     wait=False)
    epics.caput(PV_FRAME_COUNT, frame_count, wait=False)

    if frame_count % 1000 == 0:
        print(f"Frame count: {frame_count}  nsamples={nsamples}", flush=True)
