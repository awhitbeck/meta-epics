/**
 * dmaFrameStream.cpp
 *
 * Reads DMA frames from /dev/axi_stream_dma_0 and writes each frame to
 * stdout as a binary record:
 *
 *   [ uint8_t  dest             ]  -- tDest (stream index 0-3)
 *   [ uint8_t  pad[3]           ]  -- alignment padding
 *   [ uint32_t nsamples         ]  -- number of samples
 *   [ uint32_t sample[0..N-1]   ]  -- counter/PRBS sample values
 *
 * Frame layout (SsiPrbsTx, 128-bit AXI Stream words):
 *   word 0  : seed (ignored)
 *   word 1  : packetLength (number of data words that follow)
 *   word 2+ : counter/PRBS values in the low 32-bits of each 128-bit word
 *
 * Usage:
 *   dmaFrameStream                  # reads from /dev/axi_stream_dma_0, all dests
 *   dmaFrameStream -p /dev/axi_stream_dma_1
 *   dmaFrameStream --stats          # print per-dest stats to stderr every second, no stdout
 **/

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>

#include <DmaDriver.h>
#include <AxisDriver.h>

static const char    *DEV_PATH        = "/dev/axi_stream_dma_0";
static const uint32_t MAX_FRAME_BYTES = 1024 * 1024 * 2;
static const uint32_t WORD_STRIDE     = 16;   /* 128-bit AXI word = 16 bytes */
static const uint32_t HDR_WORDS       = 2;    /* seed + packetLength */
static const uint32_t MAX_DEST        = 256;

/* Per-destination statistics */
typedef struct {
    uint64_t frames;
    uint64_t bytes;
} DestStats;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    const char *path      = DEV_PATH;
    int         stats_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            path = argv[++i];
        } else if (strcmp(argv[i], "--stats") == 0) {
            stats_mode = 1;
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

    DestStats cur[MAX_DEST]  = {};
    DestStats prev[MAX_DEST] = {};
    double    last_print     = now_sec();

    fd_set         fds;
    struct timeval timeout;

    for (;;) {
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        timeout.tv_sec  = 1;
        timeout.tv_usec = 0;

        int r = select(s + 1, &fds, NULL, NULL, &timeout);

        /* In stats mode, print a report every ~1 second regardless */
        if (stats_mode) {
            double t = now_sec();
            double dt = t - last_print;
            if (dt >= 1.0) {
                fprintf(stderr, "--- %.1f s ---\n", t);
                for (uint32_t d = 0; d < MAX_DEST; d++) {
                    if (cur[d].frames == 0 && prev[d].frames == 0) continue;
                    uint64_t df = cur[d].frames - prev[d].frames;
                    uint64_t db = cur[d].bytes  - prev[d].bytes;
                    fprintf(stderr,
                        "  dest %3u : %6lu frames/s  %8.3f MB/s  (total frames=%lu)\n",
                        d,
                        (unsigned long)(df / dt),
                        (double)db / dt / 1e6,
                        (unsigned long)cur[d].frames);
                    prev[d] = cur[d];
                }
                fflush(stderr);
                last_print = t;
            }
        }

        if (r <= 0) continue;  /* timeout or error — keep looping */

        uint32_t rxFlags = 0, rxDest = 0;
        int32_t ret = dmaRead(s, rxData, MAX_FRAME_BYTES, &rxFlags, NULL, &rxDest);
        if (ret <= 0) continue;

        uint32_t nwords = (uint32_t)ret / WORD_STRIDE;
        if (nwords <= HDR_WORDS) continue;

        uint32_t nsamples = nwords - HDR_WORDS;
        uint32_t *words   = (uint32_t *)rxData;
        uint8_t   dest    = (uint8_t)(rxDest & 0xFF);

        /* Update stats */
        cur[dest].frames++;
        cur[dest].bytes += (uint64_t)ret;

        if (stats_mode) continue;  /* no stdout output in stats mode */

        /* Write record header: dest (1 byte) + pad (3 bytes) + nsamples (4 bytes) */
        uint8_t pad[3] = {0, 0, 0};
        if (fwrite(&dest,     sizeof(uint8_t),  1, stdout) != 1) return 0;
        if (fwrite(pad,       sizeof(uint8_t),  3, stdout) != 3) return 0;
        if (fwrite(&nsamples, sizeof(uint32_t), 1, stdout) != 1) return 0;

        /* Write one uint32_t per sample (low word of each 128-bit AXI word) */
        for (uint32_t i = HDR_WORDS; i < nwords; i++) {
            uint32_t sample = words[i * 4];
            if (fwrite(&sample, sizeof(uint32_t), 1, stdout) != 1) return 0;
        }
        fflush(stdout);
    }

    free(rxData);
    close(s);
    return 0;
}
