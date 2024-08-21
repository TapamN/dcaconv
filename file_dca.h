#ifndef FILE_DCA_H
#define FILE_DCA_H

#include <stdint.h>

#define DCA_MINIMUM_SAMPLE_RATE_HZ	172
#define DCA_MAXIMUM_ADPCM_SAMPLE_RATE_HZ	88200

//Max channels supported by file format
#define DCA_FILE_MAX_CHANNELS	8

#define DCA_FOURCC	"DcAF"

#define DCA_FLAG_CHANNEL_COUNT_MASK	0x7

#define DCA_FLAG_FORMAT_PCM16	0x0
#define DCA_FLAG_FORMAT_PCM8	0x1
#define DCA_FLAG_FORMAT_ADPCM	0x2
#define DCA_FLAG_FORMAT_OTHER	0x3
#define DCA_FLAG_FORMAT_SHIFT	7
#define DCA_FLAG_FORMAT_MASK	0x3

//Has loop data
#define DCA_FLAG_LOOPING	(1<<9)

//Sound is longer than AICA maximum length
#define DCA_FLAG_LONG	(1<<10)

//Largest value that can be stored in DcAudioHeader.sample_rate_hz
//If the sample rate is greater than this, it must be calculated by converting from DcAudioHeader.sample_rate_aica
#define DCA_MAX_STORED_SAMPLE_RATE_HZ	((unsigned)((1<<16)-1))

//Bits to write to AICA channel
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
	char fourcc[4];
	
	/*
		Size of file, including header. This is different to IFF, which does not include the size of the fourcc and size fields.
		
		Size is always rounded up to 32 bytes
	*/
	uint32_t chunk_size;
	
	/*
		Version of file format. Currently only 0
	*/
	uint8_t version;
	
	/*
		Size of the header in 32-byte units, minus one. This is how far from the start of the header the texture data starts.
		The header can be 32 bytes to 8KB large.
		
		A header_size of 0 means the texture data starts 32 bytes after the start of the header, a size of 3 means 128 bytes...
		
		This allows for backwards compatible changes to the size of the header, or adding additional user data.
	*/
	uint8_t header_size;
	
	/*
		TODO explain DCA_FLAG_*
	*/
	uint16_t flags;
	
	/*
		The sample rate of the audio in hertz.
		
		If the sample rate is greater than DCA_MAX_SAMPLE_RATE_HZ, 
		sample_rate_hz will be set to DCA_MAX_SAMPLE_RATE_HZ. To find 
		the real sample rate, the value in sample_rate_aica must be 
		converted to Hz.
	*/
	uint16_t sample_rate_hz;
	
	/*
		The sample rate of the audio stored in the 15-bit floating point format used by the AICA.
	*/
	uint16_t sample_rate_aica;
	
	/*
		Length of the audio in samples.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. If the sound is longer than that, the 
		DCA_FLAG_LONG_SAMPLE bit will be set in the flags. Some form 
		of streaming must be used for longer sounds.
	*/
	uint32_t length;
	
	/*
		The start of any loop used by this sound. The position is 
		specified in samples.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. See the description for length for more details.
	*/
	uint32_t loop_start;
	
	/*
		What sample to trigger the loop on. This sample gets played, but not any afterwards.
		
		The AICA does not natively support sounds longer than 2^16 
		samples. See the description for length for more details.
	*/
	uint32_t loop_end;
	
	uint32_t padding;
} DcAudioHeader;

/*
	Returns the size of the header in bytes
*/
static inline size_t fDaGetHeaderSize(const DcAudioHeader *dca) {
	return (dca->header_size+1) * 32;
}

#endif