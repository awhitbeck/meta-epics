/**
 * dmaFrameStream.cpp
 *
 * Reads DMA frames from /dev/axi_stream_dma_0 and writes each frame's
 * counter samples to stdout as a binary record:
 *
 *   [ uint32_t nsamples ][ uint32_t sample[0] ] ... [ uint32_t sample[nsamples-1] ]
 *
 * Frame layout (SsiPrbsTx counter mode, PRBS_INCREMENT_G=true):
 *   word 0  : seed (ignored)
 *   word 1  : packetLength (number of data words that follow, i.e. nsamples)
 *   word 2+ : counter values (low 32-bits of each 128-bit AXI word)
 *
 * Each 128-bit AXI word is 16 bytes = 4 x uint32_t.  The counter value sits
 * in the low word (bytes 0-3).  The driver delivers the full 128-bit words
 * so we step through the payload in 16-byte strides.
 *
 * Usage:
 *   dmaFrameStream            # reads from /dev/axi_stream_dma_0, all dests
 *   dmaFrameStream -p /dev/axi_stream_dma_1
 **/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include <DmaDriver.h>
#include <AxisDriver.h>

static const char    *DEV_PATH        = "/dev/axi_stream_dma_0";
static const uint32_t MAX_FRAME_BYTES = 1024 * 1024 * 2;
/* Each AXI-Stream 128-bit word = 16 bytes; counter is in the low uint32_t */
static const uint32_t WORD_STRIDE     = 16;
static const uint32_t HDR_WORDS       = 2;  /* seed + packetLength */

int main(int argc, char **argv) {
    const char *path = DEV_PATH;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            path = argv[++i];
        }
    }

    int s = open(path, O_RDWR);
    if (s < 0) {
        fprintf(stderr, "dmaFrameStream: cannot open %s\n", path);
        return 1;
    }

    /* Accept all destinations */
    uint8_t mask[DMA_MASK_SIZE];
    memset(mask, 0xFF, DMA_MASK_SIZE);
    dmaSetMaskBytes(s, mask);

    void *rxData = malloc(MAX_FRAME_BYTES);
    if (!rxData) {
        fprintf(stderr, "dmaFrameStream: malloc failed\n");
        close(s);
        return 1;
    }

    fd_set fds;
    struct timeval timeout;

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;

        int r = select(s + 1, &fds, NULL, NULL, &timeout);
        if (r <= 0) continue;  /* timeout or error — keep looping */

        uint32_t rxFlags = 0, rxDest = 0;
        int32_t ret = dmaRead(s, rxData, MAX_FRAME_BYTES, &rxFlags, NULL, &rxDest);
        if (ret <= 0) continue;

        /* Each frame word is 16 bytes wide (128-bit AXI word).
         * Word 0 = seed, word 1 = packetLength, words 2+ = counter samples.
         * The counter value is in the low uint32_t of each 128-bit word. */
        uint32_t nwords = (uint32_t)ret / WORD_STRIDE;
        if (nwords <= HDR_WORDS) continue;

        uint32_t nsamples = nwords - HDR_WORDS;
        uint32_t *words   = (uint32_t *)rxData;

        /* Write header: sample count */
        if (fwrite(&nsamples, sizeof(uint32_t), 1, stdout) != 1) return 0;

        /* Write one uint32_t per sample (low word of each 128-bit AXI word) */
        for (uint32_t i = HDR_WORDS; i < nwords; i++) {
            uint32_t sample = words[i * 4];  /* low uint32 of 128-bit word */
            if (fwrite(&sample, sizeof(uint32_t), 1, stdout) != 1) return 0;
        }
        fflush(stdout);
    }

    free(rxData);
    close(s);
    return 0;
}
