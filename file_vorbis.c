#include <string.h>

#include "dca_conv.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"


dcaError fVorbisLoad(DcAudioConverter *dcac, const char *fname) {
	int channel_cnt = 0;
	int sample_rate_hz = 0;
	short *output = NULL;
	
	int vorblen = stb_vorbis_decode_filename(fname, &channel_cnt, &sample_rate_hz, &output);
	if (vorblen <= 0) {
		return DCAE_READ_OPEN_ERROR;
	}
	
	dcac->samples_len = vorblen;
	dcac->sample_rate_hz = sample_rate_hz;
	dcac->channel_cnt = channel_cnt;
	dcaDeinterleaveSamples(dcac, output, vorblen, channel_cnt);
	
	free(output);
	
	return DCAE_OK;
}
