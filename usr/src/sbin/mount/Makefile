#	@(#)Makefile	5.5 (Berkeley) 5/11/90

NONSTDINC=$(INCOLDBSD)
PROG=	mount
CFLAGS+=-DNFS
MAN8=	mount.0
DPADD=	${LIBRPC}
LDADD=	-lrpc -lc
MLINKS=	mount.8 umount.8
SRCS=	mount.c

.include <bsd.prog.mk>
