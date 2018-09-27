CFLAGS=-Wall -I$(HDRDIR) -m32

ifeq ($(OS),linux)
CFLAGS += -DLINUX
EXT=
OSINT=osint_linux
endif

ifeq ($(OS),macosx)
CFLAGS += -DMACOSX
EXT=
OSINT=osint_linux
endif

SRCDIR=.
HDRDIR=.
SPINDIR=spin
TOOLSDIR=tools
OBJDIR=obj
BINDIR=bin

CC=gcc
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
$(OBJDIR)/adv2vmdebug.o

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
$(OBJDIR)/propbinary.o \
$(OBJDIR)/advsys2_run_template.o \
$(OBJDIR)/advsys2_step_template.o \
$(OBJDIR)/wordfire_template.o

all:	$(DIRS) bin2c adv2com adv2int propbinary

game:    adv2int game.dat
	./adv2int game.dat

gamed:    adv2int game.dat
	./adv2int -d game.dat

%.dat:	%.adv game.adi adv2com
	./adv2com $<
	
$(COMOBJS):	$(COMHDRS)

$(INTOBJS):	$(INTHDRS)

$(OBJDIR)/%.o:	$(SRCDIR)/%.c $(HDRS)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(ECHO) $@

$(OBJDIR)/%.binary:	$(SPINDIR)/%.spin
	@$(SPINCMP) -o $@ $<
	@$(ECHO) $@

$(OBJDIR)/%.c:	$(OBJDIR)/%.binary
	@$(BINDIR)/bin2c$(EXT) $< $@
	@$(ECHO) $@

adv2com$(EXT):	$(COMOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS)  $(CFLAGS) -o $@ $(COMOBJS)
	@$(ECHO) $@

adv2int$(EXT):	$(INTOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS)  $(CFLAGS) -o $@ $(INTOBJS)
	@$(ECHO) $@

propbinary$(EXT):	$(PROPBINARYOBJS)
	@$(CC) $(CFLAGS) $(LDFLAGS)  $(CFLAGS) -o $@ $(PROPBINARYOBJS)
	@$(ECHO) $@

.PHONY:	bin2c
bin2c:		$(BINDIR)/bin2c$(EXT)

$(BINDIR)/bin2c$(EXT):	$(OBJDIR) $(TOOLSDIR)/bin2c.c
	@$(CC) $(CFLAGS) $(LDFLAGS) $(TOOLSDIR)/bin2c.c -o $@
	@$(ECHO) $@

$(DIRS):
	$(MKDIR) $@

clean:
	rm -r -f obj bin adv2com adv2int propbinary *.dat *.binary
