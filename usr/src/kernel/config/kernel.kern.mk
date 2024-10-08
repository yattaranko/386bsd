KMODULE=kern
KERNEL_C?=	${CC} -c ${CFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}
KERNEL_DBGC?=	${CC} -c ${DBGCFLAGS} ${PROF} ${DEBUG} ${.IMPSRC} -o ${.TARGET}
KERNEL_C_C?=	${CC} -c ${CFLAGS} ${PROF} ${PARAM} ${DEBUG} ${.IMPSRC} \
			-o ${.TARGET}
MACH_C?= 	${CC} -c -I$S/kern/${MACHINE} ${CFLAGS} ${PROF} ${DEBUG} \
			${.IMPSRC} -o ${.TARGET}
MACH_DBGC?= 	${CC} -c -I$S/kern/${MACHINE} ${DBGCFLAGS} ${PROF} ${DEBUG} \
			${.IMPSRC} -o ${.TARGET}
MACH_AS?= 	${CPP} -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} ${.IMPSRC} | \
		${AS} ${ASFLAGS} -o ${.TARGET}

.SUFFIXES: .${KMODULE}o .${KMODULE}co .${KMODULE}mo .${KMODULE}do .${KMODULE}mdo

OBJS+=  ${KERN_SRCS:R:S/$/.${KMODULE}o/g}
OBJS+=  ${KERN_SRCS_DBGC:R:S/$/.${KMODULE}do/g}
OBJS+=  ${KERN_SRCS_C:R:S/$/.${KMODULE}co/g}
OBJS+=  ${MACH_SRCS:R:S/$/.${KMODULE}mo/g}
OBJS+=  ${MACH_SRCS_DBGC:R:S/$/.${KMODULE}mdo/g}
OBJS+=  ${MACH_SRCS_S:R:S/$/.${KMODULE}mo/g}

.c.${KMODULE}o:
	${KERNEL_C}

.c.${KMODULE}co:
	${KERNEL_C_C}

.c.${KMODULE}do:
	${KERNEL_DBGC}

.c.${KMODULE}mo:
	${MACH_C}

.c.${KMODULE}mdo:
	${MACH_DBGC}

memmove_${MACHINE}.${KMODULE}mo:
	${CC} -E -traditional -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} $S/kern/${MACHINE}/memmove_${MACHINE}.S | \
	${AS} ${ASFLAGS} -o ${.TARGET}

scanc_${MACHINE}.${KMODULE}mo:
	${CC} -E -traditional -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} $S/kern/${MACHINE}/scanc_${MACHINE}.S | \
	${AS} ${ASFLAGS} -o ${.TARGET}

skpc_${MACHINE}.${KMODULE}mo:
	${CC} -E -traditional -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} $S/kern/${MACHINE}/skpc_${MACHINE}.S | \
	${AS} ${ASFLAGS} -o ${.TARGET}

locc_${MACHINE}.${KMODULE}mo:
	${CC} -E -traditional -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} $S/kern/${MACHINE}/locc_${MACHINE}.S | \
	${AS} ${ASFLAGS} -o ${.TARGET}

addupc_${MACHINE}.${KMODULE}mo:
	${CC} -E -traditional -I. -I$S/kern/${MACHINE} -DLOCORE ${COPTS} $S/kern/${MACHINE}/addupc_${MACHINE}.S | \
	${AS} ${ASFLAGS} -o ${.TARGET}

#.s.${KMODULE}mo:
#	${MACH_AS}

DEPEND+= depend_kern depend_mach

depend_kern: ${KERN_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.kerno:;" < /tmp/dep >depend_kern
	rm -rf /tmp/dep

depend_mach: ${MACH_SRCS}
	mkdep ${COPTS} ${.ALLSRC}
	mv .depend /tmp/dep
	sed -e "s;.o :;.kerno:;" < /tmp/dep >depend_mach
	rm -rf /tmp/dep

