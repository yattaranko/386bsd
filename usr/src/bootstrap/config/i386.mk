# $Id: i386.mk,v 1.1 94/10/20 16:44:13 root Exp $
.PATH: $B/config

.include "$S/wd/Makefile.inc"
.include "$S/fd/Makefile.inc"
.include "$S/as/Makefile.inc"
.include "$S/ufs/Makefile.inc"
.include "$S/pccons/Makefile.inc"

DESTDIR=$(.CURDIR)
LD=/usr/bin/ld
STAND=	$B/boot
INCPATH=-I$B/../include -I$S/include -I$S/include/ic -I$S/include/fs -I$S/include/dev -I${STAND}
VPATH=	${STAND}
STANDDIR= ${DESTDIR}/usr/mdec
DEBUG=
#DEBUG= -DDEBUG=5
LIBC?=$B/../../lib/libc.a

AS=	as
CC=	cc ${INCPATH} -DSTANDALONE -DAT386
CPP=	cpp ${INCPATH} -DSTANDALONE -DAT386

RELOC=	98000
RELOC2=	98200

CFLAGS=	-DSTANDALONE -DRELOC=0x${RELOC} -Os ${INCPATH} ${DEBUG} \
		-fgnu89-inline -fcommon -fno-builtin -nostdinc -nostdlib

DRIVERS= \
	$S/pccons/pccons_cga.c $S/fd/fdboot.c $S/pccons/pccons_kbd.c $S/wd/wdboot.c $S/as/asboot.c
SRCS=	$B/boot/boot.c $S/fd/fdbootblk.c $B/boot/prf.c \
	$B/config/srt0_$(MACHINE).c $S/wd/wdbootblk.c  $S/wd/wdbootblk1.c ${DRIVERS} ${SASRC}

ALL= wdboot bootwd fdboot3 fdboot5 bootfd asboot bootas

all: ${ALL}

# startups

srt0_$(MACHINE).o: srt0_$(MACHINE).c
	${CC} -E -DLOCORE -DRELOC=0x${RELOC} \
	     $B/config/srt0_$(MACHINE).c |  ${AS} -o $@

wsrt0_$(MACHINE).o: srt0_$(MACHINE).c # srt0_$(MACHINE).c
	${CC} -E -DLOCORE -DSMALL -DRELOC=0x${RELOC} \
	     -DREL $B/config/srt0_$(MACHINE).c |  ${AS} -o $@

relsrt0_$(MACHINE).o: srt0_$(MACHINE).c
	${CC} -E -DLOCORE -DRELOC=0x${RELOC}
	     -DREL $B/config/srt0_$(MACHINE).c |  ${AS} -o $@

# block 0 boots

wdbootblk.o: wdbootblk.c 
	${CC} -E -DLOCORE -DRELOC=0x${RELOC} ${DEBUG} -nostdinc $< | ${AS} -o $@

wdbootblk1.o: wdbootblk1.c 
	${CC} -E -DLOCORE -DRELOC=0x${RELOC} ${DEBUG} -nostdinc $< | ${AS} -o $@

fdbootblk5.o: fdbootblk5.c 
	${CC} -E -DLOCORE -DRELOC=0x${RELOC} ${DEBUG} $< | ${AS} -o $@

fdbootblk3.o: fdbootblk3.c 
	${CC} -E -DLOCORE -DRELOC=0x${RELOC} -DF3 ${DEBUG} -nostdinc $< | ${AS} -o $@

# getting booted from disc

wdboot: wdbootblk1.o
#	${LD} -N -T ${RELOC} wdbootblk.o
	${LD} -N -Ttext 0x0 -nostdlib -entry=0 wdbootblk1.o
#	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@
	rm -f $@; objcopy -S -g -O binary a.out $@; rm -f a.out; ls -l $@

bootwd:	wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o ufsboot_fs.o pccons_kbd.o prf.o wdboot.o printf.o breadwd.o
#	${LD} -N -T ${RELOC2} wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o pccons_kbd.o prf.o printf.o \
		breadwd.o ufsboot_fs.o wdboot.o -lc
	${LD} -N -T bootwd.ld --image-base 0x${RELOC2} wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o pccons_kbd.o prf.o printf.o \
		breadwd.o ufsboot_fs.o wdboot.o ${LIBC}
	size a.out
#	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@
	rm -f $@; objcopy -S -g -O binary a.out $@; rm -f a.out; ls -l $@

fdboot5: fdbootblk5.o
	${LD} -N -T ${RELOC} fdbootblk5.o
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

fdboot3: fdbootblk3.o
#	${LD} -N -T ${RELOC} fdbootblk3.o
	# .textを0x7c00にするとリロケート(32bitに移行)してからのアドレスがずれるので、
	# 0x0000に設定してリロケート前(16bit)のソースをBIOSRELOCを使って調整する必要あり。
	${LD} -N -Ttext 0x0 -nostdlib -entry=0 fdbootblk3.o
	cp a.out fdb
#	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@
	rm -f $@; objcopy -S -g -O binary a.out $@; rm -f a.out; ls -l $@

bootfd:	wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o ufsboot_fs.o pccons_kbd.o prf.o fdboot.o printf.o breadfd.o
#	${LD} -N -T ${RELOC2} wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o pccons_kbd.o prf.o printf.o \
		breadfd.o ufsboot_fs.o fdboot.o -lc
	${LD} -N -T bootfd.ld --image-base 0x${RELOC2} wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o pccons_kbd.o prf.o printf.o \
		breadfd.o ufsboot_fs.o fdboot.o ${LIBC}
	cp a.out bfd
	size a.out
#	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@
	rm -f $@; objcopy -S -g -O binary a.out $@; rm -f a.out; ls -l $@

asboot:	asbootblk.o
	${LD} -N -T 7c00 asbootblk.o
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@

bootas:	wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o ufsboot_fs.o pccons_kbd.o prf.o asboot.o printf.o breadas.o
	${LD} -N -T ${RELOC2} wsrt0_$(MACHINE).o boot.o ufsboot_bmap.o pccons_cga.o pccons_kbd.o prf.o printf.o \
		breadas.o ufsboot_fs.o asboot.o -lc
	size a.out
	rm -f $@; strip a.out; trimhd 32 <a.out >$@; rm -f a.out; ls -l $@


breadwd.o: breadwd.c breadxx.o
breadfd.o: breadfd.c breadxx.o
breadas.o: breadas.c breadxx.o

breadxx.o:
	touch breadxx.o

breadwd.c: breadxx.c
	rm -f breadwd.c
	sed -e 's/XX/wd/' -e 's/xx/wd/g'	< $B/config/breadxx.c >> breadwd.c

breadfd.c: breadxx.c
	rm -f breadfd.c
	sed -e 's/XX/fd/' -e 's/xx/fd/g'	< $B/config/breadxx.c >> breadfd.c

breadas.c: breadxx.c
	rm -f breadas.c
	sed -e 's/XX/as/' -e 's/xx/as/g'	< $B/config/breadxx.c >> breadas.c

clean:
	rm -f *.o *.exe *.i sm_*.c
	rm -f a.out bfd bwd fdb wdb ${ALL}
	rm -f boot[a-wyz]? boot[a-wyz]?? boot[a-wyz]?.c boot[a-wyz]??.c \
		conf[a-wyz]?.c conf[a-wyz]??.c bread[a-wyz]?.c

cleandir: clean
	rm -f ${MAN} tags .depend

depend: ${SRCS}
	mkdep ${INCPATH} -DSTANDALONE ${SRCS} ${DUMMIES}

install: ${ALL}
	cp ${ALL} ${STANDDIR}
