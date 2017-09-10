# Notes:
#   MXE was used to cross compile programs for windows
#   make CROSS=i686-w64-mingw32.static- all-windows
#   make CROSS=x86_64-w64-mingw32.static- all-windows

CC=$(CROSS)gcc
CPP=$(CROSS)g++
LD=$(CROSS)ld
AR=$(CROSS)ar
PKG_CONFIG=$(CROSS)pkg-config
CFLAGS=-Wall

all: lasershark_jack lasershark_stdin lasershark_stdin_circlemaker lasershark_stdin_gridmaker lasershark_stdin_edgeline lasershark_stdin_displayimage lasershark_stdin_printimage lasershark_twostep fullprint

all-windows: lasershark_stdin-windows lasershark_stdin_circlemaker-windows lasershark_stdin_gridmaker-windows lasershark_stdin_edgeline-windows lasershark_stdin_displayimage-windows lasershark_stdin_printimage-windows

lasershark_jack: lasershark_jack.c lasersharklib/lasershark_lib.c lasersharklib/lasershark_lib.h
	$(CC) $(CFLAGS) -o lasershark_jack lasershark_jack.c lasersharklib/lasershark_lib.c `$(PKG_CONFIG) --libs --cflags jack libusb-1.0`

lasershark_stdin-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin-windows: lasershark_stdin
lasershark_stdin: lasershark_stdin.c lasersharklib/lasershark_lib.c lasersharklib/lasershark_lib.h \
                    getline_portable.c getline_portable.h getopt_portable.c getopt_portable.h
	$(CC) $(CFLAGS) -o lasershark_stdin lasershark_stdin.c lasersharklib/lasershark_lib.c \
                        getline_portable.c getopt_portable.c `$(PKG_CONFIG) --libs --cflags libusb-1.0`

lasershark_stdin_circlemaker-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin_circlemaker-windows: lasershark_stdin_circlemaker
lasershark_stdin_circlemaker: lasershark_stdin_circlemaker.c
	$(CC) $(CFLAGS) -o lasershark_stdin_circlemaker lasershark_stdin_circlemaker.c -lm

lasershark_stdin_gridmaker-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin_gridmaker-windows: lasershark_stdin_gridmaker
lasershark_stdin_gridmaker: lasershark_stdin_gridmaker.c
	$(CC) $(CFLAGS) -o lasershark_stdin_gridmaker lasershark_stdin_gridmaker.c -x none getopt_portable.c

lasershark_stdin_edgeline-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin_edgeline-windows: lasershark_stdin_edgeline
lasershark_stdin_edgeline: lasershark_stdin_edgeline.c
	$(CC) $(CFLAGS) -o lasershark_stdin_edgeline lasershark_stdin_edgeline.c -x none getopt_portable.c -lm

lasershark_stdin_displayimage-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin_displayimage-windows: lasershark_stdin_displayimage
lasershark_stdin_displayimage: lasershark_stdin_displayimage.c getopt_portable.c getopt_portable.h lodepng/lodepng.cpp lodepng/lodepng.h
	$(CC) $(CFLAGS) -o lasershark_stdin_displayimage lasershark_stdin_displayimage.c -x c lodepng/lodepng.cpp -x none getopt_portable.c

lasershark_stdin_printimage-windows: CFLAGS+= -mno-ms-bitfields
lasershark_stdin_printimage-windows: lasershark_stdin_printimage
lasershark_stdin_printimage: lasershark_stdin_printimage.c getopt_portable.c getopt_portable.h lodepng/lodepng.cpp lodepng/lodepng.h
	$(CC) $(CFLAGS) -o lasershark_stdin_printimage lasershark_stdin_printimage.c -x c lodepng/lodepng.cpp -x none getopt_portable.c -lm

fullprint-windows: CFLAGS+= -mno-ms-bitfields
fullprint-windows: fullprint
fullprint: fullprint.c getopt_portable.c getopt_portable.h
	$(CC) -o fullprint fullprint.c -x none getopt_portable.c -lm
	
lasershark_twostep: lasershark_twostep.c lasersharklib/lasershark_uart_bridge_lib.c lasersharklib/lasershark_uart_bridge_lib.h \
                        twosteplib/ls_ub_twostep_lib.c twosteplib/ls_ub_twostep_lib.h \
                        twosteplib/twostep_host_lib.c twosteplib/twostep_host_lib.h \
                        twosteplib/twostep_common_lib.c twosteplib/twostep_common_lib.h
	$(CC) $(CFLAGS) -o lasershark_twostep lasershark_twostep.c lasersharklib/lasershark_uart_bridge_lib.c \
                        twosteplib/ls_ub_twostep_lib.c twosteplib/twostep_host_lib.c \
                        twosteplib/twostep_common_lib.c `$(PKG_CONFIG) --libs --cflags libusb-1.0`

clean:
	rm -f  *.o lasershark_jack lasershark_stdin lasershark_stdin_circlemaker lasershark_stdin_gridmaker lasershark_stdin_edgeline lasershark_stdin_displayimage lasershark_stdin_printimage lasershark_twostep fullprint
