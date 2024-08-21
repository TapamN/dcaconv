#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dr_wav.h"
#include "dca_conv.h"


dcaError fWavLoad(DcAudioConverter *dcac, const char *fname) {
	dcaError retval = DCAE_OK;
	drwav wav;
	
	if (!drwav_init_file(&wav, fname, NULL)) {
		return DCAE_READ_OPEN_ERROR;
	}
	
	drwav_int16 *interleaved_samples = malloc(wav.totalPCMFrameCount * wav.channels * sizeof(drwav_int16));
	size_t sample_cnt = drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, interleaved_samples);
	
	if (sample_cnt != wav.totalPCMFrameCount) {
		dcaLog(LOG_WARNING, "Short read of %u samples out of %u\n", (unsigned)sample_cnt, (unsigned)wav.totalPCMFrameCount);
		retval = DCAE_READ_ERROR;
		goto cleanup;
	}
	
	dcac->sample_rate_hz = wav.sampleRate;
	dcac->channel_cnt = wav.channels;
	dcac->samples_len = sample_cnt;
	dcaDeinterleaveSamples(dcac, interleaved_samples, sample_cnt, wav.channels);
	
	//~ printf("Wave got %u samples and %u channels\n",(unsigned)sample_cnt, (unsigned)wav.channels);
cleanup:
	drwav_uninit(&wav);
	free(interleaved_samples);
	
	return retval;
}

dcaError fWavWrite(DcAudioConverter *dcac, const char *outfname) {
	drwav wav;
	drwav_data_format format;
	
	format.container = drwav_container_riff;
	format.format = DR_WAVE_FORMAT_PCM;
	format.channels = dcac->channel_cnt;
	format.sampleRate = dcac->sample_rate_hz;
	format.bitsPerSample = 16;
	
	if (drwav_init_file_write_sequential(&wav, outfname, &format, dcac->samples_len, NULL) == 0) {
		return DCAE_WRITE_OPEN_ERROR;
	}
	
	//Interleave samples
	int16_t *interleaved = malloc(dcac->samples_len * dcac->channel_cnt * sizeof(int16_t));
	const unsigned ch_cnt = dcac->channel_cnt;
	for(unsigned i = 0; i < dcac->samples_len; i++) {
		for(unsigned c = 0; c < ch_cnt; c++) {
			interleaved[i*ch_cnt+c] = dcac->samples[c][i];
		}
	}
	
	//Write
	drwav_uint64 samples_written = drwav_write_pcm_frames(&wav, dcac->samples_len, interleaved);
	
	//Clean up
	drwav_uninit(&wav);
	free(interleaved);
	
	return samples_written == dcac->samples_len ? DCAE_OK : DCAE_WRITE_ERROR;
}
