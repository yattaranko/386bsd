
PROG=	init
NONSTDINC= ${INCOLDBSD}
SRCS=	init.c 

CFLAGS+= -target i386-pc-freebsd-elf32
LDADD= -lc -lutil
NOMAN=noman

.include <bsd.prog.mk>
