#	@(#)Makefile	5.4 (Berkeley) 5/11/90

PROG=	umount
CFLAGS+=-DNFS
NONSTDINC= $(INCOLDBSD)
NOMAN=	noman
DPADD=	${LIBRPC}
LDADD=	-lrpc -lc
SRCS=	umount.c

.include <bsd.prog.mk>
