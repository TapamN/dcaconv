#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define DCAUDIO_IMPLEMENTATION
#include "file_dca.h"

#include "dca_conv.h"


unsigned fDcaToAICAFrequency(unsigned int freq_hz) {
	return fDaUnconvertFrequency(fDaConvertFrequency(freq_hz));
}

void ConvertTo8bit(const int16_t *src, int8_t *dst, size_t sample_cnt) {
	for(unsigned i = 0; i < sample_cnt; i++) {
		dst[i] = src[i] >> 8;
	}
}

void pcm2adpcm(unsigned char *dst, const short *src, size_t length);
void adpcm2pcm(short *dst, const unsigned char *src, size_t length);

dcaError fDcaLoad(DcAudioConverter *dcac, const char *fname) {
	assert(dcac);
	assert(fname);
	
	//Open
	FILE *f = fopen(fname, "rb");
	if (f == NULL)
		return DCAE_READ_OPEN_ERROR;
	
	//Get size
	fseek(f, 0, SEEK_END);
	size_t filesize = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	//Load
	fDcAudioHeader *data = malloc(filesize);
	size_t amountread = fread(data, 1, filesize, f);
	fclose(f);
	
	if (amountread != filesize)
		goto readerror;
	if (!fDaValidateHeader(data))
		goto readerror;
	
	unsigned channels = fDaGetChannelCount(data);
	unsigned sample_cnt = fDaGetTotalLength(data);
	
	dcac->sample_rate_hz = fDaCalcSampleRateHz(data);
	dcac->channel_cnt = channels;
	dcac->samples_len = sample_cnt;
	
	dcaLog(LOG_INFO, "DCA file loaded has %u channel%s with %u samples at %u hz\n", channels, channels>1?"s":"", sample_cnt, dcac->sample_rate_hz);
	
	//Convert to 16-bit PCM
	unsigned format = fDaGetSampleFormat(data);
	if (format == DCAF_PCM16) {
		for(unsigned c = 0; c < channels; c++) {
			dcac->samples[c] = calloc(sample_cnt, sizeof(int16_t));
			void *channel_ptr = fDaGetChannelSamples(data, c);
			memcpy(dcac->samples[c], channel_ptr, sample_cnt * sizeof(int16_t));
		}
	} else if (format == DCAF_PCM8) {
		for(unsigned c = 0; c < channels; c++) {
			dcac->samples[c] = calloc(sample_cnt, sizeof(int16_t));
			int8_t *channel_ptr = (int8_t*)fDaGetChannelSamples(data, c);;
			for(unsigned i = 0; i < sample_cnt; i++) {
				dcac->samples[c][i] = channel_ptr[i] * 256;
			}
		}
	} else if (format == DCAF_ADPCM) {
		for(unsigned c = 0; c < channels; c++) {
			dcac->samples[c] = calloc(sample_cnt, sizeof(int16_t));
			uint8_t *channel_ptr = (uint8_t*)fDaGetChannelSamples(data, c);;
			adpcm2pcm(dcac->samples[c], channel_ptr, sample_cnt);
		}
	} else {
		goto readerror;
	}
	
	free(data);
	return DCAE_OK;
	
readerror:
	free(data);
	return DCAE_READ_ERROR;
}

dcaError fDcaWrite(DcAudioConverter *cs, const char *outfname) {
	assert(cs);
	assert(outfname);
	assert(cs->channel_cnt > 0);
	for(unsigned i = 0; i < cs->channel_cnt; i++)
		assert(cs->samples[i] != NULL);
	
	//TODO +50 is a hack to give some slack to length check
	if (!cs->long_sound && cs->samples_len > (DCAC_MAX_SAMPLES+50)) {
		dcaLog(LOG_WARNING, "Output is %u samples\n", cs->samples_len);
		return DCAE_TOO_LONG;
	}
	
	if (cs->channel_cnt > DCA_FILE_MAX_CHANNELS)
		return DCAE_TOO_MANY_CHANNELS;
	
	//Calculate size of a channel in bytes
	unsigned channelsize = cs->samples_len;
	if (cs->format == DCAF_PCM16) {
		channelsize *= 2;
	} else if (cs->format == DCAF_PCM8) {
		//Already the right size
	} else if (cs->format == DCAF_ADPCM) {
		//Round up to byte
		channelsize = (channelsize+1) / 2;
		assert(cs->sample_rate_hz <= 88200);
	} else {
		assert(0 && "bad format");
	}
	
	//Round channel size up to multiple of 32
	channelsize = (channelsize+DCA_ALIGNMENT_MASK) & ~DCA_ALIGNMENT_MASK;
	
	//Convert to target format
	void *samples[DCA_FILE_MAX_CHANNELS];
	if (cs->format == DCAF_PCM16) {
		//Already in target format, just copy them
		for(unsigned i = 0; i < cs->channel_cnt; i++) {
			samples[i] = calloc(1, channelsize);
			memcpy(samples[i], cs->samples[i], cs->samples_len * 2);
		}
	} else if (cs->format == DCAF_PCM8) {
		//TODO add dithering?
		for(unsigned i = 0; i < cs->channel_cnt; i++) {
			samples[i] = calloc(1, channelsize);
			ConvertTo8bit(cs->samples[i], samples[i], cs->samples_len);
		}
	} else if (cs->format == DCAF_ADPCM) {
		for(unsigned i = 0; i < cs->channel_cnt; i++) {
			samples[i] = calloc(1, channelsize);
			pcm2adpcm(samples[i], cs->samples[i], cs->samples_len);
		}
	}
	
	//Initialize header
	fDcAudioHeader head;
	memset(&head, 0, sizeof(head));
	memcpy(head.fourcc, DCA_FOURCC_STR, sizeof(head.fourcc));
	head.chunk_size = sizeof(head) + channelsize * cs->channel_cnt;
	head.version = 0;
	head.flags = 
		((cs->format & DCA_FLAG_FORMAT_MASK) << DCA_FLAG_FORMAT_SHIFT) |
		(cs->channel_cnt & DCA_FLAG_CHANNEL_COUNT_MASK);
	head.sample_rate_aica = fDaConvertFrequency(cs->sample_rate_hz);
	unsigned converted_sample_rate = fDcaToAICAFrequency(cs->sample_rate_hz);
	head.total_length = cs->samples_len;
	
	if (cs->looping) {
		head.flags |= DCA_FLAG_LOOPING;
		head.loop_start = cs->loop_start;
		head.loop_end = cs->loop_end;
	} else {
		head.loop_end = cs->samples_len;
	}
	
	assert(fDaValidateHeader(&head));
	
	//Write to disk
	unsigned written = 0;
	FILE *f = fopen(outfname, "w");
	if (f == NULL)
		return DCAE_WRITE_OPEN_ERROR;
	
	written += fwrite(&head, 1, sizeof(head), f);
	for(unsigned i = 0; i < cs->channel_cnt; i++)
		written += fwrite(samples[i], 1, channelsize, f);
	fclose(f);
	
	dcaLog(LOG_PROGRESS, "Wrote %u channel%s of %u samples at %u hz, in %s format\n",
		cs->channel_cnt, cs->channel_cnt>1?"s":"", head.total_length, converted_sample_rate, fDaFormatString(cs->format));
	
	
	for(unsigned i = 0; i < cs->channel_cnt; i++)
		free(samples[i]);
	
	
	return written == head.chunk_size ? DCAE_OK : DCAE_WRITE_ERROR;
}
 