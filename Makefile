TARGET = dcaconv
OBJS = main.o file_dca.o dr_wav_impl.o optparse_impl.o wav2adpcm.o

MYFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
MYCPPFLAGS=$(MYFLAGS)
MYCFLAGS=$(MYFLAGS) -Wno-pointer-sign
#~ DEBUGOPT= -Og -g
DEBUGOPT= -O3

%.o: %.c
	gcc $(CFLAGS) $(MYCFLAGS) $(DEBUGOPT) -c $< -o $@

%.o: %.cpp
	gcc $(CFLAGS) $(MYCPPFLAGS) $(CXXFLAGS) $(DEBUGOPT) -c $< -o $@


$(TARGET): $(OBJS)
	gcc -o $(TARGET) \
		$(OBJS) $(PROGMAIN) -lm -lstdc++

clean:
	rm -f $(TARGET) $(OBJS)
