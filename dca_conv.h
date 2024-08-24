#ifndef DCA_CONV_H
#define DCA_CONV_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "file_dca.h"
#include "util.h"

//Max channels supported by converter
#define DCAC_MAX_CHANNELS	8

/*
	Max number of samples allowed in a sound by the AICA. DCA files 
	longer than this will be downsampled to fit unless long_sound is set.
	
	The AICA has a maximum sample length of 2^16, but supposedly has 
	trouble with looping if at the maximum length, so the max here is a 
	bit less.
*/
#define DCAC_MAX_SAMPLES	(64*1024-256)

typedef enum {
	//The first three (PCM16, PCM8, and ADPCM) match up to the AICA's formats. Do not change this.
	
	DCAF_PCM16,
	DCAF_PCM8,
	DCAF_ADPCM,
	
	//Guess format based on output file extension
	DCAF_AUTO,
} dcaFormat;

typedef struct {
	//sample rate of samples
	unsigned sample_rate_hz;
	
	unsigned channel_cnt;
	//Samples are always stored here as 16-bit, and converted to final format when writing
	int16_t *samples[DCAC_MAX_CHANNELS];
	//Size is in samples, not bytes. Size in bytes will always be this times sizeof(*samples[0])
	size_t samples_len;
	
	
	//The following are used for output:
	
	//Format to encode to (not the format .samples[] is in, which is always 16-bit PCM)
	dcaFormat format;
	//Channels to convert to (if 0, desired channels depends on format)
	unsigned desired_channels;
	//sample rate to convert to
	unsigned desired_sample_rate_hz;
	//Generate DCA file longer than DCAC_MAX_SAMPLES without downsampling
	bool long_sound;
	
	bool looping;
	unsigned loop_start, loop_end;
} DcAudioConverter;

static inline size_t dcaSizeSamplesBytes(const DcAudioConverter *cs) {
	return cs->samples_len * sizeof(int16_t);
}

typedef enum {
	//No error
	DCAE_OK,
	
	//Error opening file for reading
	DCAE_READ_OPEN_ERROR,
	
	//Error opening file for writing
	DCAE_WRITE_OPEN_ERROR,
	
	//Unable to determine file type
	DCAE_UNSUPPORTED_FILE_TYPE,
	
	DCAE_NO_FILE_NAME,
	
	DCAE_TOO_MANY_CHANNELS,
	
	DCAE_TOO_LONG,
	
	DCAE_READ_ERROR,
	
	DCAE_WRITE_ERROR,
	
	DCAE_UNKNOWN,
} dcaError;

dcaError fDcaLoad(DcAudioConverter *dcac, const char *fname);
dcaError fDcaWrite(DcAudioConverter *cs, const char *outfname);
unsigned fDcaConvertFrequency(unsigned int freq_hz);
float fDcaUnconvertFrequency(unsigned int freq);
//Converts a given freqency to AICA closest match
unsigned fDcaToAICAFrequency(unsigned int freq_hz);

dcaError fWavLoad(DcAudioConverter *dcac, const char *fname);
dcaError fWavWrite(DcAudioConverter *dcac, const char *outfname);

dcaError fVorbisLoad(DcAudioConverter *dcac, const char *fname);

dcaError fFlacLoad(DcAudioConverter *dcac, const char *fname);

dcaError fMp3Load(DcAudioConverter *dcac, const char *fname);

void dcaDeinterleaveSamples(DcAudioConverter *dcac, int16_t *samples, unsigned sample_cnt, unsigned channels);
void dcaDownmixMono(DcAudioConverter *dcac);

const char * dcaErrorString(dcaError error);

#endif