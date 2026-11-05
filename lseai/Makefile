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
# Libraries
# -------------------------------------------------------------
# Shared backend networking / crypto / compat
BASE_LIBS    = -lgen -lsocket -lnsl -lcurl -lssl -lcrypto -llsecompat 

# CLI extra libraries (readline, terminal control)
CLI_LIBS     = -lreadline -lcurses $(BASE_LIBS)

# Motif extra libraries (Motif, Xt, X11)
MOTIF_LIBS   = -lXm -lXt -lX11 $(BASE_LIBS)

# -------------------------------------------------------------
# Targets & Object Groups
# -------------------------------------------------------------
CLI_TARGET   = lseai
MOTIF_TARGET = lseai-motif

# Group 1: Shared Core Backend Objects (C99 + Wall)
COMMON_OBJS  = summarize.o \
               state_file.o \
               request.o \
               json.o \
               print_response.o \
               markdown_formater.o \
               iamodels.o \
               config.o \
               globals.o

# Group 2: CLI-specific Objects (C99 + Wall)
CLI_OBJS     = lseai.o \
               read_prompt.o \

# Group 3: Motif GUI-specific Objects (Add more GUI .o files here later)
MOTIF_OBJS   = lseai-motif.o 

.PHONY: all clean cli motif

# Default: build both
all: $(CLI_TARGET) $(MOTIF_TARGET)

cli: $(CLI_TARGET)

motif: $(MOTIF_TARGET)

# -------------------------------------------------------------
# Linking Binaries
# -------------------------------------------------------------
# 1. Build CLI executable (no X11/Motif linked)
$(CLI_TARGET): $(CLI_OBJS) $(COMMON_OBJS)
	$(CC) $(CLI_OBJS) $(COMMON_OBJS) -o $(CLI_TARGET) $(BASE_LDFLAGS) $(CLI_LIBS)

# 2. Build Motif GUI executable
$(MOTIF_TARGET): $(MOTIF_OBJS) $(COMMON_OBJS)
	$(CC) $(MOTIF_OBJS) $(COMMON_OBJS) -o $(MOTIF_TARGET) $(MOTIF_LDFLAGS) $(MOTIF_LIBS)

# -------------------------------------------------------------
# Compilation Rules
# -------------------------------------------------------------
# Rule for Motif GUI files: uses MOTIF_CFLAGS
# (When you add new GUI files like gui_dialogs.c, list them here or in a pattern)
lseai-motif.o: lseai-motif.c
	$(CC) $(MOTIF_CFLAGS) -c $< -o $@

# Pattern rule for all standard engine and CLI C files: uses CFLAGS (-std=gnu99 -Wall)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# -------------------------------------------------------------
# Cleanup
# -------------------------------------------------------------
clean:
	rm -f $(CLI_OBJS) $(MOTIF_OBJS) $(COMMON_OBJS) $(CLI_TARGET) $(MOTIF_TARGET)
