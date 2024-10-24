# $Id: $

.if !defined(NOINCLUDE) && exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

.SUFFIXES: .out .o .c .y .l .s .9 .8 .7 .6 .5 .4 .3 .2 .1 .0

.9.0 .8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -mandoc ${.IMPSRC} > ${.TARGET}

LDONLY=ldonly

SRCHOME?=		/home/share/386bsd
CFLAGS+=		${COPTS} ${NONSTDINC}
CFLAGS+=		-Os -nostdinc -nostdlib -nodefaultlibs
CFLAGS+=		-I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/kernel/include
LDFLAGS+=		--image-base 0x00000000 -e start

LDADD+=			-L${SRCHOME}/usr/lib -lutil

STRIP?=	-s

BINGRP?=		wheel
BINOWN?=		root
BINMODE?=		555

LIBCRT0?=		${SRCHOME}/usr/lib/crt0.o
LIBC?=          ${SRCHOME}/usr/lib/libc.a
# LIBCOMPAT?=	/usr/lib/libcompat.a
LIBCURSES?=		${SRCHOME}/usr/lib/libcurses.a
LIBDES?=		${SRCHOME}/usr/lib/libdes.a
LIBL?=			${SRCHOME}/usr/lib/libl.a
LIBLN?=			${SRCHOME}/usr/lib/libln.a
LIBKDB?=		${SRCHOME}/usr/lib/libkdb.a
LIBKRB?=		${SRCHOME}/usr/lib/libkrb.a
LIBM?=			${SRCHOME}/usr/lib/libm.a
LIBMP?=			${SRCHOME}/usr/lib/libmp.a
LIBPC?=			${SRCHOME}/usr/lib/libpc.a
LIBPLOT?=		${SRCHOME}/usr/lib/libplot.a
LIBRESOLV?=		${SRCHOME}/usr/lib/libresolv.a
LIBRPC?=		${SRCHOME}/usr/lib/librpc.a
LIBTERM?=		${SRCHOME}/usr/lib/libterm.a
LIBUTIL?=		${SRCHOME}/usr/lib/libutil.a

INCARPA?=		-I${SRCHOME}/usr/include/nonstd/arpa
INCOLDBSD?=		-I${SRCHOME}/usr/include/nonstd/bsd
INCLASTBSD?=	-I${SRCHOME}/usr/include/nonstd/lastbsd
INCDB?=			-I${SRCHOME}/usr/include/nonstd/db
INCDEV?=		-I${SRCHOME}/usr/include/nonstd/dev
INCFS?=			-I${SRCHOME}/usr/include/nonstd/fs
INCGNU?=		-I${SRCHOME}/usr/include/nonstd/gnu
INCIC?=			-I${SRCHOME}/usr/include/nonstd/ic
INCKERNEL?=		-I${SRCHOME}/usr/include/nonstd/kernel
INCNET?=		-I${SRCHOME}/usr/include/nonstd/net
INCSUN?=		-I${SRCHOME}/usr/include/nonstd/sun

.if defined(SHAREDSTRINGS)
CLEANFILES+=strings
.c.o:
	${CC} -E ${CFLAGS} ${.IMPSRC} | xstr -c -
	@${CC} ${CFLAGS} -c x.c -o ${.TARGET}
	@rm -f x.c
.endif

.if defined(PROG)
.if defined(SRCS)

OBJS+=  ${SRCS:R:S/$/.o/g}

.if defined(LDONLY)

${PROG}: ${LIBCRT0} ${LIBC} ${OBJS} ${DPADD} 
	${LD} ${LDFLAGS} -o ${.TARGET} ${LIBCRT0} ${OBJS} ${LIBC} ${LDADD}

.else # defined(LDONLY)

${PROG}: ${OBJS} ${LIBC} ${DPADD}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS} ${LDADD}

.endif

.else # defined(PROG)

SRCS= ${PROG}.c

${PROG}: ${SRCS} ${LIBC} ${DPADD}
	${CC} ${CFLAGS} -o ${.TARGET} ${.CURDIR}/${SRCS} ${LDADD}

MKDEP=	-p

.endif

.if	!defined(MAN1) && !defined(MAN2) && !defined(MAN3) && \
	!defined(MAN4) && !defined(MAN5) && !defined(MAN6) && \
	!defined(MAN7) && !defined(MAN8) && !defined(MAN9) && \
	!defined(NOMAN)
MAN1=	${PROG}.0
.endif
.endif
MANALL=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8} ${MAN9}

_PROGSUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(echo "===> $$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/}); \
	done
.endif

.MAIN: all
all: ${PROG} ${MANALL} _PROGSUBDIR

.if !target(clean)
clean: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog core ${PROG} ${OBJS} ${CLEANFILES} ${MANALL}
.endif

.if !target(cleandir)
cleandir: _PROGSUBDIR
	rm -f a.out [Ee]rrs mklog core ${PROG} ${OBJS} ${CLEANFILES}
	rm -f .depend ${MANALL}
.endif

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: .depend _PROGSUBDIR
.depend: ${SRCS}
.if defined(PROG)
	mkdep ${MKDEP} ${CFLAGS:M-[ID]*} ${.ALLSRC:M*.c}
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif

realinstall: _PROGSUBDIR
.if defined(PROG)
	install ${STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${PROG} ${DESTDIR}${BINDIR}
.endif
.if defined(HIDEGAME)
	(cd ${DESTDIR}/usr/games; rm -f ${PROG}; ln -s dm ${PROG}; \
	    chown games.bin ${PROG})
.endif
.if defined(LINKS) && !empty(LINKS)
	@set ${LINKS}; \
	while test $$# -ge 2; do \
		l=${DESTDIR}$$1; \
		shift; \
		t=${DESTDIR}$$1; \
		shift; \
		echo $$t -\> $$l; \
		rm -f $$t; \
		ln $$l $$t; \
	done; true
.endif

install: maninstall
maninstall: afterinstall
afterinstall: realinstall
realinstall: beforeinstall
.endif

.if !target(lint)
lint: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	@${LINT} ${LINTFLAGS} ${CFLAGS} ${.ALLSRC} | more 2>&1
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj: _PROGSUBDIR
.else
obj: _PROGSUBDIR
	@cd ${.CURDIR}; rm -rf obj; \
	here=`pwd`; dest=/usr/obj/`echo $$here | sed 's,/usr/src/,,'`; \
	echo "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

.if !target(tags)
tags: ${SRCS} _PROGSUBDIR
.if defined(PROG)
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif
.endif

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif
