/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: subr_log.c,v 1.4 94/07/07 21:07:39 bill Exp Locker: bill $
 */

/*
 * Error log buffer for kernel printf's.
 */
static char *log_config =
	"log 7.	 # kernel log device /dev/log $Revision: 1.4 $";

#include "sys/param.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/ioctl.h"

#include "uio.h"
#include "proc.h"
#include "msgbuf.h"
#include "signalvar.h"
#include "modconfig.h"

#include "vnode.h"	/* IO_NDELAY */

#include "prototypes.h"

#define LOG_RDPRI	(PWAIT + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	pid_t	sc_selp;		/* process waiting on select call */
	int	sc_pgid;		/* process/group for async I/O */
} logsoftc;

int	log_open;			/* also used in log() */

int
logopen(dev_t dev, int flags, int mode, struct proc *p)
{
	register struct msgbuf *mbp = msgbufp;

	if (log_open)
		return (EBUSY);
	log_open = 1;
	logsoftc.sc_pgid = p->p_pid;		/* signal process only */
	/*
	 * Potential race here with putchar() but since putchar should be
	 * called by autoconf, msg_magic should be initialized by the time
	 * we get here.
	 */
	if (mbp->msg_magic != MSG_MAGIC) {
		register int i;

		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufx = mbp->msg_bufr = 0;
		for (i=0; i < MSG_BSIZE; i++)
			mbp->msg_bufc[i] = 0;
	}
	return (0);
}

int
logclose(dev_t dev, int flags, int mode, struct proc *p)
{
	log_open = 0;
	logsoftc.sc_state = 0;
	logsoftc.sc_selp = 0;

	return 0;
}

int
logread(dev_t dev, struct uio *uio, int flag)
{
	register struct msgbuf *mbp = msgbufp;
	register long l;
	register int s;
	int error = 0;

	s = splhigh();
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		error = tsleep((caddr_t)mbp, LOG_RDPRI | PCATCH, "klog", 0);
		if (error != 0) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = MSG_BSIZE - mbp->msg_bufr;
		l = min(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomove((caddr_t)&mbp->msg_bufc[mbp->msg_bufr],
			(int)l, uio);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= MSG_BSIZE)
			mbp->msg_bufr = 0;
	}
	return (error);
}

int
logselect(dev_t dev, int rw, struct proc *p)
{
	int s = splhigh();

	switch (rw) {

	case FREAD:
		if (msgbufp->msg_bufr != msgbufp->msg_bufx) {
			splx(s);
			return (1);
		}
		logsoftc.sc_selp = p->p_pid;
		break;
	}
	splx(s);
	return (0);
}

void logwakeup()
{
	struct proc *p;

	if (!log_open)
		return;
	if (logsoftc.sc_selp) {
		selwakeup(logsoftc.sc_selp, 0);
		logsoftc.sc_selp = 0;
	}
	if (logsoftc.sc_state & LOG_ASYNC) {
#ifdef nope
		if (logsoftc.sc_pgid < 0)
			gsignal(-logsoftc.sc_pgid, SIGIO); 
		else if (p = pfind(logsoftc.sc_pgid))
			psignal(p, SIGIO);
#else
		signalpid(logsoftc.sc_pgid);
#endif
	}
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup((caddr_t)msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

int
logioctl(dev_t dev, int com, caddr_t data, int flag, struct proc *p)
{
	long l;
	int s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		splx(s);
		if (l < 0)
			l += MSG_BSIZE;
		*(off_t *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
		logsoftc.sc_pgid = *(int *)data;
		break;

	case TIOCGPGRP:
		*(int *)data = logsoftc.sc_pgid;
		break;

	default:
		return (-1);
	}
	return (0);
}

static struct devif log_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	logopen, logclose, logioctl, logread, 0, logselect, 0,
	0, 0, 0, 0,
};

DRIVER_MODCONFIG(log) {
	char *cfg_string = log_config;
	
	/* configure device */
	if (devif_config(&cfg_string, &log_devif) == 0)
		return;
}
