#!/usr/bin/env python3
"""
Counter stream DMA reader.

Runs dmaRead as a subprocess and parses frame metadata to push
frame count and rate into softIocPVX via loopback CA using pyepics.
"""

import os
import re
import subprocess
import sys

import ctypes

EPICS_LIB = '/opt/epics/epics-base/lib/linux-aarch64'
os.environ.setdefault('PYEPICS_LIBCA', f'{EPICS_LIB}/libca.so')

# Pre-load EPICS dependencies so dlopen can resolve them
ctypes.CDLL(f'{EPICS_LIB}/libCom.so', mode=ctypes.RTLD_GLOBAL)
ctypes.CDLL(f'{EPICS_LIB}/libca.so',  mode=ctypes.RTLD_GLOBAL)

import epics

PV_FRAME_COUNT = 'ZCCM:COUNTER:FRAME_COUNT'

# Run dmaRead in a loop, accepting all destinations, PRBS check disabled
proc = subprocess.Popen(
    ['dmaRead', '--prbsdis'],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1)

# Regex for lines like:
# Read ret=16384, Dest=0, Fuser=0x02, Luser=0x02, prbs=0, count=45774
pattern = re.compile(r'count=(\d+)')

print("Counter stream reader started", flush=True)

for line in proc.stdout:
    line = line.strip()
    m = pattern.search(line)
    if m:
        count = int(m.group(1))
        epics.caput(PV_FRAME_COUNT, count, wait=False)
        if count % 1000 == 0:
            print(f"Frame count: {count}", flush=True)
