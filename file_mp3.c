#include "dca_conv.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

dcaError fMp3Load(DcAudioConverter *dcac, const char *fname) {
	drmp3_config mp3info;
	drmp3_uint64 samples_len = 0;
	
	short *output = drmp3_open_file_and_read_pcm_frames_s16(fname, &mp3info, &samples_len, NULL);
	if (output == NULL) {
		return DCAE_READ_OPEN_ERROR;
	}
	
	dcac->samples_len = samples_len;
	dcac->sample_rate_hz = mp3info.sampleRate;
	dcac->channel_cnt = mp3info.channels;
	dcaDeinterleaveSamples(dcac, output, samples_len, mp3info.channels);
	
	dcaLog(LOG_WARNING, "%u, %u\n",  dcac->sample_rate_hz, dcac->channel_cnt);
	
	drmp3_free(output, NULL);
	
	return DCAE_OK;
}
