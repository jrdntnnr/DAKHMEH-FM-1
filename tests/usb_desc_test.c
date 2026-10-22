/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 DAKHMEH (after SLOOP's tests/uac_test.c, isod89) */
/* The USB configuration descriptor as a host reads it: the audio control header's collection must list the audio
 * streaming interface before the MIDI one (SLOOP 2.7.2: macOS up to 15 drops the MIDI interface while it walks the
 * collection and gives up before the audio input; listed last, MIDI is dropped at the end), and bcdDevice must say
 * so (+0.08), since hosts cache descriptors by version. The SIE is never touched. */
#include <stdint.h>
#include <stdio.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 256u
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 1
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/src/usb.c"

static int fails;
static void check(const char *what, int ok) { printf("usb: %-70s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

int main(void)
{
    const uint8_t *d = CFG_DESC, *end = CFG_DESC + sizeof CFG_DESC;
    uint8_t if_sub[16] = {0}, coll[8] = {0}, ncoll = 0;
    int cur = -1;
    for (; d < end && d[0]; d += d[0]) {
        if (d[1] == 4) { cur = d[2]; if (cur < 16) if_sub[cur] = d[6]; }   /* an interface: its subclass */
        if (d[1] == 0x24 && d[2] == 1 && cur >= 0 && if_sub[cur] == 1) {   /* the AC header: its collection */
            uint32_t i;
            ncoll = d[7];
            for (i = 0; i < ncoll && i < sizeof coll; i++) coll[i] = d[8 + i];
        }
    }
    check("the configuration descriptor's length is CFG_LEN", (CFG_DESC[2] | CFG_DESC[3] << 8) == (int)sizeof CFG_DESC);
    check("the AC collection: two interfaces", ncoll == 2);
    check("the audio streaming interface first in the collection, MIDI last", ncoll == 2 && if_sub[coll[0]] == 2 && if_sub[coll[1]] == 3);
    check("bcdDevice +0.08: the collection's new order (hosts cache descriptors by version)", (DEV_DESC[12] & 0x0Eu) == 0x08u);
    printf("%s\n", fails ? "USB FAIL" : "USB OK");
    return fails ? 1 : 0;
}
