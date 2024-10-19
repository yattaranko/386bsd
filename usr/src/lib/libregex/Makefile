LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman
NONSTDINC=-I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/kernel/include

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}${SRCHOME}/usr/include/nonstd/gnu

.include <bsd.lib.mk>
