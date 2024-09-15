#ifndef FILE_DCA_H
#define FILE_DCA_H

#include <stdint.h>
#include <stdbool.h>

/*
	Most functions provided by this header are provided as static inline, 
	as they are simple enough that there's no point in preforming an 
	actual function call for.
	
	Some more complex functions are not static inline. This header always 
	provides their prototypes, but their implementation is optionally 
	output, to prevent errors from having multiple definitions of the 
	same fucnction.
	
	To generate them, #define DCAUDIO_IMPLEMENTATION before including 
	this header. This should only be done in one file.
*/

#define DCA_ALIGNMENT	32
#define DCA_ALIGNMENT_MASK	0x1f

#define DCA_MINIMUM_SAMPLE_RATE_HZ	172
#define DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ	88200

//Max channels supported by file format
#define DCA_FILE_MAX_CHANNELS	7

#define DCA_FOURCC_STR	"DcAF"
#define DCA_FOURCC_UINT	0x46416344	//"DcAF" loaded as little endian int

#define DCA_FLAG_CHANNEL_COUNT_SHIFT	0
#define DCA_FLAG_CHANNEL_COUNT_MASK	0x7

//These flags match the AICA's format selection bits
#define DCA_FLAG_FORMAT_PCM16	0x0
#define DCA_FLAG_FORMAT_PCM8	0x1
#define DCA_FLAG_FORMAT_ADPCM	0x2
#define DCA_FLAG_FORMAT_INVALID	0x3
#define DCA_FLAG_FORMAT_SHIFT	7
#define DCA_FLAG_FORMAT_MASK	0x3

//Has loop data
#define DCA_FLAG_LOOPING	(1<<9)

#define DCA_LONG_THRESHOLD	((1<<16) - 64)

//Bits from flags to write to AICA channel
#define DCA_AICA_MASK	((DCA_FLAG_FORMAT_MASK<<DCA_FLAG_FORMAT_SHIFT) | DCA_FLAG_LOOPING)

typedef struct {
	/*
		All data is little endian (same as the Dreamcast's SH-4)
		
		Header is 32 bytes large
	*/
	
	/*
		Four character code to identify file format.
		Always equal to "DcAF"
	*/
	union {
		char fourcc[4];
		uint32_t fourcc_uint;
	};
	
	/*
		Size of file, including header. This is different to IFF, 
		which does not include the size of the fourcc and size 
		fields.
		
		Size is always rounded up to 32 bytes
	*/
	uint32_t chunk_size;
	
	/*
		Version of file format. Currently only 0
	*/
	uint8_t version;
	
	/*
		Should always be 0 in current version.
	*/
	uint8_t padding0[3];
	
	/*
		TODO explain DCA_FLAG_*
	*/
	uint16_t flags;
	
	/*
		The sample rate of the audio stored in the 15-bit floating point format used by the AICA.
	*/
	uint16_t sample_rate_aica;
	
	/*
		Total length of the audio in samples, including any samples 
		past the end of the loop point.
		
		**********************IMPORTANT**********************
		The AICA interprets the end of the sound to be one sample 
		later than one would normally expect, so the value written to 
		the AICA length register must be this value MINUS ONE.
		
		Example:
		
		With a sound that is 100 samples long, the value stored in 
		length will be 100, but the value given to the AICA must be 
		99.
		**********************IMPORTANT**********************
		
		If a sound does not loop, length will also be stored in 
		loop_end. Normally, the value to give to the AICA for the 
		length of the sample can always be found in loop_end. This 
		value is only useful if calculating the size of a channel, or 
		if you have a looping sample that you want to play without 
		looping.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. If the sound is longer than that, the 
		DCA_FLAG_LONG_SAMPLE bit will be set in the flags. Some form 
		of streaming must be used for longer sounds.
	*/
	uint32_t total_length;	//NOTE: You probably want to use loop_end value for AICA!
	
	/*
		The start of any loop used by this sound, specified in samples.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. See the description for total_length for more details.
	*/
	uint32_t loop_start;
	
	/*
		The trigger point of a loop, specified in samples. The sample 
		at this position should not be played. This is an absolute 
		position, and not relative to loop_start.
		
		**********************IMPORTANT**********************
		The AICA interprets the end of the sound to be one sample 
		later than one would normally expect, so the value written to 
		the AICA length register must be this value MINUS ONE.
		
		Example:
		
		With a sound that loops after 100 samples, the value stored 
		in loop_end will be 100, but the value given to the AICA must 
		be 99.
		**********************IMPORTANT**********************
		
		If the sound does not loop, this is will be equal to length.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. See the description for total_length for more details.
	*/
	uint32_t loop_end;
	
	/*
		Not used in the current version
	*/
	uint32_t padding1;
	
	/*
		Sample data follows the header.
		
		The data for each channel is stored seperated from one 
		another, and not interleaved. For stereo sounds, channel 0 
		contains the left channel, and channel 1 contains the right 
		channel.
		
		Each channel is padded out to 32 bytes for compatability with DMA.
		
		The size of each channel, in bytes, can be calculated using 
		fDaCalcChannelSizeBytes.
		
		The size of all channels combined, in bytes, can be 
		calculated using fDaGetDataSize.
	*/
} fDcAudioHeader;

/*
	Returns true if the fourcc matches
*/
static inline bool fDaFourccMatches(const fDcAudioHeader *dca) {
	return dca->fourcc_uint == DCA_FOURCC_UINT;
}

/*
	Returns version of file
*/
static inline int fDaGetVersion(const fDcAudioHeader *dca) {
	return dca->version;
}

/*
	Returns the sample format used.
	
	Returns one of
		DCA_FLAG_FORMAT_PCM16
		DCA_FLAG_FORMAT_PCM8
		DCA_FLAG_FORMAT_ADPCM
*/
static inline unsigned fDaGetSampleFormat(const fDcAudioHeader *dca) {
	return (dca->flags >> DCA_FLAG_FORMAT_SHIFT) & DCA_FLAG_FORMAT_MASK;
}

/*
	Returns number of channels stored in file
*/
static inline unsigned fDaGetChannelCount(const fDcAudioHeader *dca) {
	return (dca->flags >> DCA_FLAG_CHANNEL_COUNT_SHIFT) & DCA_FLAG_CHANNEL_COUNT_MASK;
}

/*
	Returns 0 if sound has no loop, or non-zero if sound has loops
*/
static inline unsigned fDaIsLooping(const fDcAudioHeader *dca) {
	return dca->flags & DCA_FLAG_LOOPING;
}

/*
	Returns size of file, including header and sound data. Will always be a multiple of 32 (assuming valid file).
*/
static inline size_t fDaGetFileSize(const fDcAudioHeader *dca) {
	return dca->chunk_size;
}

/*
	Returns the size of file, minus header size. It's the size of all samples.
*/
static inline size_t fDaGetDataSize(const fDcAudioHeader *dca) {
	return dca->chunk_size - sizeof(fDcAudioHeader);
}

/*
	Returns length of sound in samples.
	
	If the sound does not loop, this is the entire sound.
	
	If the sound does loop, this is value for the loop end, and does not 
	include any samples stored after the loop end.
*/
static inline size_t fDaGetLength(const fDcAudioHeader *dca) {
	//Yes, this is correct. loop_end will contain the correct value even if looping isn't used.
	return dca->loop_end;
}
static inline size_t fDaGetLoopEnd(const fDcAudioHeader *dca) {
	return dca->loop_end;
}

/*
	Returns loop start position.
	
	If the sound does not loop, this will be zero.
*/
static inline size_t fDaGetLoopStart(const fDcAudioHeader *dca) {
	return dca->loop_start;
}


/*
	Returns value to write to AICA length register.
	
	If the sound does not loop, this is the entire sound.
	
	If the sound does loop, this is value for the loop end, and does not 
	include any samples stored after the loop end.
*/
static inline size_t fDaGetAICALength(const fDcAudioHeader *dca) {
	//Yes, this is correct. loop_end will contain the correct value even if looping isn't used.
	return dca->loop_end - 1;
}

/*
	Returns value to write to AICA length register when playing a looping 
	sound with looping disabled.
*/
static inline size_t fDaGetAICALengthDisableLoop(const fDcAudioHeader *dca) {
	return dca->total_length - 1;
}

/*
	Returns the total size of the sound in samples. Includes any samples 
	past the loop end, if they exist.
	
	The only time is won't be equal to loop_end is if the file is looping 
	and has samples past the end of the loop.
*/
static inline size_t fDaGetTotalLength(const fDcAudioHeader *dca) {
	return dca->total_length;
}

/*
	Returns 0 if sound is less than 2^16 samples and can be played 
	unassisted by AICA, or non-zero if sound more than that and requires 
	CPU assistance for playback.
*/
static inline unsigned fDaIsLong(const fDcAudioHeader *dca) {
	return fDaGetTotalLength(dca) > DCA_LONG_THRESHOLD;
}

/*
	Returns the size of a single channel in bytes
	
	Each channel is padded to be 32 bytes long.
*/
size_t fDaCalcChannelSizeBytes(const fDcAudioHeader *dca);

/*
	Converts a sample rate in hertz to an AICA pitch value
*/
unsigned fDaConvertFrequency(unsigned int freq_hz);

/*
	Converts an AICA pitch value to hertz
*/
float fDaUnconvertFrequency(unsigned int freq);

/*
	Returns the size of a single channel, in bytes. This includes any padding.
*/
size_t fDaCalcChannelSizeBytes(const fDcAudioHeader *dca);

/*
	Returns a pointer to the start of the samples for a channel.
*/
static inline void * fDaGetChannelSamples(fDcAudioHeader *dca, unsigned channel) {
	char * ch = (char*)(dca+1);
	return (void*)(ch + fDaCalcChannelSizeBytes(dca) * channel);
}

/*
	Returns the sample rate of the sound in hertz.
	
	This is a floating point value, 
*/
static inline float fDaCalcSampleRateHz(const fDcAudioHeader *dca) {
	return fDaUnconvertFrequency(dca->sample_rate_aica);
}

/*
	Returns the sample rate value used by AICA.
*/
static inline int fDaGetSampleRateAICA(const fDcAudioHeader *dca) {
	return dca->sample_rate_aica;
}

/*
	Returns a string name for a format (like "ADPCM" for DCA_FLAG_FORMAT_ADPCM.
*/
const char * fDaFormatString(unsigned format);
static inline const char * fDaFormatStringHdr(const fDcAudioHeader *dca) {
	return fDaFormatString(fDaGetSampleFormat(dca));
}

#ifdef DCAUDIO_IMPLEMENTATION
size_t fDaCalcChannelSizeBytes(const fDcAudioHeader *dca) {
	size_t sz = dca->total_length;
	unsigned fmt = fDaGetSampleFormat(dca);
	if (fmt == DCA_FLAG_FORMAT_PCM16) {
		sz *= 2;
	} else if (fmt == DCA_FLAG_FORMAT_ADPCM) {
		//Round up to whole byte
		sz = (sz+1) / 2;
	}
	return (sz + DCA_ALIGNMENT_MASK) & ~DCA_ALIGNMENT_MASK;
}

unsigned fDaConvertFrequency(unsigned int freq_hz) {
	uint32_t freq_lo, freq_base = 5644800;
	int freq_hi = 7;

	while(freq_hz < freq_base && freq_hi > -8) {
		freq_base >>= 1;
		freq_hi--;
	}

	//~ freq_lo = (freq_hz << 10) / freq_base;
	freq_lo = ((freq_hz << 10) + freq_base/2) / freq_base;
	return ((freq_hi & 0xf) << 11) | (freq_lo & 1023);
}

float fDaUnconvertFrequency(unsigned int freq) {
	freq &= 0x7fff;
	
	int freq_hi = (freq >> 11) & 0xf;
	unsigned int freq_lo = freq & 0x3ff;
	if (freq_hi & 0x8)
		freq_hi |= 0xfffffff0;
	
	float newfreq = 44100 * pow(2, freq_hi) * (1+(float)freq_lo/1024);
	
	return newfreq;
}

int fDaValidateHeader(const fDcAudioHeader *dca) {
	bool valid = true;
	
	if (dca == NULL)
		return false;
		
	valid &= fDaGetFileSize(dca) > sizeof(fDcAudioHeader);
	
	//Check fourcc matches
	valid &= fDaFourccMatches(dca);
	
	//Currently, only version is 0. There will probably not be more than 50 versions,
	//so anything more than that is suspicious
	valid &= fDaGetVersion(dca) < 50;
	
	//Size should be multiple of 32
	valid &= (fDaGetFileSize(dca) % DCA_ALIGNMENT) == 0;
	
	//Check for invalid format
	valid &= fDaGetSampleFormat(dca) != DCA_FLAG_FORMAT_INVALID;
	
	//Check size is right
	valid &= fDaGetFileSize(dca) == (sizeof(fDcAudioHeader) + fDaGetChannelCount(dca) * fDaCalcChannelSizeBytes(dca));
	
	//Loop start can't be past end
	valid &= fDaGetLoopStart(dca) <= fDaGetLength(dca);
	
	//Loop end can be longer than total samples
	valid &= fDaGetLength(dca) <= fDaGetTotalLength(dca);
	
	//Sample rate value is 15 bits, top bit should be clear
	valid &= (fDaGetSampleRateAICA(dca) & 0x8000) == 0;
	
	return valid;
}

const char * fDaFormatString(unsigned format) {
	static const char *fmtstrs[] = {"PCM16", "PCM8", "ADPCM", "UNK"};
	
	if (format > 3) format = 3;
	
	return fmtstrs[format];
}
#endif

#endif