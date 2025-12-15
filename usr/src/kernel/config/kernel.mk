#
# This make file incorporates the generic 386BSD kernel program makefile,
# kernel.mk. Copyright (C) 1990-1994 W. Jolitz, All Rights Reserved.
# $Id:$
#

.SUFFIXES: .symbols .9 .8 .7 .6 .5 .4 .3 .2 .1 .0

TOUCH?=	touch -f -c
LD?=	/usr/local/bin/i386-unknown-freebsd14.3-ld
CC?=	/usr/local/bin/i386-unknown-freebsd14.3-gcc14
CPP?=	/usr/local/bin/i386-unknown-freebsd14.3-g++14
AS?=	/usr/local/bin/i386-unknown-freebsd14.3-as

# XXX overkill, revise include scheme
INCLUDES= -I$S/include -I. -I$S/../include

CROSS+=	-nostdinc -fno-builtin -fleading-underscore \
		-ffreestanding -fno-exceptions -fno-pie -fno-omit-frame-pointer -fvisibility=hidden
#COPTS+=	${INCLUDES} ${IDENT} -DKERNEL -Di386
COPTS+=	${INCLUDES} ${IDENT} -DKERNEL
DEPEND= depend_mk

ASFLAGS= ${DEBUG}
.if defined(GDB)
# CFLAGS=	-m486 -O ${COPTS} -g
CFLAGS=	-O ${COPTS} ${CROSS} -g
.else
# CFLAGS=	-m486 -O ${COPTS}
CFLAGS=	-O ${COPTS} ${CROSS}
.endif
DBGCFLAGS= -O ${COPTS} ${CROSS}

.if defined(KERNBASE)
CFLAGS+= -DKERNBASE=0x${KERNBASE}
BASE= -DKERNBASE=0x${KERNBASE}
.else
KERNBASE=FE000000
.endif

.if !exists(machine)
FOO!=ln -s $S/include/$(MACHINE) machine
.endif

.MAIN:	all

ALLMAN=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8} ${MAN9}

KOBJS = ${FIRSTOBJ} ${OBJS}

_KERNS=		${KERNEL}
.if defined(DDB)
_KERNS+=	${KERNEL}.ddb
.endif
.if defined(PROFILE)
_KERNS+=	${KERNEL}.prof
.endif
.if defined(KGDB)
_KERNS+=	${KERNEL}.kgdb
.endif

all: ${_KERNS}	${ALLMAN}

assym.S: $S/include/sys/param.h $S/include/buf.h $S/include/vmmeter.h \
	$S/include/proc.h $S/include/msgbuf.h machine/vmparam.h \
	$S/config/genassym.c
#	${CC} ${INCLUDES} -DKERNEL ${IDENT} ${PARAM} ${BASE} \
		 $S/config/genassym.c -o genassym
	/usr/bin/clang -m32 ${INCLUDES} ${IDENT} ${PARAM} ${BASE} \
		 $S/config/genassym.c -o genassym
	./genassym >assym.S

isym.o: $S/config/isym.c
	find $S/include -name "*.h" -a -type f -exec grep -h "^__ISYM__" {} \; > isym
	cp $S/config/isym.c isym.c
	${CC} -c -DKERNEL ${IDENT} ${PARAM} ${BASE} ${CROSS} isym.c -o isym.o
	rm isym isym.c

.include "$S/config/kernel.kern.mk"
.include "$S/config/kernel.dev.mk"
.include "$S/config/kernel.fs.mk"
.include "$S/config/kernel.domain.mk"

SRCS= ${KERN_SRCS} ${KERN_SRCS_DBGC} ${MACH_SRCS} ${DEV_SRCS} ${FS_SRCS} ${DOMAIN_SRCS}

${KERNEL}: Makefile symbols.sort ${FIRSTOBJ} ${OBJS} isym.o
	@echo loading $@
	@rm -f $@
	@$S/config/newvers.sh
	@${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${CROSS} vers.c
.if defined(DEBUGSYM)
#	@${LD} -z -T ${KERNBASE} -o $@ -X ${FIRSTOBJ} ${OBJS} vers.o isym.o
	@${LD} -N --image-base ${KERNBASE} -o $@ -X ${FIRSTOBJ} ${OBJS} vers.o isym.o \
	-nostdlib
.else
#	@${LD} -z -T ${KERNBASE} -o $@ -x ${FIRSTOBJ} ${OBJS} vers.o isym.o
	@${LD} -N --image-base ${KERNBASE} -o $@ -x ${FIRSTOBJ} ${OBJS} vers.o isym.o \
	-nostdlib
.endif
	@echo rearranging symbols
.if defined(GDB)
	cp $@ $@.gdb
	strip -d $@
.endif
#	@symorder ${SYMORDER} symbols.sort $@
.if defined(DBSYM)
	@${DBSYM} $@
.endif
	@size $@
	@chmod 755 $@

.9.0 .8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -mandoc ${.IMPSRC} > ${.TARGET}

clean:
	rm -f eddep 386bsd* tags ${KOBJS} errs linterrs makelinks vers.o isym.o \
	locore.o assym.S genassym .depend ${DEPEND} ${.TARGET}
	unlink machine

depend: ${DEPEND}
	cat ${DEPEND} >> .depend

depend_mk: assym.S
	mkdep ${COPTS} ${.ALLSRC}
	mkdep -a -p ${INCLUDES} ${IDENT} ${PARAM} $S/config/genassym.c
	mv .depend depend_mk

symbols.sort: ${SYMBOLS}
	grep -hv '^#' ${.ALLSRC} | sed 's/^	//' | sort -u > symbols.sort

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall: beforeinstall

install: afterinstall
afterinstall: realinstall maninstall
.endif

.if !target(tags)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:M*.c} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif

.include <bsd.man.mk>
