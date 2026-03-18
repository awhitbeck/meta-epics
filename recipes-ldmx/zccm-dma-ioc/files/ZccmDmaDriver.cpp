/**
 * ZccmDmaDriver.cpp
 *
 * asynPortDriver that reads DMA frames from /dev/axi_stream_dma_0 in a
 * background thread and posts sample data directly into EPICS waveform
 * records — no subprocess, no pipe, no CA round-trip.
 *
 * Record layout (per stream 0-3):
 *   ZCCM:COUNTER:SAMPLES:N     waveform (asynInt32Array, NELM=MAX_SAMPLES)
 *   ZCCM:COUNTER:FRAME_COUNT:N longin   (asynInt32)
 *
 * Frame layout from SsiPrbsTx (128-bit AXI Stream words):
 *   word 0  : seed (ignored)
 *   word 1  : packetLength
 *   word 2+ : counter/PRBS value in low uint32 of each 128-bit word
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <pthread.h>

#include <iocsh.h>
#include <epicsThread.h>
#include <asynPortDriver.h>
#include <epicsExport.h>

#include "DmaDriver.h"
#include "AxisDriver.h"

/* ------------------------------------------------------------------ */

static const uint32_t WORD_STRIDE  = 16;   /* 128-bit AXI word = 16 bytes  */
static const uint32_t HDR_WORDS    = 2;    /* seed + packetLength           */
static const uint32_t MAX_SAMPLES  = 1022; /* packetLength=1024 - 2 header  */
static const uint32_t MAX_FRAME_BYTES = (MAX_SAMPLES + HDR_WORDS) * WORD_STRIDE;
static const int      NUM_STREAMS  = 4;

/* asynPortDriver parameter indices */
enum {
    P_Samples0, P_Samples1, P_Samples2, P_Samples3,
    P_FrameCount0, P_FrameCount1, P_FrameCount2, P_FrameCount3,
    NUM_PARAMS
};

/* ------------------------------------------------------------------ */

class ZccmDmaDriver : public asynPortDriver {
public:
    ZccmDmaDriver(const char *portName, const char *devPath);
    void dmaThread();

private:
    const char *devPath_;
    int         dmaFd_;
    void       *rxBuf_;
    int         frameCount_[NUM_STREAMS];

    int samplesIdx_[NUM_STREAMS];
    int frameCountIdx_[NUM_STREAMS];
};

/* ------------------------------------------------------------------ */

static void dmaThreadC(void *arg) {
    ((ZccmDmaDriver *)arg)->dmaThread();
}

ZccmDmaDriver::ZccmDmaDriver(const char *portName, const char *devPath)
    : asynPortDriver(portName,
                     1,           /* maxAddr */
                     NUM_PARAMS,
                     asynInt32Mask | asynInt32ArrayMask | asynDrvUserMask,
                     asynInt32Mask | asynInt32ArrayMask,
                     ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                     1,           /* autoConnect */
                     0, 0),
      devPath_(devPath),
      dmaFd_(-1),
      rxBuf_(NULL)
{
    memset(frameCount_, 0, sizeof(frameCount_));

    createParam("SAMPLES_0",     asynParamInt32Array, &samplesIdx_[0]);
    createParam("SAMPLES_1",     asynParamInt32Array, &samplesIdx_[1]);
    createParam("SAMPLES_2",     asynParamInt32Array, &samplesIdx_[2]);
    createParam("SAMPLES_3",     asynParamInt32Array, &samplesIdx_[3]);
    createParam("FRAME_COUNT_0", asynParamInt32,      &frameCountIdx_[0]);
    createParam("FRAME_COUNT_1", asynParamInt32,      &frameCountIdx_[1]);
    createParam("FRAME_COUNT_2", asynParamInt32,      &frameCountIdx_[2]);
    createParam("FRAME_COUNT_3", asynParamInt32,      &frameCountIdx_[3]);

    dmaFd_ = open(devPath_, O_RDWR);
    if (dmaFd_ < 0) {
        fprintf(stderr, "ZccmDmaDriver: cannot open %s\n", devPath_);
        return;
    }

    uint8_t mask[DMA_MASK_SIZE];
    memset(mask, 0xFF, DMA_MASK_SIZE);
    dmaSetMaskBytes(dmaFd_, mask);

    rxBuf_ = malloc(MAX_FRAME_BYTES);
    if (!rxBuf_) {
        fprintf(stderr, "ZccmDmaDriver: malloc failed\n");
        close(dmaFd_);
        dmaFd_ = -1;
        return;
    }

    epicsThreadCreate("ZccmDmaThread",
                      epicsThreadPriorityMedium,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      dmaThreadC,
                      this);

    printf("ZccmDmaDriver: started on %s\n", devPath_);
}

/* ------------------------------------------------------------------ */

void ZccmDmaDriver::dmaThread() {
    fd_set         fds;
    struct timeval timeout;

    while (true) {
        if (dmaFd_ < 0) {
            epicsThreadSleep(1.0);
            continue;
        }

        FD_ZERO(&fds);
        FD_SET(dmaFd_, &fds);
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;

        int r = select(dmaFd_ + 1, &fds, NULL, NULL, &timeout);
        if (r <= 0) continue;

        uint32_t rxFlags = 0, rxDest = 0;
        int32_t  ret = dmaRead(dmaFd_, rxBuf_, MAX_FRAME_BYTES, &rxFlags, NULL, &rxDest);
        if (ret <= 0) continue;

        uint32_t nwords = (uint32_t)ret / WORD_STRIDE;
        if (nwords <= HDR_WORDS) continue;

        uint32_t  nsamples = nwords - HDR_WORDS;
        if (nsamples > MAX_SAMPLES) nsamples = MAX_SAMPLES;

        uint8_t dest = (uint8_t)(rxDest & 0xFF);
        if (dest >= NUM_STREAMS) continue;

        /* Extract one uint32 per sample from the low word of each 128-bit AXI word */
        uint32_t *words   = (uint32_t *)rxBuf_;
        epicsInt32 samples[MAX_SAMPLES];
        for (uint32_t i = 0; i < nsamples; i++) {
            samples[i] = (epicsInt32)words[(HDR_WORDS + i) * 4];
        }

        /* Post directly into EPICS records — no CA, no pipe */
        lock();
        doCallbacksInt32Array(samples, nsamples, samplesIdx_[dest], 0);
        frameCount_[dest]++;
        setIntegerParam(frameCountIdx_[dest], frameCount_[dest]);
        callParamCallbacks();
        unlock();
    }
}

/* ------------------------------------------------------------------ */
/* iocsh registration                                                  */
/* ------------------------------------------------------------------ */

static const iocshArg      cfgArg0    = { "portName", iocshArgString };
static const iocshArg      cfgArg1    = { "devPath",  iocshArgString };
static const iocshArg     *cfgArgs[]  = { &cfgArg0, &cfgArg1 };
static const iocshFuncDef  cfgFuncDef = { "ZccmDmaDriverConfigure", 2, cfgArgs };

static void cfgCallFunc(const iocshArgBuf *args) {
    new ZccmDmaDriver(args[0].sval, args[1].sval);
}

static void ZccmDmaDriverRegister(void) {
    iocshRegister(&cfgFuncDef, cfgCallFunc);
}

epicsExportRegistrar(ZccmDmaDriverRegister);
