CFLAGS=-Wall -I$(HDRDIR)
# -m32

ifeq ($(CROSS),)
  PREFIX=
else
  ifeq ($(CROSS),win32)
	PREFIX=i686-w64-mingw32-
    OS=msys
  else
    ifeq ($(CROSS),rpi)
      PREFIX=arm-linux-gnueabihf-
      OS=raspberrypi
    else
      $(error Unknown cross compilation selected)
    endif
  endif
endif

ifeq ($(OS),Windows_NT)
OS=msys
endif

ifeq ($(OS),linux)
CFLAGS+=-DLINUX
EXT=

else ifeq ($(OS),raspberrypi)
CFLAGS+=-DLINUX -DRASPBERRY_PI
EXT=

else ifeq ($(OS),msys)
CFLAGS+=-DMINGW
LDFLAGS=-static
EXT=.exe

else ifeq ($(OS),macosx)
CFLAGS+=-DMACOSX
EXT=

else ifeq ($(OS),)
$(error OS not set)

else
$(error Unknown OS $(OS))
endif

BUILD=$(realpath ..)/advsys2-$(OS)-build
$(info BUILD $(BUILD))


SRCDIR=src
HDRDIR=src
SPINDIR=spin
TOOLSDIR=tools
OBJDIR=$(BUILD)/obj
BINDIR=$(BUILD)/bin

CC=$(PREFIX)gcc
TOOLCC=gcc
ECHO=echo
MKDIR=mkdir -p
SPINCMP=openspin

DIRS = $(OBJDIR) $(BINDIR)

COMOBJS = \
$(OBJDIR)/adv2com.o \
$(OBJDIR)/adv2parse.o \
$(OBJDIR)/adv2pasm.o \
$(OBJDIR)/adv2scan.o \
$(OBJDIR)/adv2gen.o \
$(OBJDIR)/adv2debug.o \
$(OBJDIR)/adv2vmdebug.o \
$(OBJDIR)/propbinary.o \
$(OBJDIR)/advsys2_run_template.o \
$(OBJDIR)/advsys2_step_template.o \
$(OBJDIR)/wordfire_template.o

COMHDRS = \
$(HDRDIR)/adv2compiler.h \
$(HDRDIR)/adv2image.h \
$(HDRDIR)/adv2types.h \
$(HDRDIR)/adv2vmdebug.h

INTOBJS = \
$(OBJDIR)/adv2int.o \
$(OBJDIR)/adv2exe.o \
$(OBJDIR)/adv2vmdebug.o

INTHDRS = \
$(HDRDIR)/adv2image.h \
$(HDRDIR)/adv2types.h \
$(HDRDIR)/adv2vmdebug.h

PROPBINARYOBJS = \
$(OBJDIR)/propbinaryapp.o \
$(OBJDIR)/propbinary.o \
$(OBJDIR)/advsys2_run_template.o \
$(OBJDIR)/advsys2_step_template.o \
$(OBJDIR)/wordfire_template.o

all:	$(DIRS) bin2c adv2com propbinary
# adv2int 

game:    adv2int game.dat
	$(BINDIR)/adv2int game.dat

gamed:    adv2int game.dat
	$(BINDIR)/adv2int -d game.dat

%.dat:	%.adv game.adi adv2com
	$(BINDIR)/adv2com $<
	
$(COMOBJS):	$(COMHDRS)

$(INTOBJS):	$(INTHDRS)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(HDRS)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $@

$(OBJDIR)/%.o:	$(TOOLSDIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $@

$(OBJDIR)/%.binary:	$(SPINDIR)/%.spin
	@$(SPINCMP) -o $@ $<
	@$(ECHO) $@

$(OBJDIR)/%.c:	$(OBJDIR)/%.binary
	@$(BINDIR)/bin2c$(EXT) $< $@
	@$(ECHO) $@

.PHONY:	adv2com
adv2com:		$(BINDIR)/adv2com$(EXT)

$(BINDIR)/adv2com$(EXT):	$(COMOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMOBJS)
	@$(ECHO) $@

.PHONY:	adv2int
adv2int:		$(BINDIR)/adv2int$(EXT)

$(BINDIR)/adv2int$(EXT):	$(INTOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(INTOBJS)
	@$(ECHO) $@

.PHONY:	propbinary
propbinary:		$(BINDIR)/propbinary$(EXT)

$(BINDIR)/propbinary$(EXT):	$(PROPBINARYOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(PROPBINARYOBJS)
	@$(ECHO) $@

.PHONY:	bin2c
bin2c:		$(BINDIR)/bin2c$(EXT)

$(BINDIR)/bin2c$(EXT):	$(TOOLSDIR)/bin2c.c
	$(TOOLCC) $(CFLAGS) $(LDFLAGS) $(TOOLSDIR)/bin2c.c -o $@
	@$(ECHO) $@

$(DIRS):
	$(MKDIR) $@

clean:
	rm -r -f $(BUILD) *.dat *.binary
