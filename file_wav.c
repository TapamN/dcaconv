#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dr_wav.h"
#include "dca_conv.h"

dcaError fWavWrite(dcaConvSound *cs, const char *outfname) {
	drwav wav;
	drwav_data_format format;
	
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = cs->channels;
	format.sampleRate = cs->sample_rate_hz;
	format.bitsPerSample = 16;
	
	if (drwav_init_file_write_sequential(&wav, outfname, &format, cs->size_samples, NULL) == 0) {
		return DCAE_WRITE_OPEN_ERROR;
	}
	
	//Interleave samples
	int16_t *interleaved = malloc(cs->size_samples * cs->channels * sizeof(int16_t));
	const unsigned ch_cnt = cs->channels;
	for(unsigned i = 0; i < cs->size_samples; i++) {
		for(unsigned c = 0; c < ch_cnt; c++) {
			interleaved[i*ch_cnt+c] = cs->samples[c][i];
		}
	}
	
	//Write
	drwav_uint64 samples_written = drwav_write_pcm_frames(&wav, cs->size_samples, interleaved);
	
	//Clean up
	drwav_uninit(&wav);
	free(interleaved);
	
	return samples_written == cs->size_samples ? DCAE_OK : DCAE_WRITE_ERROR;
}