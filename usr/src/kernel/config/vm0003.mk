#
# 386BSD operating system kernel for final release
#

.if ${MACHINE} != "i386"
MACHINE=	i386
.endif

KERNEL=	386bsd
#IDENT+= -Di486 -DTCP_COMPAT_42 -DINET ← INETはkern/inet/Makefile.incで追加されている
IDENT+= -Di486

LD=		/usr/local/bin/i386-unknown-freebsd14.3-ld
CC=		/usr/local/bin/i386-unknown-freebsd14.3-gcc14
CPP=	/usr/local/bin/i386-unknown-freebsd14.3-g++14
AS=		/usr/local/bin/i386-unknown-freebsd14.3-as

# standard kernel with INTERNET protocols
.include "$S/config/config.std.mk"
.include "$S/config/config.inet.mk"

# additional options
#.include "$S/fpu-emu/Makefile.inc"
#.include "$S/as/Makefile.inc"				# Adaptec 1540 SCSI
#.include "$S/mcd/Makefile.inc"				# Mitsumi CDROM
.include "$S/ed/Makefile.inc"				# NS/WD/SMC/3COM ethernet
#.include "$S/ddb/Makefile.inc"				# kernel debugger
#.include "$S/kern/opt/ktrace/Makefile.inc"	# BSD ktrace mechanism
.include "$S/isofs/Makefile.inc"			# ISO-9660 CDROM filesystem
#.include "$S/mfs/Makefile.inc"				# BSD memory-based filesystem
#.include "$S/dosfs/Makefile.inc"			# MS DOS FAT filesystem

.include "$S/config/kernel.mk"
