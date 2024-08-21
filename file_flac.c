#include "dca_conv.h"

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

dcaError fFlacLoad(DcAudioConverter *dcac, const char *fname) {
	unsigned channel_cnt = 0;
	unsigned sample_rate_hz = 0;
	drflac_uint64 samples_len = 0;
	
	short *output = drflac_open_file_and_read_pcm_frames_s16(fname, &channel_cnt, &sample_rate_hz, &samples_len, NULL);
	if (output == NULL) {
		return DCAE_READ_OPEN_ERROR;
	}
	
	dcac->samples_len = samples_len;
	dcac->sample_rate_hz = sample_rate_hz;
	dcac->channel_cnt = channel_cnt;
	dcaDeinterleaveSamples(dcac, output, samples_len, channel_cnt);
	
	free(output);
	
	return DCAE_OK;
}
