ECHO=echo

SPINCMP=openspin
SPINDIR=../spin
TOOLSDIR=../tools

OBJS = \
adv2load.o \
db_config.o \
db_loader.o \
db_packet.o \
db_system.o \
PLoadLib.o \
osint_linux.o \
serial_helper.o \
advsys2_vm.o

HDRS = \
db_config.h \
db_image.h \
db_loader.h \
db_packet.h \
db_system.h \
PLoadLib.h \
osint.h

all:	adv2load

$(OBJS):	$(HDRS)

CFLAGS = -DMACOSX -Wall -Os -DMAC -m32

%.o:	%.c
	cc $(CFLAGS) -c -o $@ $<

%.dat:	$(SPINDIR)/%.spin
	@$(SPINCMP) -c -o $@ $<
	@$(ECHO) $@

%.c:	%.dat bin2c
	@./bin2c $< $@
	@$(ECHO) $@
	
bin2c:  $(TOOLSDIR)/bin2c.c
	cc $(CFLAGS) -o $@ $<

adv2load:	$(OBJS)
	cc $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f *.o *.binary *.dat advsys2_vm.c serial_helper.c adv2load bin2c
