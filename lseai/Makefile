# Makefile for Gemini AI Clients (CLI and Motif/CDE)
# Target: Solaris 2.5.1 / GCC 4.3.6

PREFIX       = /usr/local/lse
CC           = gcc

# -------------------------------------------------------------
# Compiler Flags
# -------------------------------------------------------------
# Flags for standard C files (strict warnings + C99)
CFLAGS       = -g -std=gnu99 -Wall -D__EXTENSIONS__ -include $(PREFIX)/include/lsecompat.h -I$(PREFIX)/include

# Flags for Motif/X11 C files (silences SunOS X11 header noise)
MOTIF_CFLAGS = -g -O2 -include $(PREFIX)/include/lsecompat.h \
               -I/usr/dt/include -I/usr/openwin/include -I$(PREFIX)/include

# -------------------------------------------------------------
# Linker Paths & Flags
# -------------------------------------------------------------
BASE_LDFLAGS = -L$(PREFIX)/lib -R$(PREFIX)/lib
MOTIF_LDFLAGS= -L/usr/dt/lib -R/usr/dt/lib \
               -L/usr/openwin/lib -R/usr/openwin/lib \
               $(BASE_LDFLAGS)

# -------------------------------------------------------------
# Libraries: Dynamic (Default)
# -------------------------------------------------------------
BASE_LIBS    = -lgen -lsocket -lnsl -lcurl -lssl -lcrypto -llsecompat 
CLI_LIBS     = -lreadline -lcurses $(BASE_LIBS)
MOTIF_LIBS   = -ljpeg -lXm -lXt -lX11 $(BASE_LIBS)

# -------------------------------------------------------------
# Libraries: Static (Portable Redistribution)
# Direct path references to .a archives prevent needing /usr/local/lse on target machine
# -------------------------------------------------------------
STATIC_LSE_LIBS  = $(PREFIX)/lib/libcurl.a \
                   $(PREFIX)/lib/libssl.a \
                   $(PREFIX)/lib/libcrypto.a \
                   $(PREFIX)/lib/liblsecompat.a \
                   -lsocket -lnsl -lgen -ldl

STATIC_CLI_LIBS  = $(PREFIX)/lib/libreadline.a \
                   -lcurses \
                   $(STATIC_LSE_LIBS)

# Note: /usr/dt/lib/libXm.a can be used if you want Motif baked into the binary
STATIC_MOTIF_LIBS= -L/usr/dt/lib -L/usr/openwin/lib \
                   -lXm -lXt -lX11 \
                   $(STATIC_LSE_LIBS)

# -------------------------------------------------------------
# Targets & Object Groups
# -------------------------------------------------------------
CLI_TARGET        = lseai
MOTIF_TARGET      = lseai-motif
CLI_STATIC_TARGET = lseai.static
MOTIF_STATIC_TARGET= lseai-motif.static

COMMON_OBJS  = summarize.o \
               state_file.o \
               request.o \
               json.o \
               print_response.o \
               markdown_formater.o \
               iamodels.o \
               config.o \
               globals.o

CLI_OBJS     = lseai.o \
               read_prompt.o

MOTIF_OBJS   = lseai-motif.o \
               image.o 

.PHONY: all clean cli motif static cli-static motif-static

# Default: build dynamic versions
all: $(CLI_TARGET) $(MOTIF_TARGET)

cli: $(CLI_TARGET)

motif: $(MOTIF_TARGET)

# Build standalone / portable binaries for redistribution
static: $(CLI_STATIC_TARGET) $(MOTIF_STATIC_TARGET)

cli-static: $(CLI_STATIC_TARGET)

motif-static: $(MOTIF_STATIC_TARGET)

# -------------------------------------------------------------
# Linking Binaries (Dynamic)
# -------------------------------------------------------------
$(CLI_TARGET): $(CLI_OBJS) $(COMMON_OBJS)
	$(CC) $(CLI_OBJS) $(COMMON_OBJS) -o $(CLI_TARGET) $(BASE_LDFLAGS) $(CLI_LIBS)

$(MOTIF_TARGET): $(MOTIF_OBJS) $(COMMON_OBJS)
	$(CC) $(MOTIF_OBJS) $(COMMON_OBJS) -o $(MOTIF_TARGET) $(MOTIF_LDFLAGS) $(MOTIF_LIBS)

# -------------------------------------------------------------
# Linking Binaries (Semi-Static / Portable)
# -------------------------------------------------------------
$(CLI_STATIC_TARGET): $(CLI_OBJS) $(COMMON_OBJS)
	$(CC) $(CLI_OBJS) $(COMMON_OBJS) -o $(CLI_STATIC_TARGET) $(STATIC_CLI_LIBS)

$(MOTIF_STATIC_TARGET): $(MOTIF_OBJS) $(COMMON_OBJS)
	$(CC) $(MOTIF_OBJS) $(COMMON_OBJS) -o $(MOTIF_STATIC_TARGET) $(MOTIF_LDFLAGS) $(STATIC_MOTIF_LIBS)

# -------------------------------------------------------------
# Compilation Rules
# -------------------------------------------------------------
lseai-motif.o: lseai-motif.c
	$(CC) $(MOTIF_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------------------------------------
# Cleanup
# -------------------------------------------------------------
clean:
	rm -f $(CLI_OBJS) $(MOTIF_OBJS) $(COMMON_OBJS) \
		$(CLI_TARGET) $(MOTIF_TARGET) \
		$(CLI_STATIC_TARGET) $(MOTIF_STATIC_TARGET)
