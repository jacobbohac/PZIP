# 2004-04-25-CrT: Created this from ppmz.mak, which strikes
# me as a weird nonstandard name for a makefile.
#----- makefile for pzip -----
#
# Simple UNIX makefile, tweak as necessary.
#
SHELL		= /bin/sh
CC		= gcc
PRODUCTIONCFLAGS= -O3 -Wall -Wno-parentheses -Wno-comment -finline-functions
DEVELOPMENTFLAGS= -O -Wall -Wno-parentheses -Wno-comment -g -pg -fprofile-arcs
CFLAGS		= $(DEVELOPMENTFLAGS)
VERSION		= 0.82

INCLUDES	= 

OBJS		= arithc.o config.o context.o crc32.o deterministic.o det_escape.o \
		  excluded_symbols.o intmath.o main.o node.o order-1.o pool.o \
		  pzip.o safe.o see.o

LIBS		= -lm

all:	pzip

.c.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $<

$(OBJS):	 version.h

pzip: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LIBS)

clean:
	@rm -f *.o *.da *~ ID TAGS core gmon.out pzip test.tmp test.pz \
		book1* book2* geo* news* obj1* obj2* \
		paper1*  paper2* paper3* paper4* paper5* paper6* \
		progl* progc* progp* bib* pic* trans*

version.h:	Makefile
	@echo "#define VERSION $(VERSION)" >version.h

# Simple test:
check:  pzip
	./pzip -e pzip.c test.pz
	./pzip test.pz test.tmp
	cmp test.tmp pzip.c
	@if [ $$? -ne 0 ]; then echo "FAILED"; else echo "Success!"; fi

tarball: clean 
	@if [ -f ../pzip-$(VERSION).tar     ] ; then rm -f ../pzip-$(VERSION).tar    ; fi
	@if [ -f ../pzip-$(VERSION).tar.bz2 ] ; then rm -f ../pzip-$(VERSION).tar.bz2; fi
	(cd ..; tar -cf pzip-$(VERSION).tar pzip-$(VERSION); bzip2 -9  pzip-$(VERSION).tar; ls -l  pzip-$(VERSION).tar.bz2)

corpus:
	cp ../corpus/calgary/[a-hj-z]* .

etags:
	@etags *.[ch] OVERVIEW

id:
	@mkid *.[ch] OVERVIEW

profile:
	gprof ./pzip ./gmon.out

