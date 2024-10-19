#	@(#)Makefile	5.4 (Berkeley) 6/5/91

PROG=	stty
SRCS=	cchar.c gfmt.c key.c modes.c print.c stty.c util.c

LDONLY=ldonly

SRCHOME=/home/share/386bsd-2.0
CFLAGS+=-nostdinc -nostdlib -nodefaultlibs -fgnu89-inline -fcommon -fno-builtin
CFLAGS+=-I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/kernel/include
LDADD=	-L${SRCHOME}/usr/lib
LDFLAGS+=--image-base 0x00000000 -e start

.include <bsd.prog.mk>
