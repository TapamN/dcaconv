TARGET = dcaconv
OBJS = main.o file_dca.o file_wav.o file_vorbis.o dr_wav_impl.o optparse_impl.o wav2adpcm.o util.o \
	stb_vorbis.o file_flac.o file_mp3.o \
	libsamplerate/src/samplerate.o \
	libsamplerate/src/src_linear.o \
	libsamplerate/src/src_zoh.o \
	libsamplerate/src/src_sinc.o

MYFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Ilibsamplerate/include/
#For libsamplerate
MYFLAGS+=-DPACKAGE=\"dcaconv\" -DVERSION=\"1\" -DHAVE_STDBOOL_H -DENABLE_SINC_BEST_CONVERTER

MYCPPFLAGS=$(MYFLAGS)
MYCFLAGS=$(MYFLAGS)
#~ DEBUGOPT= -Og -g
DEBUGOPT= -O3

.PHONY: all clean install README

%.o: %.c
	gcc $(CFLAGS) $(MYCFLAGS) $(DEBUGOPT) -c $< -o $@

%.o: %.cpp
	gcc $(CFLAGS) $(MYCPPFLAGS) $(CXXFLAGS) $(DEBUGOPT) -c $< -o $@


$(TARGET): $(OBJS)
	gcc -o $(TARGET) \
		$(OBJS) $(PROGMAIN) -lm -lstdc++

README: readme_unformatted.txt
	fmt -s readme_unformatted.txt > README

install: $(TARGET)
	install ./dcaconv ~/.local/bin/dcaconv

clean:
	rm -f $(TARGET) $(OBJS)
