#	%W% (Berkeley) %G%

PROG=	sh
SRCS=	builtins.c cd.c dirent.c echo.c error.c eval.c exec.c expand.c \
	input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c nodes.c options.c parser.c redir.c show.c signames.c \
	syntax.c trap.c output.c var.c
OBJS+=	init.o
#LDONLY=ldonly
#SRCHOME=/home/share/386bsd
CFLAGS+=-DSHELL -I. -I${.CURDIR} #-nostdinc -nostdlib -nodefaultlibs -L${SRCHOME}/usr/lib -lc
#CFLAGS+=-I${SRCHOME}/usr/src/include -I${SRCHOME}/usr/src/kernel/include
#LDFLAGS+=--image-base 0x00000000 -e start
.PATH:	${.CURDIR}/bltin
.EXEC:	mkinit
CLEANFILES+=\
	builtins.c builtins.h init.c mkinit mknodes mksignames mksyntax \
	nodes.c nodes.h signames.c signames.h syntax.c syntax.h token.def

.depend parser.o: token.def
token.def: mktokens
	sh ${.CURDIR}/mktokens

builtins.h builtins.c: ${.CURDIR}/mkbuiltins ${.CURDIR}/builtins
	cd ${.CURDIR}; sh mkbuiltins

init.c: mkinit.c ${SRCS}
#	${CC} ${CFLAGS} ${.CURDIR}/mkinit.c -o mkinit
	${CC} -DSHELL -I. -I${.CURDIR} ${.CURDIR}/mkinit.c -o mkinit
	./mkinit '${CC} -c ${CFLAGS} init.c' ${.ALLSRC}

mkinit: ${.CURDIR}/mkinit.c
#	${CC} ${CFLAGS} ${.CURDIR}/mkinit.c -o $@
	${CC} -DSHELL -I. -I${.CURDIR} ${.CURDIR}/mkinit.c -o $@

nodes.c nodes.h: mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

mknodes: ${.CURDIR}/mknodes.c
#	${CC} ${CFLAGS} ${.CURDIR}/mknodes.c -o $@
	${CC} -DSHELL -I. -I${.CURDIR} ${.CURDIR}/mknodes.c -o $@

signames.c signames.h: mksignames
	./mksignames

mksignames: ${.CURDIR}/mksignames.c
#	${CC} ${CFLAGS} ${.CURDIR}/mksignames.c -o $@
	${CC} -DSHELL -I. -I${.CURDIR} ${.CURDIR}/mksignames.c -o $@

syntax.c syntax.h: mksyntax
	./mksyntax

mksyntax: ${.CURDIR}/mksyntax.c ${.CURDIR}/parser.h
#	${CC} ${CFLAGS} ${.CURDIR}/mksyntax.c -o $@
	${CC} -DSHELL -I. -I${.CURDIR} ${.CURDIR}/mksyntax.c -o $@

.include <bsd.prog.mk>
