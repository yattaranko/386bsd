#	@(#)Makefile	5.1 (Berkeley) 6/5/90

NONSTDINC=$(INCOLDBSD)
LIB=resolv
CFLAGS+=-DDEBUG -DLIBC_SCCS
NONSTDINC=-I${SRCHOME}/usr/src/kernel/include -I${SRCHOME}/usr/src/kernel/include/sys -I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/include/nonstd -I${SRCHOME}/usr/src/include/nonstd/bsd

.PATH:	${.CURDIR}/../libc/net

SRCS=gethostnamadr.c res_mkquery.c res_query.c res_send.c

.include <bsd.lib.mk>
