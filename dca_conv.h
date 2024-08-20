#ifndef DCA_CONV_H
#define DCA_CONV_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "file_dca.h"

//Max channels supported by converter
#define DCAC_MAX_CHANNELS	8

/*
	Max number of samples allowed in a sound by the AICA. DCA files 
	longer than this will be downsampled to fit unless long_sound is set.
	
	The AICA has a maximum sample length of 2^16, but supposedly has 
	trouble with looping if at the maximum length, so the max here is a 
	bit less.
*/
#define DCAC_MAX_SAMPLES	(64*1024-64)

typedef enum {
	//The first three (PCM16, PCM8, and ADPCM) match up to the AICA's formats. Do not change this.
	
	DCAF_PCM16,
	DCAF_PCM8,
	DCAF_ADPCM,
	
	//Guess format based on output file extension
	DCAF_AUTO,
} dcaFormat;

typedef struct {
	const char *filename;
	unsigned sample_rate_hz;
	
	unsigned channels;
	//Samples are always stored here as 16-bit, and converted to final format when writing
	int16_t *samples[DCAC_MAX_CHANNELS];
	//Size is in samples, not bytes. Size in bytes will always be this times sizeof(*samples[0])
	size_t size_samples;
	
	
	//The following are used for output:
	
	//Format to encode to, not the format .samples is in.
	dcaFormat format;
	//Channels to convert to (if 0, desired channels depends on format)
	unsigned desired_channels;
	
	unsigned desired_sample_rate_hz;
	//~ float ratio;
	//Generate DCA file longer than DCAC_MAX_SAMPLES without downsampling
	bool long_sound;
	
	//File has loop if loop_end > loop_start
	unsigned loop_start, loop_end;
} dcaConvSound;
static inline size_t dcaCSSizeBytes(const dcaConvSound *cs) {
	return cs->size_samples * sizeof(int16_t);
}

typedef struct {
	//Format must always noninterleaved 16-bit signed PCM
	//If source file is not in that format, it must be converted before storing it here
	dcaConvSound in;
	
	//File resulting from conversion
	dcaConvSound out;
	
} DcAudioConverter;

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
dcaError fWavLoad(DcAudioConverter *dcac, const char *fname);
dcaError fDcaWrite(dcaConvSound *cs, const char *outfname);
dcaError fWavWrite(dcaConvSound *cs, const char *outfname);

void DeinterleaveSamples(dcaConvSound *cs, int16_t *samples, unsigned sample_cnt, unsigned channels);

#endif