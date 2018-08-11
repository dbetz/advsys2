COMOBJS = \
adv2com.o \
adv2parse.o \
adv2scan.o \
adv2gen.o \
adv2exe.o \
adv2debug.o \
adv2vmdebug.o

COMHDRS = \
adv2compiler.h \
adv2image.h \
adv2symbols.h \
adv2types.h \
adv2vmdebug.h

INTOBJS = \
adv2int.o \
adv2exe.o \
adv2vmdebug.o

INTHDRS = \
adv2image.h \
adv2types.h \
adv2vmdebug.h

all:	adv2com adv2int

run:    adv2com test.adv
	./adv2com test.adv

$(COMOBJS):	$(COMHDRS)

$(INTOBJS):	$(INTHDRS)

CFLAGS = -Wall -Os -DMAC -m32

%.o:	%.c
	cc $(CFLAGS) -c -o $@ $<

adv2com:	$(COMOBJS)
	cc $(CFLAGS) -o $@ $(COMOBJS)

adv2int:	$(INTOBJS)
	cc $(CFLAGS) -o $@ $(INTOBJS)

clean:
	rm -f *.o adv2com adv2int
