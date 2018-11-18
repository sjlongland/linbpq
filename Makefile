#	LinBPQ Makefile

# Configuration: To override any of these, export them in your environment
# or override them when calling `make`:
# e.g. `make CROSS_COMPILE=arm-linux-gnueabi-` to cross-build for ARM on x86
#   or `make install PREFIX=/usr`              to install to /usr instead.

# Where do we install linbpq's binaries and libraries?
PREFIX ?= /usr/local

# Where should linbpq "live"?  `bpqhome` will be copied to this location.
DATADIR ?= /var/lib/linbpq

# Where are our sources kept?  By default, the current working directory.
SOURCES ?= .

# Where's our work directory?  By default, the same place.
OBJECTS ?= .

# Do we want I²C support?  By default, yes, but you can run
# `make I2C=n` if you wish to disable this.
I2C ?= y

# What compiler do we use?  Default to the GNU C Compiler.
# If you need a cross-compiler, set CROSS_COMPILE appropriately.
CROSS_COMPILE ?=
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++

# What to use for installing files, in case `install` isn't a BSD-compatible
# "install" program?
INSTALL ?= install

# What to do to copy lots of files around
TAR ?= tar

# What C preprocessor flags do we use?
CPPFLAGS := $(CPPFLAGS) -DLINBPQ -MMD

# What C/C++ compiler flags to use?
CFLAGS := $(CFLAGS) -g
CXXFLAGS := $(CXXFLAGS) -g

# What linker flags should we use?
LDFLAGS := $(LDFLAGS) -Xlinker -Map=$(OBJECTS)/output.map

# What libraries do we link in?
LIBS := $(LIBS) -lrt -lm -lpthread -lconfig -lpcap

# ----- Release-related options -----

# What version is this?
VERSION ?= $(shell git describe )

# ----- No further changes beneath here should be needed -----

# What files are we to build?
OBJS = WebMail.o  utf8Routines.o VARA.o LzFind.o Alloc.o LzmaDec.o \
       LzmaEnc.o LzmaLib.o Multicast.o ARDOP.o IPCode.o FLDigi.o \
       linether.o TNCEmulators.o CMSAuth.o APRSCode.o BPQtoAGW.o \
       KAMPactor.o AEAPactor.o HALDriver.o MULTIPSK.o BBSHTMLConfig.o \
       ChatHTMLConfig.o HTMLCommonCode.o BBSUtilities.o bpqaxip.o \
       BPQINP3.o BPQNRR.o cMain.o Cmd.o CommonCode.o compatbits.o \
       config.o datadefs.o FBBRoutines.o HFCommon.o Housekeeping.o \
       HTTPcode.o kiss.o L2Code.o L3Code.o L4Code.o lzhuf32.o \
       MailCommands.o MailDataDefs.o LinBPQ.o MailRouting.o MailTCP.o \
       MBLRoutines.o md5.o Moncode.o NNTPRoutines.o RigControl.o \
       TelnetV6.o WINMOR.o TNCCode.o UZ7HODrv.o WPRoutines.o \
       SCSTrackeMulti.o SCSPactor.o SCSTracker.o HanksRT.o  \
       UIRoutines.o AGWAPI.o AGWMoncode.o

# What targets to build?  We can add to this with +=
TARGETS := linbpq

all: $(addprefix $(OBJECTS)/,$(TARGETS))

ifneq ($(I2C),y)
all: CPPFLAGS+=-DNOI2C
endif

$(OBJECTS)/linbpq: $(addprefix $(OBJECTS)/,$(OBJS))
	$(CC) $^ $(LDFLAGS) $(LIBS) -o $@

$(OBJECTS)/%.o: $(SOURCES)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(OBJECTS)/linbpq
	install -d $(DESTDIR)$(PREFIX)/bin
	install -t $(DESTDIR)$(PREFIX)/bin $(OBJECTS)/linbpq
	setcap "CAP_NET_ADMIN=ep CAP_NET_RAW=ep CAP_NET_BIND_SERVICE=ep" \
		$(DESTDIR)$(PREFIX)/bin/linbpq
	install -d $(DESTDIR)$(DATADIR)
	ln -s $(PREFIX)/bin/linbpq $(DESTDIR)$(DATADIR)/linbpq
	( cd $(SOURCES)/bpqhome && tar cf - . ) \
		| ( cd $(DESTDIR)$(DATADIR) && tar xf - )

clean:
	rm -f linbpq $(addprefix $(OBJECTS)/,$(OBJS))

realclean: clean
	rm -f $(addprefix $(OBJECTS)/,$(OBJS:.o=.d))

# Package up this current release as a tarball for download
release: DESTDIR ?= $(PWD)
release: $(DESTDIR)/linbpq-$(VERSION).tar.xz

$(DESTDIR)/linbpq-$(VERSION).tar.xz: .git
	git archive --format=tar \
		--prefix=linbpq-$(VERSION) \
		-o $(DESTDIR)/linbpq-$(VERSION).tar \
		HEAD
	xz -9 $(DESTDIR)/linbpq-$(VERSION).tar

# The following targets do not produce files
.PHONY: clean realclean all install linbpq release

-include $(wildcard $(OBJECTS)/*.d)
