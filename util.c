#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dca_conv.h"

#define SAFE_FREE(ptr) \
	if (*(ptr) != NULL) { free(*(ptr)); *(ptr) = NULL; }

void dcaDeinterleaveSamples(DcAudioConverter *dcac, int16_t *samples, unsigned sample_cnt, unsigned channels) {
	for(unsigned i = 0; i < channels; i++) {
		int16_t *dst = malloc(sample_cnt * sizeof(int16_t));
		int16_t *src = samples + i;
		dcac->samples[i] = dst;
		
		for(unsigned j = 0; j < sample_cnt; j++) {
			dst[j] = src[j*channels];
		}
	}
}

void dcaDownmixMono(DcAudioConverter *dcac) {
	assert(dcac);
	assert(dcac->channel_cnt > 0);
	for(unsigned c = 0; c < dcac->channel_cnt; c++)
		assert(dcac->samples[c]);
	
	//If already mono, return
	if (dcac->channel_cnt == 1)
		return;
	
	dcaLog(LOG_PROGRESS, "\nDownmixing to mono\n");
	
	//Average channels together
	int16_t *newsamples = malloc(dcaSizeSamplesBytes(dcac));
	for(unsigned i = 0; i < dcac->samples_len; i++) {
		int val = 0;
		for(unsigned c = 0; c < dcac->channel_cnt; c++) {
			val += dcac->samples[c][i];
		}
		newsamples[i] = val / dcac->channel_cnt;
	}
	
	//Free old samples
	for(unsigned c = 0; c < dcac->channel_cnt; c++) {
		SAFE_FREE(dcac->samples + c);
	}
	
	//Add new samples to sound
	dcac->channel_cnt = 1;
	dcac->samples[0] = newsamples;
}

int dcaCurrentLogLevel = LOG_COMPLETION;
void dcaLogLocV(unsigned level, const char *file, unsigned line, const char *fmt, va_list args) {
	static const char * logtypes[] = {
		[LOG_ALL] = "ALL",
		[LOG_DEBUG] = "DEBUG",
		[LOG_INFO] = "INFO",
		[LOG_PROGRESS] = "PROGRESS",
		[LOG_WARNING] = "WARNING",
		[LOG_COMPLETION] = "COMPLETION",
		[LOG_NONE] = "NONE"
	};
	
	if (level > dcaCurrentLogLevel)
		return;
	
	if (dcaCurrentLogLevel == LOG_DEBUG) {
		if (level >= LOG_DEBUG)
			level = LOG_DEBUG;
		if (file == NULL)
			file = "unk";
		fprintf(stderr, "[%20s, ln %4i] %10s: ", file, line, logtypes[level]);
	}
	vfprintf(stderr, fmt, args);
}

void dcaLogLoc(unsigned level, const char *file, unsigned line, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	dcaLogLocV(level, file, line, fmt, args);
	va_end(args);
}

const char * dcaErrorString(dcaError error) {
	static const char * errstrings[] = {
		[DCAE_OK] = "Ok",
		[DCAE_READ_OPEN_ERROR] = "Could not open file for reading",
		[DCAE_WRITE_OPEN_ERROR] = "Could not open file for reading",
		[DCAE_UNSUPPORTED_FILE_TYPE] = "Unsupported file format",
		[DCAE_NO_FILE_NAME] = "File name not specified",
		[DCAE_TOO_MANY_CHANNELS] = "Too many channels",
		[DCAE_TOO_LONG] = "Sound is too long",
		[DCAE_READ_ERROR] = "Error while reading file",
		[DCAE_WRITE_ERROR] = "Error while writing file",
		[DCAE_UNKNOWN] = "Unknown error",
	};
	
	unsigned e = error;
	if (e > DCAE_UNKNOWN)
		e = DCAE_UNKNOWN;
	
	return errstrings[e];
}
