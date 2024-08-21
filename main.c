#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "dca_conv.h"
#include "dr_wav.h"
#include "optparse.h"
#include "samplerate.h"

/*
	Planned switches:
	
	-i [filename] input
	-o [filename] output
	-f [format] output format (adpcm (default), pcm16, pcm8)
	-r [number] Output sample rate (default is same as original)
	-c [number] channels output (default 1)
	-S same as -c 2
	-s [sample_num] loop start sample
	-e [sample_num]  loop end sample
	-E disable trim loop end
	-t [level] trim trailing silence
	-l generate long samples
	-d downsample if sample too long, rather than error
*/

#define ARR_SIZE(array)	(sizeof(array) / sizeof(array[0]))
#define SAFE_FREE(ptr) \
	if (*(ptr) != NULL) { free(*(ptr)); *(ptr) = NULL; }
#define SMART_ALLOC(ptr, size) \
	do { SAFE_FREE(ptr); *ptr = calloc(size, 1); } while(0)

void ErrorExitV(const char *fmt, va_list args) {
	fprintf(stderr, "Error: ");
	vfprintf(stderr, fmt, args);
	exit(1);
}
	
void ErrorExit(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	ErrorExitV(fmt, args);
	va_end(args);
	
	exit(1);
}
void ErrorExitOn(int cond, const char *fmt, ...) {
	if (!cond)
		return;
	
	va_list args;
	va_start(args, fmt);
	ErrorExitV(fmt, args);
	va_end(args);
}

//Returns a pointer to the start of the extension for a filename. If the file does not have an extension,
//returns a pointer to a zero length string.
const char * GetExtension(const char *name) {
	const char *extension = "";
	if (name) {
		extension = strrchr(name, '.');
		if (extension == NULL)
			extension = "";
	}
	return extension;
}

void DeinterleaveSamples(DcAudioConverter *dcac, int16_t *samples, unsigned sample_cnt, unsigned channels) {
	for(unsigned i = 0; i < channels; i++) {
		int16_t *dst = malloc(sample_cnt * sizeof(int16_t));
		int16_t *src = samples + i;
		dcac->samples[i] = dst;
		
		for(unsigned j = 0; j < sample_cnt; j++) {
			dst[j] = src[j*channels];
		}
	}
}

void dcaInit(DcAudioConverter *dcac) {
	assert(dcac);
	
	memset(dcac, 0, sizeof(*dcac));
	
	dcac->format = DCAF_AUTO;
}

void dcaFree(DcAudioConverter *dcac) {
	assert(dcac);
	for(unsigned i = 0; i < DCAC_MAX_CHANNELS; i++) {
		SAFE_FREE(&dcac->samples[i]);
	}
}

dcaError GeneratePreview(const char *src_fname, const char *preview_fname) {
	if (strcasecmp(GetExtension(src_fname), ".dca") != 0) {
		printf("Can only generate previews for .DCA format output files\n");
		return DCAE_UNSUPPORTED_FILE_TYPE;
	}
	if (strcasecmp(GetExtension(preview_fname), ".wav") != 0) {
		printf("Can only generate .WAV preview files\n");
		return DCAE_UNSUPPORTED_FILE_TYPE;
	}
	
	DcAudioConverter dcac, *dcacp = &dcac;
	dcaInit(dcacp);
	dcaError retval = fDcaLoad(dcacp, src_fname);
	
	if (retval) {
		printf("Error retrieving output file for preview\n");
	} else {
		retval = fWavWrite(&dcac, preview_fname);
		printf("Wrote preview to '%s' (errval %u)\n", preview_fname, retval);
	}
	dcaFree(dcacp);
	
	return retval;
}

void dcaDownmixMono(DcAudioConverter *dcac) {
	assert(dcac);
	assert(dcac->channel_cnt > 0);
	for(unsigned c = 0; c < dcac->channel_cnt; c++)
		assert(dcac->samples[c]);
	
	//If already mono, return
	if (dcac->channel_cnt == 1)
		return;
	
	printf("Downmixing to mono\n");
	
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

//https://cfengine.com/blog/2021/optional-arguments-with-getopt-long/
#define OPTARG_FIX_UP do { \
	if (options.optarg == NULL && options.optind < argc && options.argv[options.optind][0] != '-') \
		options.optarg = options.argv[options.optind++]; \
	} while(0)

typedef struct {
	const char *name;
	int value;
} OptionMap;

static const OptionMap out_sound_format[] = {
	{"auto", DCAF_AUTO},
	{"pcm16", DCAF_PCM16},
	{"pcm8", DCAF_PCM8},
	{"adpcm", DCAF_ADPCM},
};

//Search through OptionMap for match and return it's value.
//If name is not found and invalid_msg is NULL, it returns default_value 
//If name is not found and invalid_msg is not NULL, it prints invalid_msg and exits
int GetOptMap(const OptionMap *map, size_t mapsize, const char *name, int default_value, const char *invalid_msg) {
	if (name == NULL) {
		if (invalid_msg)
			ErrorExit("%s", invalid_msg);
		return default_value;
	}
	
	for(size_t i = 0; i < mapsize; i++) {
		if (!strcasecmp(map[i].name, name)) {
			return map[i].value;
		}
	}
	if (invalid_msg)
		ErrorExit("%s", invalid_msg);
	return default_value;
}

int main(int argc, char **argv) {
	DcAudioConverter dcac, *dcacp = &dcac;
	dcaInit(dcacp);
	
	const char *in_fname = NULL;
	const char *out_fname = NULL;
	const char *preview = NULL;
	
	//Parse command line parameters
	struct optparse options;
	int option;
	optparse_init(&options, argv);
	struct optparse_long longopts[] = {
		{"help", 'h', OPTPARSE_NONE},
		{"out", 'o', OPTPARSE_REQUIRED},
		{"in", 'i', OPTPARSE_REQUIRED},
		{"preview", 'p', OPTPARSE_REQUIRED},
		
		{"format", 'f', OPTPARSE_REQUIRED},
		{"rate", 'r', OPTPARSE_REQUIRED},
		{"channels", 'c', OPTPARSE_REQUIRED},
		{"stereo", 'S', OPTPARSE_NONE},
		
		{"loop-start", 's', OPTPARSE_REQUIRED},
		{"loop-end", 'e', OPTPARSE_REQUIRED},
		
		{"trim", 't', OPTPARSE_OPTIONAL},
		{"long", 'l', OPTPARSE_NONE},
		{"down-sample", 'd', OPTPARSE_NONE},
		{"no-loop-end-trim", 'E', OPTPARSE_NONE},
		
		{"verbose", 'v', OPTPARSE_NONE},
		{"version", 'V', OPTPARSE_NONE},
		{0}
	};
	
	while ((option = optparse_long(&options, longopts, NULL)) != -1) {
		switch(option) {
		case 'h':
			printf("No help yet\n");
			return 0;
			break;
		case 'i':
			in_fname = options.optarg;
			break;
		case 'o':
			out_fname = options.optarg;
			break;
		case 'p':
			preview = options.optarg;
			break;
		case 'f':
			dcac.format = GetOptMap(out_sound_format, ARR_SIZE(out_sound_format), options.optarg, -1, "invalid format\n");
			break;
		case 'S':
			dcac.desired_channels = 2;
			break;
		case 'l':
			dcac.long_sound = true;
			break;
		case 's':
			if (sscanf(options.optarg, "%u", &dcac.loop_start) != 1)  {
				ErrorExit("Invalid loop start. Must be in sample position. The first sample is 0.\n");
			}
			break;
		case 'e':
			if (sscanf(options.optarg, "%u", &dcac.loop_end) != 1)  {
				ErrorExit("Invalid loop end. Must be in sample position. The first sample is 0.\n");
			}
			break;
		case 'c':
			if ((sscanf(options.optarg, "%u", &dcac.desired_channels) != 1) 
					|| (dcac.desired_channels == 0) || (dcac.desired_channels > DCAC_MAX_CHANNELS))  {
				ErrorExit("invalid number of channels, should be in the range [0, %u]\n", DCAC_MAX_CHANNELS);
			}
			break;
		case 'r':
			if ((sscanf(options.optarg, "%u", &dcac.desired_sample_rate_hz) != 1) 
					|| (dcac.desired_sample_rate_hz == 0) || (dcac.desired_sample_rate_hz > 44100))  {
				ErrorExit("invalid sample rate, should be in the range [0, 44100]\n");
			}
			break;
		default:
			printf("%s\n", options.errmsg);
			return 1;
		}
	}
	
	//Load input file
	const char *inext = GetExtension(in_fname);
	ErrorExitOn(inext[0] == 0, "Unknown file type (no extension)\n");
	dcaError loadresult = DCAE_OK;
	
	if (strcasecmp(inext, ".wav") == 0) {
		loadresult = fWavLoad(dcacp, in_fname);
	} else if (strcasecmp(inext, ".dca") == 0) {
		loadresult = fDcaLoad(dcacp, in_fname);
	} else {
		ErrorExit("Unknown file type\n");
	}
	ErrorExitOn(loadresult, "error loading file %i\n", loadresult);
	
	assert(dcac.channel_cnt > 0);
	assert(dcac.sample_rate_hz > 0);
	assert(dcac.samples[0] != NULL);
	//TODO create silence instead of error
	ErrorExitOn(dcac.samples_len == 0, "zero length sound probably doesn't work well on AICA\n");
	
	//If no sample rate is specified, default to source file rate
	if (dcac.desired_sample_rate_hz == 0)
		dcac.desired_sample_rate_hz = dcac.sample_rate_hz;
	
	//Get output extension
	const char *outext = GetExtension(out_fname);
	
	if (strcasecmp(outext, ".dca") == 0) {
		//If user hasn't specified the number of channels, default to one
		if (dcac.desired_channels == 0)
			dcac.desired_channels = 1;
		
		//Default to ADPCM
		if (dcac.format == DCAF_AUTO)
			dcac.format = DCAF_ADPCM;
		
		unsigned expected_size = (float)dcac.samples_len * dcac.desired_sample_rate_hz / dcac.sample_rate_hz;
		if (!dcac.long_sound && expected_size > DCAC_MAX_SAMPLES) {
			float ratio = (float)DCAC_MAX_SAMPLES / dcac.samples_len;
			unsigned new_rate = fDcaNearestAICAFrequency(dcac.sample_rate_hz * ratio);
			unsigned desired_size_samples = (float)dcac.samples_len * new_rate / dcac.sample_rate_hz;
			
			ErrorExitOn(new_rate < DCA_MINIMUM_SAMPLE_RATE_HZ, "This sound is too long for the AICA to handle directly.\n"
				"To allow long sounds, use the --long option\n");
			
			printf("Input file is long (%u samples). AICA only directly supports sounds shorter than %u samples.\n"
				"Reducing frequency from %u hz to %u hz to fit within AICA limits. Resulting file will be %u samples long\n"
				"To allow long sounds, use the --long option\n",
				(unsigned)dcac.samples_len,
				(1<<16)-1,
				dcac.desired_sample_rate_hz,
				new_rate,
				desired_size_samples);
			dcac.desired_sample_rate_hz = new_rate;
		}
		
		//The floating point format of the AICA's frequency rate register results in some values getting rounded.
		//Do the rounding here to so resampling calculations will better match actual output
		dcac.desired_sample_rate_hz = fDcaNearestAICAFrequency(dcac.desired_sample_rate_hz);
		
		
		if (dcac.desired_sample_rate_hz < 172) {
			printf("Sample rate of %u is too low. AICA does not support sample rates less than 172 hz. Using 172 hz sample rate\n", dcac.desired_sample_rate_hz);
			dcac.desired_sample_rate_hz = 172;
		}
		
		if (dcac.format == DCAF_ADPCM && dcac.desired_sample_rate_hz > DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ) {
			printf("Sample rate of %u is too high for ADPCM. AICA ADPCM does not support sample rates over %u hz, reducing sample rate to %u",
				dcac.desired_sample_rate_hz,
				DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ,
				DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ);
			dcac.desired_sample_rate_hz = DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ;
		}
	} else if (strcasecmp(outext, ".wav") == 0) {
		if (dcac.desired_channels == 0)
			dcac.desired_channels = dcac.channel_cnt;
		
		if (dcac.format == DCAF_AUTO)
			dcac.format = DCAF_PCM16;
	} else {
		ErrorExit("Unknown output file type\n");
	}
	
	if (dcac.desired_channels > dcac.channel_cnt) {
		printf("Warning: Specifed number of output channels of %u is greater than number "
			"of source file channels of %u. Output will have %u channels.\n",
			dcac.channel_cnt,
			dcac.channel_cnt,
			dcac.channel_cnt);
		dcac.desired_channels = dcac.channel_cnt;
	}
	
	//Handle channel conversion
	if (dcac.desired_channels == dcac.channel_cnt) {
		//Nothing to do in this case
	} else if (dcac.desired_channels == 1) {
		dcaDownmixMono(&dcac);
	} else if (dcac.desired_channels == 2 && dcac.channel_cnt == 1) {
		//pad out to stereo. why is user doing this?
		assert(0 && "todo no mono to stereo yet");
	} else {
		ErrorExit("Cannot convert from %u channels to %u channels\n",
			dcac.channel_cnt,
			dcac.channel_cnt);
	}
	
	//Adjust sample rate
	if (dcac.desired_sample_rate_hz != dcac.sample_rate_hz) {
		printf("Converting input sample rate from %u hz to %u hz\n", dcac.sample_rate_hz, dcac.desired_sample_rate_hz);
		int src_err = 0;
		SRC_STATE *src = src_new(SRC_SINC_BEST_QUALITY, 1, &src_err);
		assert(src);
		if (src_err) {
			printf("SRC error: %s\n", src_strerror(src_err));
			return 1;
		}
		
		//Allocate space to convert to float and store results
		unsigned new_size = (float)dcac.samples_len * dcac.desired_sample_rate_hz / dcac.sample_rate_hz;
		float *in_float = calloc(1, dcac.samples_len * sizeof(float));
		float *out_float = calloc(1, new_size * sizeof(float));
		
		//Resample every channel
		for(unsigned i = 0; i < dcac.channel_cnt; i++) {
			//Convert to float
			src_short_to_float_array(dcac.samples[i], in_float, dcac.samples_len);
			
			//Preform resampling
			SRC_DATA srcd;
			srcd.data_in = in_float;
			srcd.data_out = out_float;
			srcd.input_frames = dcac.samples_len;
			srcd.output_frames = new_size;
			srcd.src_ratio = (float)dcac.desired_sample_rate_hz / dcac.sample_rate_hz;
			srcd.end_of_input = 1;
			src_process(src, &srcd);
			
			//Convert to 16-bit
			SMART_ALLOC(&dcac.samples[i], new_size * sizeof(int16_t));
			src_float_to_short_array(out_float, dcac.samples[i], new_size);
			
			src_reset(src);
		}
		
		free(in_float);
		free(out_float);
		
		if (src_err) {
			printf("SRC error: %s\n", src_strerror(src_err));
		}
		
		src_delete(src);
		
		dcac.sample_rate_hz = dcac.desired_sample_rate_hz;
		dcac.samples_len = new_size;
	}
	
	//Write output file
	dcaError write_error = DCAE_UNKNOWN;
	if (strcasecmp(outext, ".dca") == 0) {
		write_error = fDcaWrite(&dcac, out_fname);
	} else if (strcasecmp(outext, ".wav") == 0) {
		write_error = fWavWrite(&dcac, out_fname);
	} else {
		ErrorExit("Unsupported output file type '%s'\n", outext);
	}
	if (write_error)
		printf("Write error #%i\n", write_error);
	else
		printf("Success\n");
	
	//Write preview
	if (preview && !write_error) {
		GeneratePreview(out_fname, preview);
	}
	
	dcaFree(dcacp);
	
	return 0;
}
