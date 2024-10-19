#	@(#)Makefile	5.3 (Berkeley) 12/4/90

PROG=	date
SRCS=	date.c netdate.c

LDONLY=ldonly
LIBCRT0=${SRCHOME}/usr/lib/crt0.o

SRCHOME=/home/share/386bsd-2.0
CFLAGS+=-nostdinc -nostdlib #-nodefaultlibs -fgnu89-inline -fcommon -fno-builtin
CFLAGS+=-I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/kernel/include
LDFLAGS+=--image-base 0x00000000 -e start
NONSTDINC+= ${INCOLDBSD}
DPADD=	${LIBUTIL}
LDADD=	-L${SRCHOME}/usr/lib -lutil

.include <bsd.prog.mk>
