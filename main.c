#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#include "dca_conv.h"
#include "dr_wav.h"
#include "optparse.h"
#include "samplerate.h"

#define VERSION_STRING	"1.00"

/*
	Planned switches:
	
	-E disable trim loop end
	-t [level] trim trailing silence
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
		dcaLog(LOG_WARNING, "Can only generate previews for .DCA format output files\n");
		return DCAE_UNSUPPORTED_FILE_TYPE;
	}
	if (strcasecmp(GetExtension(preview_fname), ".wav") != 0) {
		dcaLog(LOG_WARNING, "Can only generate .WAV preview files\n");
		return DCAE_UNSUPPORTED_FILE_TYPE;
	}
	
	DcAudioConverter dcac, *dcacp = &dcac;
	dcaInit(dcacp);
	dcaError retval = fDcaLoad(dcacp, src_fname);
	
	if (retval) {
		dcaLog(LOG_WARNING, "Error retrieving output file for preview (%s)\n", dcaErrorString(retval));
	} else {
		retval = fWavWrite(&dcac, preview_fname);
		if (retval == DCAE_OK) {
			dcaLog(LOG_COMPLETION, "Wrote preview to '%s'\n", preview_fname, retval);
		} else {
			dcaLog(LOG_WARNING, "Could not create preview of '%s' (%s)\n", src_fname, dcaErrorString(retval));
		}
	}
	dcaFree(dcacp);
	
	return retval;
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

enum {
	TRIM_START,
	TRIM_END,
	TRIM_BOTH,
};
static const OptionMap trim_type[] = {
	{"start", TRIM_START},
	{"end", TRIM_END},
	{"both", TRIM_BOTH},
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
	int trim_threshold = 1*256;
	bool trim_silence_start = false;
	bool trim_silence_end = false;
	bool trim_loop_end = false;
	bool loop_start_set = false;
	bool loop_end_set = false;
	
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
		
		{"loop", 'l', OPTPARSE_NONE},
		{"loop-start", 's', OPTPARSE_REQUIRED},
		{"loop-end", 'e', OPTPARSE_REQUIRED},
		
		{"trim", 't', OPTPARSE_OPTIONAL},
		{"long", 'L', OPTPARSE_NONE},
		{"trim-loop-end", 'E', OPTPARSE_NONE},
		
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
		case 'L':
			dcac.long_sound = true;
			break;
		case 'l':
			dcac.looping = true;
			break;
		case 's':
			dcac.looping = true;
			loop_start_set = true;
			if (sscanf(options.optarg, "%u", &dcac.loop_start) != 1)  {
				ErrorExit("Invalid loop start. Must be in sample position. The first sample is 0.\n");
			}
			break;
		case 'e':
			dcac.looping = true;
			loop_end_set = true;
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
		case 't': OPTARG_FIX_UP; {
				if (options.optarg) {
					int trim = GetOptMap(trim_type, ARR_SIZE(trim_type), options.optarg, -1, "invalid trim setting\n");
					if (trim == TRIM_BOTH) {
						trim_silence_start = true;
						trim_silence_end = true;
					} else if (trim == TRIM_START) {
						trim_silence_start = true;
					} else if (trim == TRIM_END) {
						trim_silence_end = true;
					} else {
						assert(0);
					}
				} else {
					trim_silence_start = true;
					trim_silence_end = true;
				}
			} break;
		case 'E':
			trim_loop_end = true;
			break;
		case 'v':
			dcaCurrentLogLevel = LOG_INFO;
			
			//If someone runs this with only -v as a parameter, they probably want the version
			if (argc != 2)
				break;
			//Fallthrough
		case 'V':
			printf("dcaconv - Dreamcast Audio Converter - Version "VERSION_STRING"\n");
			return 0;
		default:
			ErrorExit("Unknown option: %s\n", options.errmsg);
		}
	}
	
	ErrorExitOn(in_fname == NULL, "No input file specified\n");
	ErrorExitOn(out_fname == NULL, "No output file specified\n");
	
	dcaLog(LOG_INFO, "Converting '%s' to '%s'\n", in_fname, out_fname);
	
	const char *inext = GetExtension(in_fname);
	const char *outext = GetExtension(out_fname);
	ErrorExitOn(inext[0] == 0, "Unknown input file type (no extension)\n");
	ErrorExitOn(outext[0] == 0, "Unknown output file type (no extension)\n");
	
	//Load input file
	dcaError loadresult = DCAE_OK;
	
	if (strcasecmp(inext, ".wav") == 0) {
		loadresult = fWavLoad(dcacp, in_fname);
	} else if (strcasecmp(inext, ".dca") == 0) {
		loadresult = fDcaLoad(dcacp, in_fname);
	} else if (strcasecmp(inext, ".ogg") == 0) {
		loadresult = fVorbisLoad(dcacp, in_fname);
	} else if (strcasecmp(inext, ".flac") == 0) {
		loadresult = fFlacLoad(dcacp, in_fname);
	} else if (strcasecmp(inext, ".mp3") == 0) {
		loadresult = fMp3Load(dcacp, in_fname);
	} else {
		ErrorExit("Unknown input file type\n");
	}
	ErrorExitOn(loadresult, "While loading input file: %s\n", dcaErrorString(loadresult));
	
	assert(dcac.channel_cnt > 0);
	assert(dcac.sample_rate_hz > 0);
	assert(dcac.samples[0] != NULL);
	
	//For .DCA, if no sample rate is specified and source sample rate is >44.1Khz, reduce output to 44.1Khz
	//Otherwise, if no sample rate is specified, default to source file rate
	if (strcasecmp(outext, ".dca") == 0 && dcac.desired_sample_rate_hz == 0 && dcac.sample_rate_hz > 44100)
		dcac.desired_sample_rate_hz = 44100;
	else if (dcac.desired_sample_rate_hz == 0)
		dcac.desired_sample_rate_hz = dcac.sample_rate_hz;
	
	//Set loop end if not specified
	if (dcac.loop_end == 0)
		dcac.loop_end = dcac.samples_len;
	ErrorExitOn(dcac.loop_start >= dcac.samples_len, "Loop start is past end of file\n");
	ErrorExitOn(dcac.loop_start == dcac.loop_end, "Loop start is equal to loop end\n");
	ErrorExitOn(dcac.loop_start > dcac.loop_end, "Loop start is after loop end\n");
	if (dcac.loop_end > dcac.samples_len) {
		dcaLog(LOG_WARNING, "\nLoop end (%u) is past end of file (%u), loop end will be set to end\n", dcac.loop_end, dcac.samples_len);
		dcac.loop_end = dcac.samples_len;
	}
	
	//Trim initial/trailing silence
	if (trim_silence_start || trim_silence_end) {
		unsigned new_start = 0, new_end = dcac.samples_len;
		
		//If loop points are explicitly set, do not trim past them, otherwise trim as much as possible
		//and move the loop points in bounds
		if (trim_silence_start) {
			unsigned max_start_trim = loop_start_set ? dcac.loop_start : dcac.samples_len;
			for(unsigned i = 0; i < max_start_trim; i++) {
				for(unsigned c = 0; c < dcac.channel_cnt; c++) {
					if (abs(dcac.samples[c][i]) > trim_threshold) {
						new_start = i;
						goto exit_scan_start;
					}
				}
			}
		exit_scan_start:;
		}
		
		if (trim_silence_end) {
			unsigned max_end_trim = loop_end_set ? dcac.loop_end : 0;
			for(unsigned i = dcac.samples_len; i > max_end_trim; i--) {
				for(unsigned c = 0; c < dcac.channel_cnt; c++) {
					if (abs(dcac.samples[c][i]) > trim_threshold) {
						new_end = i;
						goto exit_scan_end;
					}
				}
			}
		exit_scan_end:;
		}
		//TODO think of how to handle this better
		if (new_end < new_start)
			new_end = new_start;
		
		unsigned new_len = new_end - new_start;
		dcaLog(LOG_INFO, "Trimming results: New start: 0 -> %u, new end: %u -> %u, new len %u\n", new_start, dcac.samples_len, new_end, new_len);
		
		//Rebuild samples arrays with start/end removed
		for(unsigned c = 0; c < dcac.channel_cnt; c++) {
			int16_t *newsamples = malloc(new_len * sizeof(int16_t));
			memcpy(newsamples, dcac.samples[c] + new_start, new_len * sizeof(int16_t));
			free(dcac.samples[c]);
			dcac.samples[c] = newsamples;
		}
		
		dcac.samples_len = new_len;
		
		//Fix up loops
		dcac.loop_start -= new_start;
		if (dcac.loop_start > dcac.samples_len)
			dcac.loop_start = 0;
		dcac.loop_end -= new_start;
		if (dcac.loop_end > dcac.samples_len)
			dcac.loop_end = dcac.samples_len;
	}
	
	if (strcasecmp(outext, ".dca") == 0) {
		//TODO maybe create a few samples of silence instead of error?
		ErrorExitOn(dcac.samples_len == 0, "zero length sound probably doesn't work well on AICA\n");
		
		//If user hasn't specified the number of channels, default to one
		if (dcac.desired_channels == 0)
			dcac.desired_channels = 1;
		
		//Default to ADPCM
		if (dcac.format == DCAF_AUTO)
			dcac.format = DCAF_ADPCM;
		
		//If we are trimming the end of the loop, use that for length instead of current length
		unsigned len = trim_loop_end ? dcac.loop_end : dcac.samples_len;
		unsigned expected_size = (float)len * dcac.desired_sample_rate_hz / dcac.sample_rate_hz;
		if (!dcac.long_sound && expected_size > DCAC_MAX_SAMPLES) {
			//TODO the -64 is a hack to deal with some rounding issues when calculating sample length. find a better fix later
			float ratio = (float)(DCAC_MAX_SAMPLES-64) / len;
			unsigned new_rate = fDcaToAICAFrequency(dcac.sample_rate_hz * ratio);
			unsigned desired_size_samples = (float)len * new_rate / dcac.sample_rate_hz;
			
			ErrorExitOn(new_rate < DCA_MINIMUM_SAMPLE_RATE_HZ, "This sound is too long for the AICA to handle directly.\n"
				"To allow long sounds, use the --long option\n");
			
			dcaLog(LOG_WARNING, "\nInput file is long (%u samples%s). AICA only directly supports sounds shorter than %u samples.\n"
				"Reducing frequency from %u hz to %u hz to fit within AICA limits. Resulting file will be %u samples long\n"
				"To allow long sounds, use the --long option. Playing long sounds will require software assistance to stream samples.\n",
				(unsigned)len,
				trim_loop_end ? " after trimming end of loop" : "",
				(1<<16)-1,
				dcac.sample_rate_hz,
				new_rate,
				desired_size_samples);
			dcac.desired_sample_rate_hz = new_rate;
		}
		
		//The floating point format of the AICA's frequency rate register results in some values getting rounded.
		//Do the rounding here to so resampling calculations will better match actual output
		dcac.desired_sample_rate_hz = fDcaToAICAFrequency(dcac.desired_sample_rate_hz);
		
		
		if (dcac.desired_sample_rate_hz < 172) {
			dcaLog(LOG_WARNING, "\nSample rate of %u is too low. AICA does not support sample rates less than 172 hz. Using 172 hz sample rate\n", dcac.desired_sample_rate_hz);
			dcac.desired_sample_rate_hz = 172;
		} else if (dcac.format == DCAF_ADPCM && dcac.desired_sample_rate_hz > DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ) {
			dcaLog(LOG_WARNING, "\nSample rate of %u is too high for ADPCM. AICA ADPCM does not support sample rates over %u hz, reducing sample rate to %u",
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
	
	//Clamp number of channels to input
	if (dcac.desired_channels > dcac.channel_cnt) {
		dcaLog(LOG_WARNING, "\nSpecifed number of output channels of %u is greater than number "
			"of source file channels of %u. Output will have %u channels.\n",
			dcac.desired_channels,
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
		ErrorExit("Converting from mono to stereo is not currently supported\n");
	} else {
		ErrorExit("Cannot convert from %u channel%s to %u channel%s\n",
			dcac.channel_cnt,
			dcac.channel_cnt > 1 ? "s" : "",
			dcac.channel_cnt,
			dcac.channel_cnt > 1 ? "s" : "");
	}
	
	//Adjust sample rate
	if (dcac.desired_sample_rate_hz != dcac.sample_rate_hz) {
		dcaLog(LOG_PROGRESS, "\nConverting input sample rate from %u hz to %u hz\n", dcac.sample_rate_hz, dcac.desired_sample_rate_hz);
		int src_err = 0;
		SRC_STATE *src = src_new(SRC_SINC_BEST_QUALITY, 1, &src_err);
		assert(src);
		ErrorExitOn(src_err, "Sample rate conversion error (%s)\n", src_strerror(src_err));
		
		//Allocate space to convert to float and store results
		float ratio = (float)dcac.desired_sample_rate_hz / dcac.sample_rate_hz;
		unsigned new_size = dcac.samples_len * ratio;
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
		
		ErrorExitOn(src_err, "Sample rate conversion error (%s)\n", src_strerror(src_err));
		
		src_delete(src);
		
		dcac.sample_rate_hz = dcac.desired_sample_rate_hz;
		dcac.samples_len = new_size;
		//Letting it just truncate so that loop_end can't possibly go past the end
		dcac.loop_start *= ratio;
		dcac.loop_end *= ratio;
	}
	
	//Trim off anything after the end of the loop and fix up any other loop problems
	if (trim_loop_end && dcac.looping && dcac.loop_end < dcac.samples_len)
		dcac.samples_len = dcac.loop_end;
	if (dcac.loop_end > dcac.samples_len)
		dcac.loop_end = dcac.samples_len;
	if (dcac.loop_start >= dcac.loop_end) {
		dcaLog(LOG_WARNING, "Loop start is after loop end, disabling looping\n");
		dcac.loop_start = 0;
		dcac.looping = false;
	}
	
	if (dcac.looping)
		dcaLog(LOG_INFO, "\nFinal loop points: %u to %u\n", dcac.loop_start, dcac.loop_end);
	
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
		dcaLog(LOG_WARNING, "Error writing to '%s' (%s)\n", out_fname, dcaErrorString(write_error));
	else
		dcaLog(LOG_COMPLETION, "\nSuccessfully wrote to '%s'\n", out_fname);
	
	//Write preview
	if (preview && !write_error) {
		GeneratePreview(out_fname, preview);
	}
	
	dcaFree(dcacp);
	
	return 0;
}
