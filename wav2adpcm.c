/*
    aica adpcm <-> wave converter;

    (c) 2002 BERO <bero@geocities.co.jp>
    under GPL or notify me

    aica adpcm seems same as YMZ280B adpcm
    adpcm->pcm algorithm can found MAME/src/sound/ymz280b.c by Aaron Giles

    this code is for little endian machine

    Modified by Megan Potter to read/write ADPCM WAV files, and to
    handle stereo (though the stereo is very likely KOS specific
    since we make no effort to interleave it). Please see README.GPL
    in the KOS docs dir for more info on the GPL license.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static int diff_lookup[16] = {
    1, 3, 5, 7, 9, 11, 13, 15,
    -1, -3, -5, -7, -9, -11, -13, -15,
};

static int index_scale[16] = {
    0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266,
    0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 /* same value for speedup */
};

static inline int limit(int val, int min, int max) {
    if(val < min) return min;
    else if(val > max) return max;
    else return val;
}

/*
    In the original wav2adpcm, length was the length of src in bytes. Now it is the number of samples to convert.
*/
void pcm2adpcm(unsigned char *dst, const short *src, size_t length) {
    int signal, step;
    signal = 0;
    step = 0x7f;

    do {
        int data, val, diff;

        /* hign nibble */
        //masking is from https://github.com/superctr/adpcm/blob/master/ymz_codec.h to improve quality
        diff = (*src++ & ~7) - signal;
        diff = (diff * 8) / step;

        val = abs(diff) / 2;

        if(val > 7) val = 7;

        if(diff < 0) val += 8;

        signal += (step * diff_lookup[val]) / 8;
        signal = limit(signal, -32768, 32767);

        step = (step * index_scale[val]) >> 8;
        step = limit(step, 0x7f, 0x6000);

        data = val;
        
        if (--length == 0) {
            *dst++ = data;
            break;
        }

        /* low nibble */
        //masking is from https://github.com/superctr/adpcm/blob/master/ymz_codec.h to improve quality
        diff = (*src++ & ~7) - signal;
        diff = (diff * 8) / step;

        val = (abs(diff)) / 2;

        if(val > 7) val = 7;

        if(diff < 0) val += 8;

        signal += (step * diff_lookup[val]) / 8;
        signal = limit(signal, -32768, 32767);

        step = (step * index_scale[val]) >> 8;
        step = limit(step, 0x7f, 0x6000);

        data |= val << 4;

        *dst++ = data;

    }
    while(--length);
}


/*
    In the original wav2adpcm, length was the length of src in bytes. Since 
    there were two samples per byte, it was impossible to write an odd number 
    of samples, potentially overwriting the end of a buffer by one sample. 
    Now it is the number of samples to convert.
*/
void adpcm2pcm(short *dst, const unsigned char *src, size_t length) {
    int signal, step;
    signal = 0;
    step = 0x7f;

    do {
        int data, val;

        data = *src++;

        /* low nibble */
        val = data & 15;

        signal += (step * diff_lookup[val]) / 8;
        signal = limit(signal, -32768, 32767);

        step = (step * index_scale[val & 7]) >> 8;
        step = limit(step, 0x7f, 0x6000);

        *dst++ = signal;
        
        if (--length == 0) {
            break;
        }

        /* high nibble */
        val = (data >> 4) & 15;

        signal += (step * diff_lookup[val]) / 8;
        signal = limit(signal, -32768, 32767);

        step = (step * index_scale[val & 7]) >> 8;
        step = limit(step, 0x7f, 0x6000);

        *dst++ = signal;

    }
    while(--length);
}

void deinterleave(void *buffer, size_t size) {
    short * buf;
    short * buf1, * buf2;
    int i;

    buf = (short *)buffer;
    buf1 = malloc(size / 2);
    buf2 = malloc(size / 2);

    for(i = 0; i < size / 4; i++) {
        buf1[i] = buf[i * 2 + 0];
        buf2[i] = buf[i * 2 + 1];
    }

    memcpy(buf, buf1, size / 2);
    memcpy(buf + size / 4, buf2, size / 2);

    free(buf1);
    free(buf2);
}

void interleave(void *buffer, size_t size) {
    short * buf;
    short * buf1, * buf2;
    int i;

    buf = malloc(size);
    buf1 = (short *)buffer;
    buf2 = buf1 + size / 4;

    for(i = 0; i < size / 4; i++) {
        buf[i * 2 + 0] = buf1[i];
        buf[i * 2 + 1] = buf2[i];
    }

    memcpy(buffer, buf, size);

    free(buf);
}
