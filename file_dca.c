#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "file_dca.h"
#include "dca_conv.h"

unsigned fDcaConvertFrequency(unsigned int freq_hz) {
	uint32_t freq_lo, freq_base = 5644800;
	int freq_hi = 7;

	while(freq_hz < freq_base && freq_hi > -8) {
		freq_base >>= 1;
		freq_hi--;
	}

	//~ freq_lo = (freq_hz << 10) / freq_base;
	freq_lo = ((freq_hz << 10) + freq_base/2) / freq_base;
	return (freq_hi << 11) | (freq_lo & 1023);
}
float fDcaUnconvertFrequency(unsigned int freq) {
	freq &= 0x7fff;
	
	int freq_hi = (freq >> 11) & 0xf;
	unsigned int freq_lo = freq & 0x3ff;
	if (freq_hi & 0x8)
		freq_hi |= 0xfffffff0;
	
	float newfreq = 44100 * pow(2, freq_hi) * (1+(float)freq_lo/1024);
	
	return newfreq;
}
unsigned fDcaNearestAICAFrequency(unsigned int freq_hz) {
	return fDcaUnconvertFrequency(fDcaConvertFrequency(freq_hz)) + 0.5;
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
	DcAudioHeader *data = malloc(filesize);
	size_t amountread = fread(data, 1, filesize, f);
	fclose(f);
	
	if (amountread != filesize)
		goto readerror;
	if (data->chunk_size > filesize)
		goto readerror;
	if (memcmp(data->fourcc, DCA_FOURCC, sizeof(data->fourcc)) != 0)
		goto readerror;
	
	unsigned channels = data->flags & DCA_FLAG_CHANNEL_COUNT_MASK;
	unsigned sample_cnt = data->length;
	void *filesamples = (char*)data + fDaGetHeaderSize(data);
	size_t channel_size = (dcac->in.size_samples + 31) & ~0x1f;
	
	dcac->in.sample_rate_hz = fDcaUnconvertFrequency(data->sample_rate_aica);
	dcac->in.channels = channels;
	dcac->in.size_samples = sample_cnt;
	
	printf("DCA file loaded has %u channel%s with %u samples at %u hz\n", channels, channels>1?"s":"", sample_cnt, data->sample_rate_hz);
	
	//Convert to 16-bit PCM
	unsigned format = (data->flags >> DCA_FLAG_FORMAT_SHIFT) & DCA_FLAG_FORMAT_MASK;
	if (format == DCAF_PCM16) {
		channel_size *= sizeof(int16_t);
		for(unsigned c = 0; c < channels; c++) {
			dcac->in.samples[c] = calloc(sample_cnt, sizeof(int16_t));
			void *channel_ptr = (char*)filesamples + channel_size * c;
			memcpy(dcac->in.samples[c], channel_ptr, sample_cnt * sizeof(int16_t));
		}
	} else if (format == DCAF_PCM8) {
		channel_size *= sizeof(int8_t);
		for(unsigned c = 0; c < channels; c++) {
			dcac->in.samples[c] = calloc(sample_cnt, sizeof(int16_t));
			int8_t *channel_ptr = (int8_t*)((char*)filesamples + channel_size * c);
			for(unsigned i = 0; i < sample_cnt; i++) {
				dcac->in.samples[c][i] = channel_ptr[i] * 256;
			}
		}
	} else if (format == DCAF_ADPCM) {
		channel_size = (dcac->in.size_samples/2 + 31) & ~0x1f;
		printf("ch size: %u\n", (unsigned)channel_size);
		for(unsigned c = 0; c < channels; c++) {
			dcac->in.samples[c] = calloc(sample_cnt, sizeof(int16_t));
			uint8_t *channel_ptr = (int8_t*)filesamples + channel_size * c;
			adpcm2pcm(dcac->in.samples[c], channel_ptr, sample_cnt);
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

dcaError fDcaWrite(dcaConvSound *cs, const char *outfname) {
	assert(cs);
	assert(outfname);
	assert(cs->channels > 0);
	for(unsigned i = 0; i < cs->channels; i++)
		assert(cs->samples[i] != NULL);
	
	if (!cs->long_sound && cs->size_samples > DCAC_MAX_SAMPLES)
		return DCAE_TOO_LONG;
	
	if (cs->channels > DCA_FILE_MAX_CHANNELS)
		return DCAE_TOO_MANY_CHANNELS;
	
	//Calculate size of a channel in bytes
	unsigned channelsize = cs->size_samples;
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
	channelsize = (channelsize+31) & ~0x1f;
	
	//Convert to target format
	void *samples[DCA_FILE_MAX_CHANNELS];
	if (cs->format == DCAF_PCM16) {
		//Already in target format, just copy them
		for(unsigned i = 0; i < cs->channels; i++) {
			samples[i] = calloc(1, channelsize);
			memcpy(samples[i], cs->samples[i], cs->size_samples * 2);
		}
	} else if (cs->format == DCAF_PCM8) {
		//TODO add dithering?
		for(unsigned i = 0; i < cs->channels; i++) {
			samples[i] = calloc(1, channelsize);
			ConvertTo8bit(cs->samples[i], samples[i], cs->size_samples);
		}
	} else if (cs->format == DCAF_ADPCM) {
		for(unsigned i = 0; i < cs->channels; i++) {
			samples[i] = calloc(1, channelsize);
			pcm2adpcm(samples[i], cs->samples[i], cs->size_samples);
		}
	}
	
	//Initialize header
	DcAudioHeader head;
	memset(&head, 0, sizeof(head));
	memcpy(head.fourcc, DCA_FOURCC, sizeof(head.fourcc));
	head.chunk_size = sizeof(head) + channelsize * cs->channels;
	head.version = 0;
	head.header_size = 0;
	head.flags = 
		((cs->format & DCA_FLAG_FORMAT_MASK) << DCA_FLAG_FORMAT_SHIFT) |
		(cs->channels & DCA_FLAG_CHANNEL_COUNT_MASK);
	if (cs->long_sound)
		head.flags |= DCA_FLAG_LONG;
	head.sample_rate_aica = fDcaConvertFrequency(cs->sample_rate_hz);
	unsigned converted_sample_rate = fDcaNearestAICAFrequency(cs->sample_rate_hz);
	head.sample_rate_hz = converted_sample_rate <= DCA_MAX_STORED_SAMPLE_RATE_HZ ? converted_sample_rate : DCA_MAX_STORED_SAMPLE_RATE_HZ;
	head.length = cs->size_samples;
	
	if (cs->loop_end > cs->loop_start) {
		head.flags |= DCA_FLAG_LOOPING;
		head.loop_start = cs->loop_start;
		head.loop_end = cs->loop_end-1;
	}
	
	//Write to disk
	unsigned written = 0;
	FILE *f = fopen(outfname, "w");
	if (f == NULL)
		return DCAE_WRITE_OPEN_ERROR;
	
	written += fwrite(&head, 1, sizeof(head), f);
	for(unsigned i = 0; i < cs->channels; i++)
		written += fwrite(samples[i], 1, channelsize, f);
	fclose(f);
	
	//~ printf("Exoected size: %u\n", (unsigned)head.chunk_size);
	//~ printf("Written: %u\n", written);
	(void)written;
	
	for(unsigned i = 0; i < cs->channels; i++)
		free(samples[i]);
	
	
	return written == head.chunk_size ? DCAE_OK : DCAE_WRITE_ERROR;
}
 