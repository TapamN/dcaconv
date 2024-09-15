dcaconv converts audio to a format for the Dreamcast's AICA.

--------------------------------------------------------------------------

Usage Examples:

dcaconv -i source.wav -o result.dca
	Converts a Wave file to a DCA file. The result will be ADPCM compressed. If the source contains too many samples for the AICA to directly play, its sample rate will be reduced to lower the sample count to the point that the AICA can handle it. If the source is stereo, both channels will be averaged together to generate a mono result. The resulting file will always be able to be played fully in hardware by a single AICA channel.

dcaconv -i source.wav -o result.dca -p preview.wav -f pcm16 -S -L
	Converts a Wave file to a DCA file. The result will be uncompressed stereo 16-bit PCM. If the sample rate is above 44.1 KHz, it will be reduced to 44.1 KHz, but otherwise would be kept unchanged. The resulting file may require CPU assistance to play if it is too long.
	
--------------------------------------------------------------------------

Building:

	Run "make".
	
	To generate the proper README with linebreaks, from readme_unformatted.txt, run "make README" or "make all". Requires "fmt".

--------------------------------------------------------------------------

AICA Sound End Value:

The AICA treats the length/loop end point value differently than one might expect. Normally, when playing a sound of that is X samples long, one would expect X samples to be played, samples 0 though X-1. The AICA given a length of X will play one more sample than this (e.g. X+1), and play samples 0 though X. 

The DCA format does NOT account for this. Sound length and loop end point are specified the "normal" way, so a sound with a length of 8 samples would be stored as 8, and not 7 as required by the AICA. When giving the AICA the length of the sound, the user must subtract one from the length. This was done to make the DCA format work more like one would expect, and keep the length correction to just AICA code. 

--------------------------------------------------------------------------

AICA Limits (Sample Rates and Max Length):

The AICA outputs 44.1 KHz audio. There is normally little use to storing audio with a higher sample rate, as playing at higher frequencies than 44.1 KHz will generally introduce aliasing and wastes sound RAM, so dcaconv will downsample source audio at >44.1KHz to 44.1 KHz by default. If you wish to disable this, you can do so with --rate keep, or specify a custom sample rate with --rate [number].

There is a hard limit of 88.2 Khz for ADPCM's the maximum sample rate. PCM8 and PCM16 can be played at higher rate. It's not recommended to play audio above 44.1 KHz due to the aliasing this would cause.

An AICA channel is limited to playing sounds of 2^16 samples long. At a sample rate of 44100 hz, a sound could be at most 1.4 seconds long. At 22050 hz, the limit is 2.8 seconds. By default, when the source file is larger than 2^16 samples, dcaconv will reduce the sample rate of the output file enough so that the result is less than 2^16 samples long.

Resampling is done using libsamplerate.

Enabling the --long option will disable this and allow for longer sounds. The resulting files cannot be played purely by the AICA and will require software assistance from the SH4 or ARM CPU by stream the samples into a looping buffer.

The AICA is said to sometimes have issues with looping when a sound is near 2^16 samples long. So work around this, dcaconv will target having a bit less than exactly 2^16 samples when converting.

	Examples of --rate and --long:
	[no options]
		If >44KHz, reduce to 44Khz. If length is >2^16 samples, reduce sample rate to output 2^16. Resulting file can always be played by a single AICA channel without software assistance.
	--rate 32000
		Target 32Khz. If length at 32Khz is >2^16 samples, reduce sample rate to output 2^16 samples
	--long
		If >44KHz, reduce to 44Khz. If length is >2^16 samples, output more than 2^16 samples
	--rate 32000 --long
		Target 32Khz. If length at 32Khz is >2^16 samples, output more than 2^16 samples

--------------------------------------------------------------------------

Stereo:

The AICA does not directly support stereo sound clips. To play a stereo sound, one AICA sound channel must play the left stereo channel with a left pan, then a second AICA sound channel must play the right stereo channel with a right pan.

The DCA format stores multichannel audio as non-interleaved, as the AICA cannot play interleaved audio. After the header, the first channel's samples are stored, then possible padding to align to a 32 byte boundary, followed by the second channel's samples. For stereo, left is in the first channel, with right in the second channel.

--------------------------------------------------------------------------

Looping:

When adding loop points, it's highly recommended to convert the source data into the final sample rate before converting to ensure that no pops or clicks occur at the loop point if resampling needs to be done.

--------------------------------------------------------------------------

Command Line Options:

--help, -h
	Displays help

--version, -V
	Displays version

--in [filename], -i [filename]
	Input audio file. This option is required.
	
	The following formats are supported:
	
	.WAV
		Uses dr_wav library. Supports multiple formats, including signed integer, floating point, and several ADPCM formats.
	.FLAC
		Uses dr_flac library.
	.OGG (Vorbis)
		Uses stb_vorbis library
	.MP3
		Uses dr_mp3 library.
	.DCA
		It's possible to convert DCA to WAV

--out [filename], -o [filename]
	Sets the file name of the resulting audio. The extension of this filename controls the file format.

	The supported formats are:

	.DCA
		This is a format optimized for the Dreamcast.
	.WAV
		Standard Wave file.
	
--format [type], -f [type]
	Sets the encoding format of the resulting audio for DCA files. Has no effect when outputting .WAV files.
	
	[type] can be one of the following:
	
	ADPCM
		4-bit per sample ADPCM. This is the default for .DCA files if the format isn't specified.
	PCM8
		8-bit per sample PCM.
	PCM16
		16-bit per sample PCM.
	
--preview [filename], -p [filename]
	Generates a preview of the resulting audio file, after resampling and encoding. The preview must always be .WAV format.
	
--rate [integer], -r [integer]
	Sets the desired sample rate of resulting audio. The handling of the sample rate depends on the output format:
	
	.WAV
		If --rate is not specified, it always keeps the source sample rate. If a specific sample rate is specified, that rate will always be used.
		
	.DCA
		The output will target the sample rate in the source file if --rate is not used. If --rate is used with an integer parameter, it will target the specified rate.
	
		When a target rate has not been specified, it will target the source file's sample rate. If the source sample rate is above 44100 hz, the target sample rate will be set to 44100 hz, to match the AICA's output rate.
		
		If the audio is more than 2^16 samples long at the target sample rate, the sample rate will be reduced to fit the sound into the AICA's 2^16 sample limit. For 44100 hz audio, 2^16 samples is about 1.4 seconds long; audio longer than that would have it's sample rate reduced.
		
		The --long option can disable the 2^16 sample limit, and generate longer sounds. The resulting file cannot be played fully in hardware, and will require CPU assistance to stream samples into a playback buffer.
		
		For ADPCM, the AICA has a hard upper limit of 88200 Hz.

--long, -L
	Generates long .DCA files. The AICA is limited to playing sounds no longer than 2^16 samples long without streaming. If --long is not specified, and the input is more than 2^16 sample long, its sample rate will be reduced so that the result fits in 2^16. If --long is specified, a file longer than 2^16 samples will be generated and the sample rate will not be changed. See "AICA Max Length Resampling" above for more information.
		
--channels [integer], -c [integer}
	The number of channels to output in the resulting file. If --channels isn't specified, default behavior depends on output format. For .WAV, the channel count is kept the same. For .DCA, the channel count defaults to 1, so multichannel inputs are downmixed to mono. Sound effects are not typically stereo.
	
	The desired channel count must either equal the input channel count (and keep all channels), or be one (and average all channels together). It's not possible to, for example, downmix 5.1 audio to stereo, or convert mono channel to stereo.

--stereo, -S
	This is equivalent to "--channels 2"

--trim [ends], -t [ends]
	Trim silence off the start or end of the input audio.
	
	The method of trimming is currently very crude, and just removes samples until it find a sample with a 16-bit absolute value greater than 256.
	
	[ends] controls what end of the audio to trim. The options are:
	START
		Trim only the beginning of the audio
	END
		Trim only the end of the audio
	BOTH
		Trim both the start and end of the audio. This is the default if not the [ends] is not specified.
	
	If a loop start or end point is specified, that point will not be trimmed off. If --loop is specified without explicit loop points with --loop-start or --loop-end, the audio will be trimmed before setting the "entire audio" loop points.
--loop, -l
	Marks the output as looping. If no start or end is specified, the entire audio will loop.
	
--loop-start [sample_pos], -s [sample_pos]
	Sets position to return to when looping. The first sample in the audio is sample 0, not 1. This option automatically implies --loop. Must be less than loop-end.
	
--loop-end [sample_pos], -e [sample_pos]
	Sets position of when to trigger loop. The first sample in the audio is sample 0, not 1. The sample at this position will not be played, and the sample before it will be. This option automatically implies --loop. Must be greater than loop-start.

--trim-loop-end, -E
	Trim samples after loop end

--verbose, -v
	Print extra information on conversion process
	
--------------------------------------------------------------------------

.DCA File Format:

	See file_dca.h for documentation. file_dca.h can also be used as a library to help access information from the file's header.
	
	Most functions in file_dca.h are simple enough that they declared "static inline" and are effectively type checked macros, but some larger functions are not. To use these functions, in one file in the executable, #define DCAUDIO_IMPLEMENTATION before including the header.

--------------------------------------------------------------------------

Future Ideas:

	* Improve ADPCM quality
	
	* Add support for generating other file formats, like ADX.
	
	* Better downmixing to stereo/mono

--------------------------------------------------------------------------

Possible Issues:

	ADPCM looping has not been throughly tested. Not sure how the AICA handles the ADPCM prediction on when looping. Looping seems to work correctly when the sound is less than 2^16 samples long, which indicates the AICA remembers the ADPCM prediction for the loop start. That means it probably won't work as expected on long sounds.

--------------------------------------------------------------------------

History:

	Version 1.0
		Initial release

--------------------------------------------------------------------------

License:

	GPL