# $Id: $

.if !defined(NOINCLUDE) && exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

SRCHOME?=	/home/share/386bsd-2.0
LIBDIR?=	${SRCHOME}/usr/lib
LINTLIBDIR?=	${SRCHOME}/usr/lib/lint
#LIBGRP?=	bin
LIBGRP?=	wheel
#LIBOWN?=	bin
LIBOWN?=	root
LIBMODE?=	444

STRIP?=	-s

#BINGRP?=	bin
BINGRP?=	wheel
#BINOWN?=	bin
BINOWN?=	root
BINMODE?=	555

INCARPA?=	-I${SRCHOME}/usr/include/nonstd/arpa
INCOLDBSD?=	-I${SRCHOME}/usr/include/nonstd/bsd
INCLASTBSD?=	-I${SRCHOME}/usr/include/nonstd/lastbsd
INCDB?=		-I${SRCHOME}/usr/include/nonstd/db
INCDEV?=	-I${SRCHOME}/usr/include/nonstd/dev
INCFS?=		-I${SRCHOME}/usr/include/nonstd/fs
INCGNU?=	-I${SRCHOME}/usr/include/nonstd/gnu
INCIC?=		-I${SRCHOME}/usr/include/nonstd/ic
INCKERNEL?=	-I${SRCHOME}/usr/include/nonstd/kernel
INCNET?=	-I${SRCHOME}/usr/include/nonstd/net
INCSUN?=	-I${SRCHOME}/usr/include/nonstd/sun
CFLAGS+= -nostdinc -nostdlib -r ${NONSTDINC}
.MAIN: all

# prefer .s to a .c, add .po, remove stuff not used in the BSD libraries
.SUFFIXES:
.SUFFIXES: .out .o .po .s .S .c .f .y .l .9 .8 .7 .6 .5 .4 .3 .2 .1 .0

.9.0 .8.0 .7.0 .6.0 .5.0 .4.0 .3.0 .2.0 .1.0:
	nroff -mandoc ${.IMPSRC} > ${.TARGET}

.c.o:
	${CC} ${CFLAGS} -c ${.IMPSRC} 
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.c.po:
#	${CC} -p ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	${CC} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

.s.o:
	${CPP} -E ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.S.o:
	${CC} -E ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -x -r ${.TARGET}
	@mv a.out ${.TARGET}

.s.po:
	${CPP} -E -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

.S.po:
	${CC} -E -DPROF ${CFLAGS:M-[ID]*} ${AINC} ${.IMPSRC} | \
	    ${AS} -o ${.TARGET}
	@${LD} -X -r ${.TARGET}
	@mv a.out ${.TARGET}

MANALL=	${MAN1} ${MAN2} ${MAN3} ${MAN4} ${MAN5} ${MAN6} ${MAN7} ${MAN8} ${MAN9}

.if !defined(NOPROFILE)
_LIBS=lib${LIB}.a lib${LIB}_p.a
.else
_LIBS=lib${LIB}.a
.endif

all: ${_LIBS} ${MANALL}# llib-l${LIB}.ln

OBJS+=	${SRCS:R:S/$/.o/g}

lib${LIB}.a:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.a
	@${AR} cTq lib${LIB}.a `lorder ${OBJS} | tsort` ${LDADD}
#	ranlib lib${LIB}.a

POBJS+=	${OBJS:.o=.po}
lib${LIB}_p.a:: ${POBJS}
	@echo building profiled ${LIB} library
	@rm -f lib${LIB}_p.a
	@${AR} cTq lib${LIB}_p.a `lorder ${POBJS} | tsort` ${LDADD}
#	ranlib lib${LIB}_p.a

llib-l${LIB}.ln: ${SRCS}
	${LINT} -C${LIB} ${CFLAGS} ${.ALLSRC:M*.c}

.if !target(clean)
clean:
	rm -f .depend a.out Errs errs mklog core ${CLEANFILES} ${OBJS} ${POBJS} \
	    profiled/*.o lib${LIB}.a lib${LIB}_p.a llib-l${LIB}.ln ${MANALL}
.endif

.if !target(cleandir)
cleandir:
	rm -f a.out Errs errs mklog core ${CLEANFILES} ${OBJS} ${POBJS} \
	    profiled/*.o lib${LIB}.a lib${LIB}_p.a llib-l${LIB}.ln
	rm -f ${MANALL} ${.CURDIR}/tags .depend
.endif

.if !target(depend)
depend: .depend
.depend: ${SRCS}
	mkdep ${CFLAGS:M-[ID]*} ${AINC} ${.ALLSRC}
	@(TMP=/tmp/_depend$$$$; \
	    sed -e 's/^\([^\.]*\).o:/\1.o \1.po:/' < .depend > $$TMP; \
	    mv $$TMP .depend)
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif

realinstall: beforeinstall
	ranlib lib${LIB}.a
	install -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} lib${LIB}.a \
	    ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}.a
	ranlib lib${LIB}_p.a
	install -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    lib${LIB}_p.a ${DESTDIR}${LIBDIR}
	${RANLIB} -t ${DESTDIR}${LIBDIR}/lib${LIB}_p.a
#	install -c -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
#	    llib-l${LIB}.ln ${DESTDIR}${LINTLIBDIR}
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

install: afterinstall
afterinstall: realinstall maninstall
.endif

.if !target(lint)
lint:
.endif

.if !target(tags)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:M*.c} | \
	    sed "s;\${.CURDIR}/;;" > tags
.endif

.include <bsd.man.mk>
.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:
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
