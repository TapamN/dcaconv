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

void DeinterleaveSamples(dcaConvSound *cs, int16_t *samples, unsigned sample_cnt, unsigned channels) {
	for(unsigned i = 0; i < channels; i++) {
		int16_t *dst = malloc(sample_cnt * sizeof(int16_t));
		int16_t *src = samples + i;
		cs->samples[i] = dst;
		
		for(unsigned j = 0; j < sample_cnt; j++) {
			dst[j] = src[j*channels];
		}
	}
}

void dcaInit(DcAudioConverter *dcac) {
	assert(dcac);
	
	memset(dcac, 0, sizeof(*dcac));
	
	dcac->out.format = DCAF_AUTO;
}

void dcaFree(DcAudioConverter *dcac) {
	assert(dcac);
	for(unsigned i = 0; i < DCAC_MAX_CHANNELS; i++) {
		SAFE_FREE(&dcac->in.samples[i]);
		SAFE_FREE(&dcac->out.samples[i]);
	}
}


/*
	Frees any samples stored in dst, then allocates copies of samples in src.
	Also sets channels and size_samples
	Rest of dst is unchanged
*/
void dcaCSCopySamples(const dcaConvSound *src, dcaConvSound *dst) {
	dst->channels = src->channels;
	dst->size_samples = src->size_samples;
	
	size_t size = dcaCSSizeBytes(src);
	
	for(unsigned i = 0; i < DCAC_MAX_CHANNELS; i++) {
		if (i < src->channels) {
			SMART_ALLOC(&dst->samples[i], size);
			memcpy(dst->samples[i], src->samples[i], size);
		} else {
			SAFE_FREE(&dst->samples[i]);
		}
	}
}

void dcaCSDownmixMono(dcaConvSound *cs) {
	assert(cs);
	assert(cs->channels > 0);
	for(unsigned c = 0; c < cs->channels; c++)
		assert(cs->samples[c]);
	
	//If already mono, return
	if (cs->channels == 1)
		return;
	
	printf("Downmixing to mono\n");
	
	//Average channels together
	int16_t *newsamples = malloc(dcaCSSizeBytes(cs));
	for(unsigned i = 0; i < cs->size_samples; i++) {
		int val = 0;
		for(unsigned c = 0; c < cs->channels; c++) {
			val += cs->samples[c][i];
		}
		newsamples[i] = val / cs->channels;
	}
	
	//Free old samples
	for(unsigned c = 0; c < cs->channels; c++) {
		SAFE_FREE(cs->samples + c);
	}
	
	//Add new samples to sound
	cs->channels = 1;
	cs->samples[0] = newsamples;
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
	
	//Parse command line parameters
	struct optparse options;
	int option;
	optparse_init(&options, argv);
	struct optparse_long longopts[] = {
		{"help", 'h', OPTPARSE_NONE},
		{"out", 'o', OPTPARSE_REQUIRED},
		{"in", 'i', OPTPARSE_REQUIRED},
		
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
			dcac.in.filename = options.optarg;
			break;
		case 'o':
			dcac.out.filename = options.optarg;
			break;
		case 'f':
			dcac.out.format = GetOptMap(out_sound_format, ARR_SIZE(out_sound_format), options.optarg, -1, "invalid format\n");
			break;
		case 'S':
			dcac.out.desired_channels = 2;
			break;
		case 'l':
			dcac.out.long_sound = true;
			break;
		case 's':
			if (sscanf(options.optarg, "%u", &dcac.out.loop_start) != 1)  {
				ErrorExit("Invalid loop start. Must be in sample position. The first sample is 0.\n");
			}
			break;
		case 'e':
			if (sscanf(options.optarg, "%u", &dcac.out.loop_end) != 1)  {
				ErrorExit("Invalid loop end. Must be in sample position. The first sample is 0.\n");
			}
			break;
		case 'c':
			if ((sscanf(options.optarg, "%u", &dcac.out.desired_channels) != 1) 
					|| (dcac.out.desired_channels == 0) || (dcac.out.desired_channels > DCAC_MAX_CHANNELS))  {
				ErrorExit("invalid number of channels, should be in the range [0, %u]\n", DCAC_MAX_CHANNELS);
			}
			break;
		case 'r':
			if ((sscanf(options.optarg, "%u", &dcac.out.desired_sample_rate_hz) != 1) 
					|| (dcac.out.desired_sample_rate_hz == 0) || (dcac.out.desired_sample_rate_hz > 44100))  {
				ErrorExit("invalid sample rate, should be in the range [0, 44100]\n");
			}
			break;
		default:
			printf("%s\n", options.errmsg);
			return 1;
		}
	}
	
	//Load input file
	const char *inext = GetExtension(dcac.in.filename);
	ErrorExitOn(inext[0] == 0, "Unknown file type (no extension)\n");
	dcaError loadresult = DCAE_OK;
	
	if (strcasecmp(inext, ".wav") == 0) {
		loadresult = fWavLoad(dcacp, dcac.in.filename);
	} else {
		ErrorExit("Unknwon file type\n");
	}
	ErrorExitOn(loadresult, "error loading file %i\n", loadresult);
	
	assert(dcac.in.channels > 0);
	assert(dcac.in.sample_rate_hz > 0);
	assert(dcac.in.samples[0] != NULL);
	//TODO create silence instead of error
	ErrorExitOn(dcac.in.size_samples == 0, "zero length sound probably doesn't work well on AICA\n");
	
	//Assign input samples to output
	dcaCSCopySamples(&dcac.in, &dcac.out);
	
	//Convert input samples to 16-bit PCM output samples at out rate
	//Actual compression happens later
	dcac.out.sample_rate_hz = dcac.in.sample_rate_hz;
	
	//If no sample rate is specified, default to source file rate
	if (dcac.out.desired_sample_rate_hz == 0)
		dcac.out.desired_sample_rate_hz = dcac.in.sample_rate_hz;
	
	//Get output extension
	const char *outext = GetExtension(dcac.out.filename);
	
	if (strcasecmp(outext, ".dca") == 0) {
		//If user hasn't specified the number of channels, default to one
		if (dcac.out.desired_channels == 0)
			dcac.out.desired_channels = 1;
		
		//Default to ADPCM
		if (dcac.out.format == DCAF_AUTO)
			dcac.out.format = DCAF_ADPCM;
		
		unsigned expected_size = (float)dcac.out.size_samples * dcac.out.desired_sample_rate_hz / dcac.out.sample_rate_hz;
		if (!dcac.out.long_sound && expected_size > DCAC_MAX_SAMPLES) {
			float ratio = (float)DCAC_MAX_SAMPLES / dcac.out.size_samples;
			unsigned new_rate = dcac.out.sample_rate_hz * ratio;
			unsigned desired_size_samples = (float)dcac.out.size_samples * new_rate / dcac.out.sample_rate_hz;
			
			ErrorExitOn(new_rate < 172, "This sound is too long for the AICA to handle directly.\n"
				"To allow long sounds, use the --long option\n");
			
			printf("Input file is long (%u samples). AICA only directly supports sounds shorter than %u samples.\n"
				"Reducing frequency from %u hz to %u hz to fit within AICA limits. Resulting file will be %u samples long\n"
				"To allow long sounds, use the --long option\n",
				(unsigned)dcac.out.size_samples,
				(1<<16)-1,
				dcac.out.desired_sample_rate_hz,
				new_rate,
				desired_size_samples);
			dcac.out.desired_sample_rate_hz = new_rate;
		}
		if (dcac.out.desired_sample_rate_hz < 172) {
			
			printf("Sample rate of %u is too low. AICA does not support sample rates less than 172 hz. Using 172 hz sample rate\n", dcac.out.desired_sample_rate_hz);
			dcac.out.desired_sample_rate_hz = 172;
		}
		if (dcac.out.format == DCAF_ADPCM && dcac.out.desired_sample_rate_hz > 88200) {
			printf("Sample rate of %u is too high for ADPCM. AICA ADPCM does not support sample rates over 88200 hz, reducing sample rate to 88200", dcac.out.desired_sample_rate_hz);
			dcac.out.desired_sample_rate_hz = 88200;
		}
	} else if (strcasecmp(outext, ".wav") == 0) {
		if (dcac.out.desired_channels == 0)
			dcac.out.desired_channels = dcac.in.channels;
		
		if (dcac.out.format == DCAF_AUTO)
			dcac.out.format = DCAF_PCM16;
	} else {
		ErrorExit("Unknown output file type\n");
	}
	
	if (dcac.out.desired_channels > dcac.in.channels) {
		printf("Warning: Specifed number of output channels of %u is greater than number "
			"of source file channels of %u. Output will have %u channels.\n",
			dcac.out.channels,
			dcac.in.channels,
			dcac.out.channels);
		dcac.out.desired_channels = dcac.in.channels;
	}
	
	//Handle channel conversion
	if (dcac.out.desired_channels == dcac.out.channels) {
		//Nothing to do in this case
	} else if (dcac.out.desired_channels == 1) {
		dcaCSDownmixMono(&dcac.out);
	} else if (dcac.out.desired_channels == 2 && dcac.out.channels == 1) {
		//pad out to stereo. why is user doing this?
		assert(0);
	} else {
		ErrorExit("Cannot convert from %u channels to %u channels\n",
			dcac.in.channels,
			dcac.out.channels);
	}
	
	//Adjust sample rate
	if (dcac.out.desired_sample_rate_hz != dcac.out.sample_rate_hz) {
		int src_err = 0;
		SRC_STATE *src = src_new(SRC_SINC_BEST_QUALITY, 1, &src_err);
		assert(src);
		if (src_err) {
			printf("SRC error: %s\n", src_strerror(src_err));
			return 1;
		}
		
		//Allocate space to convert to float and store results
		unsigned new_size = (float)dcac.out.size_samples * dcac.out.desired_sample_rate_hz / dcac.out.sample_rate_hz;
		float *in_float = calloc(1, dcac.out.size_samples * sizeof(float));
		float *out_float = calloc(1, new_size * sizeof(float));
		
		//Resample every channel
		for(unsigned i = 0; i < dcac.out.channels; i++) {
			//Convert to float
			src_short_to_float_array(dcac.out.samples[i], in_float, dcac.out.size_samples);
			
			//Preform resampling
			SRC_DATA srcd;
			srcd.data_in = in_float;
			srcd.data_out = out_float;
			srcd.input_frames = dcac.out.size_samples;
			srcd.output_frames = new_size;
			srcd.src_ratio = (float)dcac.out.desired_sample_rate_hz / dcac.out.sample_rate_hz;
			srcd.end_of_input = 1;
			src_process(src, &srcd);
			
			//Convert to 16-bit
			SMART_ALLOC(&dcac.out.samples[i], new_size * sizeof(int16_t));
			src_float_to_short_array(out_float, dcac.out.samples[i], new_size);
			
			src_reset(src);
		}
		
		free(in_float);
		free(out_float);
		
		if (src_err) {
			printf("SRC error: %s\n", src_strerror(src_err));
		}
		
		src_delete(src);
		
		dcac.out.sample_rate_hz = dcac.out.desired_sample_rate_hz;
		dcac.out.size_samples = new_size;
	}
	
	//Write output file
	dcaError write_error = DCAE_UNKNOWN;
	if (strcasecmp(outext, ".dca") == 0) {
		write_error = fDcaWrite(&dcac.out, dcac.out.filename);
	} else if (strcasecmp(outext, ".wav") == 0) {
		write_error = fWavWrite(&dcac.out, dcac.out.filename);
	} else {
		ErrorExit("Unsupported output file type '%s'\n", outext);
	}
	if (write_error)
		printf("write error #%i\n", write_error);
	else
		printf("success\n");
	
	dcaFree(dcacp);
	
	return 0;
}
