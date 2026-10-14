# ==============================================================================
# Makefile for lse-compat (Multi-OS: SunOS 4.x / Solaris 2.5.1+)
# ==============================================================================

# OS & Platform Detection
UNAME_S := $(shell uname -s)
UNAME_R := $(shell uname -r)

# Handle prefix / PREFIX override
ifdef prefix
  PREFIX = $(prefix)
endif
PREFIX ?= /usr/local/lse

INCDIR     = $(PREFIX)/include
LSE_INCDIR = $(INCDIR)/lse
LIBDIR     = $(PREFIX)/lib
# Customisable RPATH flag (can be overridden via: make RPATH_FLAG="-Wl,-R,$(LIBDIR)")
# Default uses GNU ld syntax (-Wl,-rpath,...)
#RPATH_FLAG ?= -Wl,-rpath,$(LIBDIR)
RPATH_FLAG ?= -Wl,-R$(LIBDIR) -Wl,-rpath,$(LIBDIR)

# Toolchain
CC      = gcc
AR      = ar
RANLIB  = ranlib
RM      = rm -f

# Base Flags
BASE_CFLAGS  = -g -O2 -mcpu=v7 -fPIC
ALL_CFLAGS   = $(BASE_CFLAGS) $(CFLAGS)
ALL_CPPFLAGS = -DHAS_PROCFS_SVR4 -DSOLARIS_COMPATIBLE -DHAVE_LONG_LONG -DHAVE_LONG_DOUBLE -I. -I$(INCDIR) $(CPPFLAGS)

# Dynamic Library Versioning
SHLIB_MAJOR = 1
SHLIB_MINOR = 0

# ------------------------------------------------------------------------------
# OS-Specific Configuration
# ------------------------------------------------------------------------------

# Base sources common to both SunOS 4 and Solaris 2.5.1
COMMON_SRCS = strtoimax_compat.c \
              ftello_compat.c \
              snprintf_compat.c \
              getprogname_compat.c \
              usleep_compat.c \
              strerror_r_compat.c \
              time_compat.c \
              math_compat.c \
              getline_compat.c \
	      vdprintf_compat.c


W_SRCS      = mbstowcs_compat.c \
              wcsstr_compat.c 

ATOMIC_SRCS = atomic_compat.c

NET_SRCS    = dns_rfc2553_compat.c \
              socket_compat.c

ifeq ($(UNAME_S),SunOS)
  OS_MAJOR := $(firstword $(subst ., ,$(UNAME_R)))
  
  ifeq ($(OS_MAJOR),4)
    # --- SunOS 4.x Configuration (a.out) ---
    OS_TYPE          = SunOS4
    LSECOMPAT_SRCS   = $(COMMON_SRCS) string_compat.c
    
    # SunOS 4 shared lib syntax: liblsecompat.so.1.0 (requires ld -assert pure-text)
    LIB_LSECOMPAT_SO = liblsecompat.so.$(SHLIB_MAJOR).$(SHLIB_MINOR)
    LIB_LSENET_SO    = liblsenet.so.$(SHLIB_MAJOR).$(SHLIB_MINOR)
    LIB_LSEW_SO      = liblsew.so.$(SHLIB_MAJOR).$(SHLIB_MINOR)
    LIB_LSEATOMIC_SO      = liblseatomic.so.$(SHLIB_MAJOR).$(SHLIB_MINOR)
    
    BUILD_SO_CMD     = ld -assert pure-text
    SHLIB_LDFLAGS    =
    NET_EXTRA_LIBS   =
  else
    # --- Solaris 2.x / SunOS 5.x Configuration (ELF) ---
    OS_TYPE          = Solaris2
    # Solaris 2.5.1 already has string functions, skip string_compat.c
    LSECOMPAT_SRCS   = $(COMMON_SRCS)
    
    # ELF shared lib syntax: liblsecompat.so
    LIB_LSECOMPAT_SO = liblsecompat.so
    LIB_LSENET_SO    = liblsenet.so
    LIB_LSEW_SO      = liblsew.so
    LIB_LSEATOMIC_SO      = liblseatomic.so
    
    # Solaris ELF shared library creation via GCC with embedded RPATH
    BUILD_SO_CMD     = $(CC) -shared
    SHLIB_LDFLAGS    = -Wl,-h,$@ $(RPATH_FLAG)
    NET_EXTRA_LIBS   = -lsocket -lnsl
  endif
endif

# Object Files
LSECOMPAT_OBJS = $(LSECOMPAT_SRCS:.c=.o)
LSENET_OBJS    = $(NET_SRCS:.c=.o)
LSEW_OBJS      = $(W_SRCS:.c=.o)
LSEATOMIC_OBJS      = $(ATOMIC_SRCS:.c=.o)

# Target Static Libraries
LIB_LSECOMPAT_A = liblsecompat.a
LIB_LSENET_A    = liblsenet.a
LIB_LSEATOMIC_A      = liblseatomic.a

# Headers
HEADERS_TOP    = lsecompat.h lsenet.h lsew.h lseatomic.h stdint.h inttypes.h
HEADERS_LSE    = string_compat.h \
                 snprintf_compat.h \
                 math_compat.h \
                 strtoimax_compat.h \
                 ftello_compat.h \
                 getprogname_compat.h \
                 usleep_compat.h \
                 time_compat.h \
                 dns_rfc2553_compat.h \
                 socket_compat.h \
                 mbstowcs_compat.h \
                 wcsstr_compat.h \
                 strerror_r_compat.h \
                 map_compat.h \
                 vterm_compat.h \
                 wchar_compat.h \
                 unistd_compat.h \
                 atomic_compat.h \
                 getline_compat.h \
                 ctime_r_compat.h \
		 vdprintf_compat.h


# ------------------------------------------------------------------------------
# Build Rules
# ------------------------------------------------------------------------------

all: info $(LIB_LSECOMPAT_A) $(LIB_LSENET_A) $(LIB_LSEW_A) $(LIB_LSEATOMIC_A) \
     $(LIB_LSECOMPAT_SO) $(LIB_LSENET_SO) $(LIB_LSEW_SO) $(LIB_LSEATOMIC_SO)

info:
	@echo "=== Building lse-compat for Target OS: $(OS_TYPE) ($(UNAME_S) $(UNAME_R)) ==="

# Static Archives
$(LIB_LSECOMPAT_A): $(LSECOMPAT_OBJS)
	@echo "=== Creating Static Archive $@ ==="
	$(RM) $@
	$(AR) cr $@ $(LSECOMPAT_OBJS)
	$(RANLIB) $@

$(LIB_LSENET_A): $(LSENET_OBJS)
	@echo "=== Creating Static Archive $@ ==="
	$(RM) $@
	$(AR) cr $@ $(LSENET_OBJS)
	$(RANLIB) $@

$(LIB_LSEW_A): $(LSEW_OBJS)
	@echo "=== Creating Static Archive $@ ==="
	$(RM) $@
	$(AR) cr $@ $(LSEW_OBJS)
	$(RANLIB) $@

$(LIB_LSEATOMIC_A): $(LSEATOMIC_OBJS)
	@echo "=== Creating Static Archive $@ ==="
	$(RM) $@
	$(AR) cr $@ $(LSEATOMIC_OBJS)
	$(RANLIB) $@

# Shared Objects (.so)
$(LIB_LSECOMPAT_SO): $(LSECOMPAT_OBJS)
	@echo "=== Creating Shared Object $@ ==="
	$(BUILD_SO_CMD) $(SHLIB_LDFLAGS) $(LDFLAGS) -o $@ $(LSECOMPAT_OBJS)

$(LIB_LSENET_SO): $(LSENET_OBJS)
	@echo "=== Creating Shared Object $@ ==="
	$(BUILD_SO_CMD) $(SHLIB_LDFLAGS) $(LDFLAGS) -o $@ $(LSENET_OBJS) $(NET_EXTRA_LIBS)

$(LIB_LSEW_SO): $(LSEW_OBJS)
	@echo "=== Creating Shared Object $@ ==="
	$(BUILD_SO_CMD) $(SHLIB_LDFLAGS) $(LDFLAGS) -o $@ $(LSEW_OBJS) $(NET_EXTRA_LIBS)

$(LIB_LSEATOMIC_SO): $(LSEATOMIC_OBJS)
	@echo "=== Creating Shared Object $@ ==="
	$(BUILD_SO_CMD) $(SHLIB_LDFLAGS) $(LDFLAGS) -o $@ $(LSEATOMIC_OBJS) $(NET_EXTRA_LIBS)

# Pattern Rule
%.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c $< -o $@

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------

install: all
	@echo "=== Installing to $(PREFIX) ==="
	@if [ ! -d $(INCDIR) ]; then mkdir -p $(INCDIR); fi
	@if [ ! -d $(LSE_INCDIR) ]; then mkdir -p $(LSE_INCDIR); fi
	@if [ ! -d $(LIBDIR) ]; then mkdir -p $(LIBDIR); fi
	
	# Install Top-Level Headers
	@for h in $(HEADERS_TOP); do \
		if [ -f $$h ]; then \
			cp $$h $(INCDIR)/$$h; \
			chmod 644 $(INCDIR)/$$h; \
		fi; \
	done
	
	# Install Internal Headers
	@for h in $(HEADERS_LSE); do \
		if [ -f $$h ]; then \
			cp $$h $(LSE_INCDIR)/$$h; \
			chmod 644 $(LSE_INCDIR)/$$h; \
		fi; \
	done

	# Install Static and Shared Libraries
	cp liblse*.a liblse*.so* $(LIBDIR)/
	chmod 644 $(LIBDIR)/liblse*
	$(RANLIB) $(LIBDIR)/$(LIB_LSECOMPAT_A)
	$(RANLIB) $(LIBDIR)/$(LIB_LSENET_A)
	$(RANLIB) $(LIBDIR)/$(LIB_LSEW_A)
	$(RANLIB) $(LIBDIR)/$(LIB_LSEATOMIC_A)
	@echo "=== Installation complete ==="

clean:
	$(RM) *.o *.a *.so*

.PHONY: all info install clean
