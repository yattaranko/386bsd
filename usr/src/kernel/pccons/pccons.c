/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	$Id: pccons.c,v 1.5 94/07/27 16:45:03 bill Exp Locker: bill $
 */

/*
 * code to work keyboard & display for PC-style console
 */

/* standard ISA/AT configuration: */
static char *pc_config =
	"pccons	12 (0x60 1 -1 0xb8000 65536).	# console video $Revision: 1.5 $";
/* default console device */
static char *pc_console_config =
	"console default 0.";

#include "sys/param.h"
#include "sys/ioctl.h"
#include "proc.h"
#include "sys/file.h"
#include "sys/user.h"
#include "tty.h"
#include "uio.h"
#include "isa_driver.h"
#include "callout.h"
#include "systm.h"
#include "kernel.h"
#include "sys/syslog.h"
#include "isa_irq.h"
#include "isa_stdports.h"
#include "modconfig.h"
#include "machine/icu.h"
#include "i8042.h"
#include "isa_kbd.h"
#include "isa_display.h"

#include "prototypes.h"
#include "machine/inline/io.h"

int pc_xmode;
int _debug_mode_;
struct	tty pccons;

struct	pcconsoftc {
	char	cs_flags;
#define	CSF_ACTIVE	0x1	/* timeout active */
#define	CSF_POLLING	0x2	/* polling for input */
	char	cs_lastc;	/* last char sent */
	int	cs_timo;	/* timeouts since interrupt */
	u_long	cs_wedgecnt;	/* times restarted */
} pcconsoftc;

struct	kbdsoftc {
	char	kbd_flags;
#define	KBDF_ACTIVE	0x1	/* timeout active */
#define	KBDF_POLLING	0x2	/* polling for input */
#define	KBDF_RAW	0x4	/* pass thru scan codes for input */
	char	kbd_lastc;	/* last char sent */
} kbdsoftc;

static struct video_state {
	char	esc;	/* seen escape */
	char	ebrac;	/* seen escape bracket */
	char	eparm;	/* seen escape and parameters */
	char	so;	/* in standout mode? */
	int 	cx;	/* "x" parameter */
	int 	cy;	/* "y" parameter */
	int 	row, col;	/* current cursor position */
	int 	nrow, ncol;	/* current screen geometry */
	char	fg_at, bg_at;	/* normal attributes */
	char	so_at;	/* standout attribute */
	char	kern_fg_at, kern_bg_at;
	char	color;	/* color or mono display */
} vs;

int pcprobe(struct isa_device *dev);
void pcattach(struct isa_device *dev);
void pcrint(int dev);
static void pc_xmode_on();
static void pc_xmode_off();
static void sput(u_char c, u_char ka);

struct	isa_driver pcdriver = {
	pcprobe, pcattach, pcrint, "pc", &ttymask
};

/* block cursor so wfj does not go blind on laptop hunting for
	the verdamnt cursor -wfj */
/* #define	FAT_CURSOR */

#define	COL			80
#define	ROW			25
#define	CHR			2
#define MONO_BASE	0x3B4
#define MONO_BUF	0xfe0B0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xfe0B8000
#define IOPHYSMEM	0xA0000

static unsigned int addr_6845 = MONO_BASE;
u_short *Crtat = (u_short *)MONO_BUF;
static int openf;

char *sgetc(int);
static	char	*more_chars;
static	int	char_count;

/*
 * We check the console periodically to make sure
 * that it hasn't wedged.  Unfortunately, if an XOFF
 * is typed on the console, that can't be distinguished
 * from more catastrophic failure.
 */
#define	CN_TIMERVAL	(hz)		/* frequency at which to check cons */
#define	CN_TIMO		(2*60)		/* intervals to allow for output char */

static int pcstart();
static int pcparam();
static char partab[];

extern int pcopen(dev_t, int, int, struct proc *);
/*
 * Wait for CP to accept last CP command sent
 * before setting up next command.
 */
#define	waitforlast(timo) { \
	if (pclast) { \
		(timo) = 10000; \
		do \
			uncache((char *)&pclast->cp_unit); \
		while ((pclast->cp_unit&CPTAKE) == 0 && --(timo)); \
	} \
}

unsigned kbd_rd(), kbd_cmd_read_param();

/*
 * these are both bad jokes
 */
int pcprobe(struct isa_device *dev)
{
	unsigned c;
	int again = 0;

	if(dev->id_unit == 1)
	{
		int i;

		/*outb(0x2fa,0x55);
		outb(0x3fa,0xaa);
		outb(0x3fa,0x36);
		outb(0x3fa,0xe4);
		outb(0x2fa,0x1b);
		outb(0x2fa,0x36); */
		printf("\n[");
		for(i=0; i < 16; i++)
		{
			outb(0x390, i);
			printf("%x ", inb(0x391));
		}
		printf("]\n");
		outb(0x311, 0x88);
		DELAY(250);
		outb(0x311, 0x80);
		DELAY(2500);
		while((inb(0x311)&4)==0) ;
		outb(0x310, 0xf4);
		while((inb(0x311)&4)==0) ;
		outb(0x311, 0x90);
		return(1);
	}

	(void)kbd_drain();
	kbd_cmd(K_DISKEY);
	kbd_cmd(K_DISAUX);

	/* disable interrupts and keyboard controller */
	kbd_cmd_write_param(K_WRITE + K__CMDBYTE, DISABLE_CMDBYTE);

	/* are we a type 1 or type 2 ? */
	if ((kbd_cmd_read_param(K_READ + K__CMDBYTE) & KC8_TRANS) !=  KC8_TRANS)
		printf(" type 2 ");

	/* Start keyboard stuff RESET */
	if ((c = key_cmd(KBC_RESET)) != KBR_RSTDONE && c != KBR_ACK)
	{
		if ((c == KBR_RESEND) ||  (c == KBR_OVERRUN))
		{
			DELAY(400);
		}
	}
#ifdef nope

	/* pick up keyboard reset return code */
	if((c = kbd_rd()) != KBR_RSTDONE)
		printf("keyboard failed selftest (%x) \n", c);

kbd_drain();

#endif
	kbd_cmd(K_DISKEY);
	kbd_cmd(K_ENAAUX);

	/* if(aux_cmd(0xff) != KBR_ACK)
		printf("!"); */
	/* if((c = kbd_rd()) != KBR_RSTDONE)
		printf("aux failed selftest (%x) \n", c); */
	printf("9");
	if(aux_cmd(0xf4) != KBR_ACK);

	/* enable interrupts and keyboard controller */
	kbd_cmd_write_param(K_WRITE + K__CMDBYTE, ENABLE_CMDBYTE);

	/* enable interrupts and keyboard controller */
	kbd_cmd_write_param(K_WRITE + K__CMDBYTE, 0x47);

	printf (" kbd cmd %x %x ", kbd_cmd_read_param(K_READ + K__CMDBYTE),
		kbd_cmd_read_param(0xd0));
	kbd_cmd(K_DISAUX);

	return (1);
}

void
pcattach(struct isa_device *dev)
{
	u_short *cp = Crtat + (CGA_BUF-MONO_BUF)/CHR;
	u_short was;

printf("pc0 ");
	if (vs.color == 0)
		printf("<mono>");
	else	printf("<color>");
	cursor(0);
}

int
pcopen(dev_t dev, int flag, int mode, struct proc *p)
{
	register struct tty *tp;

	tp = &pccons;
	tp->t_oproc = pcstart;
	tp->t_stop = 0;
	tp->t_param = pcparam;
	tp->t_dev = dev;
	openf++;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	tp->t_state |= TS_CARR_ON;
	return (ldiscif_open(dev, tp, flag));
	/*return ((*linesw[tp->t_line].l_open)(dev, tp, flag));*/
}

int
pcclose(dev_t dev, int flag, int mode, struct proc *p)
{
struct tty *tp = &pccons;
/*extern int xxxcons;*/
	/*if (flag & O_NONBLOCK) {		/* XXX */
	ldiscif_close(&pccons, flag);
	/* (*linesw[pccons.t_line].l_close)(&pccons, flag);*/
	ttyclose(&pccons);
	tp->t_state |= TS_CARR_ON;	/* XXX: rude X hanging bug fix */
	/*}*/
	pc_xmode_off();
	return(0);
}

int
pcread(dev_t dev, struct uio *uio, int flag)
{
	return (ldiscif_read(&pccons, uio, flag));
	/*return ((*linesw[pccons.t_line].l_read)(&pccons, uio, flag));*/
}

int
pcwrite(dev_t dev, struct uio *uio, int flag)
{
	return (ldiscif_write(&pccons, uio, flag));
	/*return ((*linesw[pccons.t_line].l_write)(&pccons, uio, flag));*/
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 * Catch the character, and see who it goes to.
 */
void
pcrint(int dev)
{
	int c;
	char *cp;

	if(_debug_mode_)
	{
		(void)inb(KBSTATP);
		(void)inb(KBDATAP);
		printf("*");
		return;
	}
	cp = sgetc(1);
	if (cp == 0)
	{
		return;
	}
	if (pcconsoftc.cs_flags & CSF_POLLING)
	{
		return;
	}
#ifdef KDB
	if (kdbrintr(c, &pccons))
		return;
#endif
	if (!openf)
	{
		return;
	}
	do
	{
		ldiscif_rint(*cp++ & 0xff, &pccons);
		/*(*linesw[pccons.t_line].l_rint)(*cp++ & 0xff, &pccons);*/
	} while (*cp);
}

#define CONSOLE_X_MODE_ON _IO('t',121)
#define CONSOLE_X_MODE_OFF _IO('t',122)
#define CONSOLE_X_MAP _IOR('t',123,char *)

int
pcioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	register struct tty *tp = &pccons;
	register error;
 
	if (cmd == CONSOLE_X_MODE_ON)
	{
		pc_xmode_on ();
		return (0);
	}
	else if (cmd == CONSOLE_X_MODE_OFF)
	{
		pc_xmode_off ();
		return (0);
	}

	error = ldiscif_ioctl(tp, cmd, addr, flag, p);
	/*error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, addr, flag);*/
	if (error >= 0)
	{
		return (error);
	}
	error = ttioctl(tp, cmd, addr, flag, p);
	if (error >= 0)
	{
		return (error);
	}
	return (ENOTTY);
}

int	pcconsintr = 1;
/*
 * Got a console transmission interrupt -
 * the console processor wants another character.
 */
//pcxint(dev)
//	dev_t dev;
//{
//	register struct tty *tp;
//	register int unit;
//
//	if (!pcconsintr)
//		return;
//	pccons.t_state &= ~TS_BUSY;
//	pcconsoftc.cs_timo = 0;
//	if (pccons.t_line)
//		ldiscif_start(&pccons);
//		/*(*linesw[pccons.t_line].l_start)(&pccons);*/
//	else
//		pcstart(&pccons);
//}

int pcstart(register struct tty* tp)
{
	int c, s;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
		goto out;
	do {
		if (RB_LEN(&tp->t_out) <= tp->t_lowat) {
			if (tp->t_state&TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t)&tp->t_out);
			}
			if (tp->t_wsel) {
				selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
				tp->t_wsel = 0;
				tp->t_state &= ~TS_WCOLL;
			}
		}
		if (RB_LEN(&tp->t_out) == 0)
		{
			goto out;
		}
		c = getc(&tp->t_out);
		tp->t_state |= TS_BUSY;
		splx(s);
		sput(c, 0);
		(void)spltty();
		tp->t_state &= ~TS_BUSY;
	} while(1);
out:
	splx(s);

	return (0);
}

static __color;

/* ARGSUSED */
void
pccnputc(dev_t dev, unsigned c)
{
	if (c == '\n')
		sput('\r', 1);
	sput(c, 1);
}

/*
 * Print a character on console.
 */
pcputchar(c, tp)
	char c;
	register struct tty *tp;
{
	sput(c, 1);
	/*if (c=='\n') getchar();*/
}


/* ARGSUSED */
int
pccngetc(dev_t dev)
{
	register int s;
	register char *cp;

	if (pc_xmode)
		return(0);
_debug_mode_ = 1;
	s = spltty();		/* block pcrint while we poll */
	cp = sgetc(0);
	splx(s);
_debug_mode_ = 0;
	if (cp == 0)
{
printf("+");
		return (0);
}
	if (*cp == '\r') return('\n');
	return (*cp);
}

pcgetchar(tp)
	register struct tty *tp;
{
	char *cp;

	if (pc_xmode)
		return(0);
	cp = sgetc(0);
	return (*cp&0xff);
}

/*
 * Set line parameters
 */
pcparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register int cflag = t->c_cflag;
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	return(0);
}

#ifdef KDB
/*
 * Turn input polling on/off (used by debugger).
 */
pcpoll(onoff)
	int onoff;
{
}
#endif

/*
 * cursor():
 *   reassigns cursor position, updated by the rescheduling clock
 *   which is a index (0-1999) into the text area. Note that the
 *   cursor is a "foreground" character, it's color determined by
 *   the fg_at attribute. Thus if fg_at is left as 0, (FG_BLACK),
 *   as when a portion of screen memory is 0, the cursor may dissappear.
 */

static u_short *crtat = 0;

cursor(int a)
{ 	int pos = crtat - Crtat;

	if (!pc_xmode) {
	outb(addr_6845, 14);
	outb(addr_6845+1, pos>> 8);
	outb(addr_6845, 15);
	outb(addr_6845+1, pos);
#ifdef	FAT_CURSOR
	outb(addr_6845, 10);
	outb(addr_6845+1, 0);
	outb(addr_6845, 11);
	outb(addr_6845+1, 18);
#endif	/* FAT_CURSOR */
	}
	if (a == 0)
		timeout(cursor, 0, hz/10);
}

/*
 * Half-word fill function, like memset.
 */
extern inline void
memsetw(void *toaddr, int pat, size_t maxlength) {
	void *t = toaddr;

	/* construct pattern for fill */
	if (pat) {
		pat &= 0xffff;
		pat |= (pat<<16);
	}

	/* fill by words first, then any remaining halfwords */
	asm volatile ("cld ; repe ; stosl" :
	    "=D" (t) : "0" (t), "c" (maxlength / 2), "a" (pat));

	asm volatile ("repe ; stosw" :
	    "=D" (t) : "0" (t), "c" (maxlength & 1), "a" (pat));
}

static u_char shift_down, ctrl_down, alt_down, caps, num, scroll;

#define	wrtchar(c, at) \
	{ char *cp = (char *)crtat; *cp++ = (c); *cp = (at); crtat++; vs.col++; }


/* translate ANSI color codes to standard pc ones */
static char fgansitopc[] =
{	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY};

static char bgansitopc[] =
{	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY};

/*
 *   sput has support for emulation of the 'pc3' termcap entry.
 *   if ka, use kernel attributes.
 */
void sput(c,  ka)
u_char c;
u_char ka;
{

	int sc = 1;	/* do scroll check */
	char fg_at, bg_at, at;

	if(pc_xmode) return;

	if (crtat == 0)
	{
		u_short *cp = Crtat + (CGA_BUF-MONO_BUF)/CHR, was;
		unsigned cursorat;

		/* check if mono */
		if ((rtcin(0x14) & 0x30) != 0x30) {
			/*Crtat = cp;*/
			addr_6845 = CGA_BASE;
			vs.color=1;
		} else {
			addr_6845 = MONO_BASE;
			vs.color=0;
		}
#ifdef nope
addr_6845 = MONO_BASE;
/*Crtat = Crtat + (CGA_BUF-MONO_BUF)/CHR;*/
#endif
		/*
		 *   Crtat  initialized  to  point  to  MONO  buffer  if not present
		 *   change   to  CGA_BUF  offset  ONLY  ADD  the  difference  since
		 *   locore.s adds in the remapped offset at the right time
		 */

		was = *cp;
		*cp = (u_short) 0xA55A;
		if (*cp != 0xA55A) {
			addr_6845 = MONO_BASE;
			vs.color=0;
		} else {
			*cp = was;
			addr_6845 = CGA_BASE;
			Crtat = Crtat + (CGA_BUF-MONO_BUF)/CHR;
			vs.color=1;
		}
		/* Extract cursor location */
		outb(addr_6845,14);
		cursorat = inb(addr_6845+1)<<8 ;
		outb(addr_6845,15);
		cursorat |= inb(addr_6845+1);

		crtat = Crtat + cursorat;
		vs.ncol = COL;
		vs.nrow = ROW;
		vs.fg_at = FG_LIGHTGREY;
		vs.bg_at = BG_BLACK;

		if (vs.color == 0) {
			vs.kern_fg_at = FG_INTENSE;
			vs.so_at = FG_BLACK | BG_LIGHTGREY;
		} else {
			vs.kern_fg_at = FG_LIGHTGREY;
			vs.so_at = FG_YELLOW | BG_BLACK;
		}
		vs.kern_bg_at = BG_BLACK;

		memsetw(crtat, ((vs.bg_at|vs.fg_at)<<8)|' ', COL*ROW-cursorat);
	}

	/* which attributes do we use? */
	if (ka) {
		fg_at = vs.kern_fg_at;
		bg_at = vs.kern_bg_at;
	} else {
		fg_at = vs.fg_at;
		bg_at = vs.bg_at;
	}
	at = fg_at|bg_at;

	switch(c) {
		int inccol;

	case 0x1B:
		if(vs.esc)
			wrtchar(c, vs.so_at); 
		vs.esc = 1; vs.ebrac = 0; vs.eparm = 0;
		break;

	case '\t':
		inccol = (8 - vs.col % 8);	/* non-destructive tab */
		crtat += inccol;
		vs.col += inccol;
		break;

	case '\010':
		crtat--; vs.col--;
		if (vs.col < 0) vs.col += vs.ncol;  /* non-destructive backspace */
		break;

	case '\r':
		crtat -=  (crtat - Crtat) % vs.ncol; vs.col = 0;
		break;

	case '\n':
		crtat += vs.ncol ;
		break;

	default:
	bypass:
		if (vs.esc) {
			if (vs.ebrac) {
				switch(c) {
					int pos;
				case 'm':
					if (!vs.cx) vs.so = 0;
					else vs.so = 1;
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'A': /* back cx rows */
					if (vs.cx <= 0) vs.cx = 1;
					pos = crtat - Crtat;
					pos -= vs.ncol * vs.cx;
					if (pos < 0)
						pos += vs.nrow * vs.ncol;
					crtat = Crtat + pos;
					sc = vs.esc = vs.ebrac = vs.eparm = 0;
					break;
				case 'B': /* down cx rows */
					if (vs.cx <= 0) vs.cx = 1;
					pos = crtat - Crtat;
					pos += vs.ncol * vs.cx;
					if (pos >= vs.nrow * vs.ncol) 
						pos -= vs.nrow * vs.ncol;
					crtat = Crtat + pos;
					sc = vs.esc = vs.ebrac = vs.eparm = 0;
					break;
				case 'C': /* right cursor */
					if (vs.cx <= 0)
						vs.cx = 1;
					pos = crtat - Crtat;
					pos += vs.cx; vs.col += vs.cx;
					if (vs.col >= vs.ncol) {
						vs.col -= vs.ncol;
						pos -= vs.ncol;     /* cursor stays on same line */
					}
					crtat = Crtat + pos;
					sc = vs.esc = vs.ebrac = vs.eparm = 0;
					break;
				case 'D': /* left cursor */
					if (vs.cx <= 0)
						vs.cx = 1;
					pos = crtat - Crtat;
					pos -= vs.cx; vs.col -= vs.cx;
					if (vs.col < 0) {
						vs.col += vs.ncol;
						pos += vs.ncol;     /* cursor stays on same line */
					}
					crtat = Crtat + pos;
					sc = vs.esc = vs.ebrac = vs.eparm = 0;
					break;
				case 'J': /* Clear ... */
					if (vs.cx == 0)
						/* ... to end of display */
						memsetw(crtat, (at << 8) + ' ',
							Crtat + vs.ncol * vs.nrow - crtat);
					else if (vs.cx == 1)
						/* ... to next location */
						memsetw(Crtat, (at << 8) + ' ',
							crtat - Crtat + 1);
					else if (vs.cx == 2)
						/* ... whole display */
						memsetw(Crtat, (at << 8) + ' ',
							vs.ncol * vs.nrow);
						
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'K': /* Clear line ... */
					if (vs.cx == 0)
						/* ... current to EOL */
						memsetw(crtat, (at << 8) + ' ',
							vs.ncol - (crtat - Crtat) % vs.ncol);
					else if (vs.cx == 1)
						/* ... beginning to next */
						memsetw(crtat - (crtat - Crtat) % vs.ncol,
							(at << 8) + ' ',
							((crtat - Crtat) % vs.ncol) + 1);
					else if (vs.cx == 2)
						/* ... entire line */
						memsetw(crtat - (crtat - Crtat) % vs.ncol,
							(at << 8) + ' ',
							vs.ncol);
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'f': /* in system V consoles */
				case 'H': /* Cursor move */
					if ((!vs.cx)||(!vs.cy)) {
						crtat = Crtat;
						vs.col = 0;
					} else {
						crtat = Crtat + (vs.cx - 1) * vs.ncol + vs.cy - 1;
						vs.col = vs.cy - 1;
					}
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'S':  /* scroll up cx lines */
					if (vs.cx <= 0) vs.cx = 1;
					memcpy(Crtat, Crtat+vs.ncol*vs.cx, vs.ncol*(vs.nrow-vs.cx)*CHR);
					memsetw(Crtat+vs.ncol*(vs.nrow-vs.cx),
						(at <<8)+' ', vs.ncol*vs.cx);
					/* crtat -= vs.ncol*vs.cx; /* XXX */
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'M': /* delete line. */
					vs.row = (crtat - Crtat) / vs.ncol;
					if (vs.row + 1 < vs.nrow) {
						memcpy(Crtat+vs.ncol*vs.row,
							Crtat+vs.ncol*(vs.row+1),
						(vs.nrow-(vs.row+1)) * vs.ncol * CHR);
					}
					memsetw(Crtat+vs.ncol*(vs.nrow-1), (at <<8)+' ', vs.ncol);
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'T':  /* scroll down cx lines */
					if (vs.cx <= 0) vs.cx = 1;
					memmove(Crtat+vs.ncol*vs.cx, Crtat, vs.ncol*(vs.nrow-vs.cx)*CHR);
					memsetw(Crtat, (at <<8)+' ',
						vs.ncol*vs.cx);
					/* crtat += vs.ncol*vs.cx; /* XXX */
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'L' : /* insert line */
					vs.row = (crtat- Crtat) / vs.ncol;
					if (vs.row +1 < vs.nrow) {
						memmove (Crtat+vs.ncol*(vs.row+1),
						Crtat+vs.ncol*vs.row,
						(vs.nrow-(vs.row+1)) * vs.ncol * CHR);
					}
					memsetw(Crtat+vs.ncol*vs.row, (at <<8)+' ',
 						vs.ncol);
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case ';': /* Switch params in cursor def */
					vs.eparm = 1;
					break;
				case 'r':
					vs.so_at = (vs.cx & 0x0f) | ((vs.cy & 0x0f) << 4);
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
				case 'x': /* set attributes */
					switch (vs.cx) {
					case 0:
						/* reset to normal attributes */
						bg_at = BG_BLACK;
						if (ka)
							fg_at = vs.color? FG_LIGHTGREY: FG_UNDERLINE;
						else
							fg_at = FG_LIGHTGREY;
						break;
					case 1:
						/* ansi background */
						if (vs.color)
							bg_at = bgansitopc[vs.cy & 7];
						break;
					case 2:
						/* ansi foreground */
						if (vs.color)
							fg_at = fgansitopc[vs.cy & 7];
						break;
					case 3:
						/* pc text attribute */
						if (vs.eparm) {
							fg_at = vs.cy & 0x8f;
							bg_at = vs.cy & 0x70;
						}
						break;
					}
					if (ka) {
						vs.kern_fg_at = fg_at;
						vs.kern_bg_at = bg_at;
					} else {
						vs.fg_at = fg_at;
						vs.bg_at = bg_at;
					}
					vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					break;
					
				default: /* Only numbers valid here */
					if ((c >= '0')&&(c <= '9')) {
						if (vs.eparm) {
							vs.cy *= 10;
							vs.cy += c - '0';
						} else {
							vs.cx *= 10;
							vs.cx += c - '0';
						}
					} else {
						vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					}
					break;
				}
				break;
			} else if (c == 'c') { /* Clear screen & home */
				memsetw(Crtat, (at << 8) + ' ', vs.ncol*vs.nrow);
				crtat = Crtat; vs.col = 0;
				vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
			} else if (c == '[') { /* Start ESC [ sequence */
				vs.ebrac = 1; vs.cx = 0; vs.cy = 0; vs.eparm = 0;
			} else { /* Invalid, clear state */
				 vs.esc = 0; vs.ebrac = 0; vs.eparm = 0;
					wrtchar(c, vs.so_at); 
			}
		} else {
			if (c == 7)
				sysbeep(0x31b, hz/4);
			else {
				if (vs.so) {
					wrtchar(c, vs.so_at);
				} else
					wrtchar(c, at); 
				if (vs.col >= vs.ncol) {
					/*crtat -=  (crtat - Crtat) % vs.ncol;*/
					vs.col = 0;
				}
				break ;
			}
		}
	}
	if (sc && crtat >= Crtat+vs.ncol*vs.nrow) { /* scroll check */
		if (openf) do (void)sgetc(1); while (scroll);
		memcpy(Crtat, Crtat+vs.ncol, vs.ncol*(vs.nrow-1)*CHR);
		memsetw(Crtat + vs.ncol*(vs.nrow-1), (at << 8) + ' ',
			vs.ncol);
		crtat -= vs.ncol;
	}
	if (ka)
		cursor(1);
}


unsigned	__debug = 0; /*0xffe */;
static char scantokey[] = {
0,
120,	/* F9 */
0,
116,	/* F5 */
114,	/* F3 */
112,	/* F1 */
113,	/* F2 */
123,	/* F12 */
0,
121,	/* F10 */
119,	/* F8 */
117,	/* F6 */
115,	/* F4 */
16,	/* TAB */
1,	/* ` */
0,
0,
60,	/* ALT (left) */
44,	/* SHIFT (left) */
0,
58,	/* CTRL (left) */
17,	/* Q */
2,	/* 1 */
0,
0,
0,
46,	/* Z */
32,	/* S */
31,	/* A */
18,	/* W */
3,	/* 2 */
0,
0,
48,	/* C */
47,	/* X */
33,	/* D */
19,	/* E */
5,	/* 4 */
4,	/* 3 */
0,
0,
61,	/* SPACE */
49,	/* V */
34,	/* F */
21,	/* T */
20,	/* R */
6,	/* 5 */
0,
0,
51,	/* N */
50,	/* B */
36,	/* H */
35,	/* G */
22,	/* Y */
7,	/* 6 */
0,
0,
0,
52,	/* M */
37,	/* J */
23,	/* U */
8,	/* 7 */
9,	/* 8 */
0,
0,
53,	/* , */
38,	/* K */
24,	/* I */
25,	/* O */
11,	/* 0 */
10,	/* 9 */
0,
0,
54,	/* . */
55,	/* / */
39,	/* L */
40,	/* ; */
26,	/* P */
12,	/* - */
0,
0,
0,
41,	/* " */
0,
27,	/* [ */
13,	/* + */
0,
0,
0,
57,	/* SHIFT (right) */
43,	/* ENTER */
28,	/* ] */
0,
29,	/* \ */
0,
0,
0,
45,	/* na*/
0,
0,
0,
0,
15,	/* backspace */
0,
0,				/* keypad */
93,	/* 1 */
0,
92,	/* 4 */
91,	/* 7 */
0,
0,
0,
99,	/* 0 */
104,	/* . */
98,	/* 2 */
97,	/* 5 */
102,	/* 6 */
96,	/* 8 */
110,	/* ESC */
90,	/* Num Lock */
122,	/* F11 */
106,	/* + */
103,	/* 3 */
105,	/* - */
100,	/* * */
101,	/* 9 */
0,
0,
0,
0,
0,
118,	/* F7 */
};
static char extscantokey[] = {
0,
120,	/* F9 */
0,
116,	/* F5 */
114,	/* F3 */
112,	/* F1 */
113,	/* F2 */
123,	/* F12 */
0,
121,	/* F10 */
119,	/* F8 */
117,	/* F6 */
115,	/* F4 */
16,	/* TAB */
1,	/* ` */
0,
0,
 62,	/* ALT (right) */
 124,	/* Print Screen */
0,
 64,	/* CTRL (right) */
17,	/* Q */
2,	/* 1 */
0,
0,
0,
46,	/* Z */
32,	/* S */
31,	/* A */
18,	/* W */
3,	/* 2 */
0,
0,
48,	/* C */
47,	/* X */
33,	/* D */
19,	/* E */
5,	/* 4 */
4,	/* 3 */
0,
0,
61,	/* SPACE */
49,	/* V */
34,	/* F */
21,	/* T */
20,	/* R */
6,	/* 5 */
0,
0,
51,	/* N */
50,	/* B */
36,	/* H */
35,	/* G */
22,	/* Y */
7,	/* 6 */
0,
0,
0,
52,	/* M */
37,	/* J */
23,	/* U */
8,	/* 7 */
9,	/* 8 */
0,
0,
53,	/* , */
38,	/* K */
24,	/* I */
25,	/* O */
11,	/* 0 */
10,	/* 9 */
0,
0,
54,	/* . */
 95,	/* / */
39,	/* L */
40,	/* ; */
26,	/* P */
12,	/* - */
0,
0,
0,
41,	/* " */
0,
27,	/* [ */
13,	/* + */
0,
0,
0,
57,	/* SHIFT (right) */
 108,	/* ENTER */
28,	/* ] */
0,
29,	/* \ */
0,
0,
0,
45,	/* na*/
0,
0,
0,
0,
15,	/* backspace */
0,
0,				/* keypad */
 81,	/* end */
0,
 79,	/* left arrow */
 80,	/* home */
0,
0,
0,
 75,	/* ins */
 76,	/* del */
 84,	/* down arrow */
97,	/* 5 */
 89,	/* right arrow */
 83,	/* up arrow */
110,	/* ESC */
90,	/* Num Lock */
122,	/* F11 */
106,	/* + */
 86,	/* page down */
105,	/* - */
 124,	/* print screen */
 85,	/* page up */
0,
0,
0,
0,
0,
118,	/* F7 */
};
#define	CODE_SIZE	4		/* Use a max of 4 for now... */
typedef struct
{
	u_short	type;
	char	unshift[CODE_SIZE];
	char	shift[CODE_SIZE];
	char	ctrl[CODE_SIZE];
} Scan_def;

#define	SHIFT		0x0002	/* keyboard shift */
#define	ALT		0x0004	/* alternate shift -- alternate chars */
#define	NUM		0x0008	/* numeric shift  cursors vs. numeric */
#define	CTL		0x0010	/* control shift  -- allows ctl function */
#define	CAPS		0x0020	/* caps shift -- swaps case of letter */
#define	ASCII		0x0040	/* ascii code for this key */
#define	SCROLL		0x0080	/* stop output */
#define	FUNC		0x0100	/* function key */
#define	KP		0x0200	/* Keypad keys */
#define	NONE		0x0400	/* no function */

static Scan_def	scan_codes[] =
{
	NONE,	"",		"",		"",		/* 0 unused */
	ASCII,	"\033",		"\033",		"\033",		/* 1 ESCape */
	ASCII,	"1",		"!",		"!",		/* 2 1 */
	ASCII,	"2",		"@",		"\000",		/* 3 2 */
	ASCII,	"3",		"#",		"#",		/* 4 3 */
	ASCII,	"4",		"$",		"$",		/* 5 4 */
	ASCII,	"5",		"%",		"%",		/* 6 5 */
	ASCII,	"6",		"^",		"\036",		/* 7 6 */
	ASCII,	"7",		"&",		"&",		/* 8 7 */
	ASCII,	"8",		"*",		"\010",		/* 9 8 */
	ASCII,	"9",		"(",		"(",		/* 10 9 */
	ASCII,	"0",		")",		")",		/* 11 0 */
	ASCII,	"-",		"_",		"\037",		/* 12 - */
	ASCII,	"=",		"+",		"+",		/* 13 = */
	ASCII,	"\177",		"\177",		"\010",		/* 14 backspace */
	ASCII,	"\t",		"\177\t",	"\t",		/* 15 tab */
	ASCII,	"q",		"Q",		"\021",		/* 16 q */
	ASCII,	"w",		"W",		"\027",		/* 17 w */
	ASCII,	"e",		"E",		"\005",		/* 18 e */
	ASCII,	"r",		"R",		"\022",		/* 19 r */
	ASCII,	"t",		"T",		"\024",		/* 20 t */
	ASCII,	"y",		"Y",		"\031",		/* 21 y */
	ASCII,	"u",		"U",		"\025",		/* 22 u */
	ASCII,	"i",		"I",		"\011",		/* 23 i */
	ASCII,	"o",		"O",		"\017",		/* 24 o */
	ASCII,	"p",		"P",		"\020",		/* 25 p */
	ASCII,	"[",		"{",		"\033",		/* 26 [ */
	ASCII,	"]",		"}",		"\035",		/* 27 ] */
	ASCII,	"\r",		"\r",		"\n",		/* 28 return */
	CTL,	"",		"",		"",		/* 29 control */
	ASCII,	"a",		"A",		"\001",		/* 30 a */
	ASCII,	"s",		"S",		"\023",		/* 31 s */
	ASCII,	"d",		"D",		"\004",		/* 32 d */
	ASCII,	"f",		"F",		"\006",		/* 33 f */
	ASCII,	"g",		"G",		"\007",		/* 34 g */
	ASCII,	"h",		"H",		"\010",		/* 35 h */
	ASCII,	"j",		"J",		"\n",		/* 36 j */
	ASCII,	"k",		"K",		"\013",		/* 37 k */
	ASCII,	"l",		"L",		"\014",		/* 38 l */
	ASCII,	";",		":",		";",		/* 39 ; */
	ASCII,	"'",		"\"",		"'",		/* 40 ' */
	ASCII,	"`",		"~",		"`",		/* 41 ` */
	SHIFT,	"",		"",		"",		/* 42 shift */
	ASCII,	"\\",		"|",		"\034",		/* 43 \ */
	ASCII,	"z",		"Z",		"\032",		/* 44 z */
	ASCII,	"x",		"X",		"\030",		/* 45 x */
	ASCII,	"c",		"C",		"\003",		/* 46 c */
	ASCII,	"v",		"V",		"\026",		/* 47 v */
	ASCII,	"b",		"B",		"\002",		/* 48 b */
	ASCII,	"n",		"N",		"\016",		/* 49 n */
	ASCII,	"m",		"M",		"\r",		/* 50 m */
	ASCII,	",",		"<",		"<",		/* 51 , */
	ASCII,	".",		">",		">",		/* 52 . */
	ASCII,	"/",		"?",		"\177",		/* 53 / */
	SHIFT,	"",		"",		"",		/* 54 shift */
	KP,	"*",		"*",		"*",		/* 55 kp * */
	ALT,	"",		"",		"",		/* 56 alt */
	ASCII,	" ",		" ",		" ",		/* 57 space */
	CAPS,	"",		"",		"",		/* 58 caps */
	FUNC,	"\033[M",	"\033[Y",	"\033[k",	/* 59 f1 */
	FUNC,	"\033[N",	"\033[Z",	"\033[l",	/* 60 f2 */
	FUNC,	"\033[O",	"\033[a",	"\033[m",	/* 61 f3 */
	FUNC,	"\033[P",	"\033[b",	"\033[n",	/* 62 f4 */
	FUNC,	"\033[Q",	"\033[c",	"\033[o",	/* 63 f5 */
	FUNC,	"\033[R",	"\033[d",	"\033[p",	/* 64 f6 */
	FUNC,	"\033[S",	"\033[e",	"\033[q",	/* 65 f7 */
	FUNC,	"\033[T",	"\033[f",	"\033[r",	/* 66 f8 */
	FUNC,	"\033[U",	"\033[g",	"\033[s",	/* 67 f9 */
	FUNC,	"\033[V",	"\033[h",	"\033[t",	/* 68 f10 */
	NUM,	"",		"",		"",		/* 69 num lock */
	SCROLL,	"",		"",		"",		/* 70 scroll lock */
	KP,	"7",		"\033[H",	"7",		/* 71 kp 7 */
	KP,	"8",		"\033[A",	"8",		/* 72 kp 8 */
	KP,	"9",		"\033[I",	"9",		/* 73 kp 9 */
	KP,	"-",		"-",		"-",		/* 74 kp - */
	KP,	"4",		"\033[D",	"4",		/* 75 kp 4 */
	KP,	"5",		"\033[E",	"5",		/* 76 kp 5 */
	KP,	"6",		"\033[C",	"6",		/* 77 kp 6 */
	KP,	"+",		"+",		"+",		/* 78 kp + */
	KP,	"1",		"\033[F",	"1",		/* 79 kp 1 */
	KP,	"2",		"\033[B",	"2",		/* 80 kp 2 */
	KP,	"3",		"\033[G",	"3",		/* 81 kp 3 */
	KP,	"0",		"\033[L",	"0",		/* 82 kp 0 */
	KP,	".",		"\177",		".",		/* 83 kp . */
	NONE,	"",		"",		"",		/* 84 0 */
	NONE,	"100",		"",		"",		/* 85 0 */
	NONE,	"101",		"",		"",		/* 86 0 */
	FUNC,	"\033[W",	"\033[i",	"\033[u",	/* 87 f11 */
	FUNC,	"\033[X",	"\033[j",	"\033[v",	/* 88 f12 */
	NONE,	"102",		"",		"",		/* 89 0 */
	NONE,	"103",		"",		"",		/* 90 0 */
	NONE,	"",		"",		"",		/* 91 0 */
	NONE,	"",		"",		"",		/* 92 0 */
	NONE,	"",		"",		"",		/* 93 0 */
	NONE,	"",		"",		"",		/* 94 0 */
	NONE,	"",		"",		"",		/* 95 0 */
	NONE,	"",		"",		"",		/* 96 0 */
	NONE,	"",		"",		"",		/* 97 0 */
	NONE,	"",		"",		"",		/* 98 0 */
	NONE,	"",		"",		"",		/* 99 0 */
	NONE,	"",		"",		"",		/* 100 */
	NONE,	"",		"",		"",		/* 101 */
	NONE,	"",		"",		"",		/* 102 */
	NONE,	"",		"",		"",		/* 103 */
	NONE,	"",		"",		"",		/* 104 */
	NONE,	"",		"",		"",		/* 105 */
	NONE,	"",		"",		"",		/* 106 */
	NONE,	"",		"",		"",		/* 107 */
	NONE,	"",		"",		"",		/* 108 */
	NONE,	"",		"",		"",		/* 109 */
	NONE,	"",		"",		"",		/* 110 */
	NONE,	"",		"",		"",		/* 111 */
	NONE,	"",		"",		"",		/* 112 */
	NONE,	"",		"",		"",		/* 113 */
	NONE,	"",		"",		"",		/* 114 */
	NONE,	"",		"",		"",		/* 115 */
	NONE,	"",		"",		"",		/* 116 */
	NONE,	"",		"",		"",		/* 117 */
	NONE,	"",		"",		"",		/* 118 */
	NONE,	"",		"",		"",		/* 119 */
	NONE,	"",		"",		"",		/* 120 */
	NONE,	"",		"",		"",		/* 121 */
	NONE,	"",		"",		"",		/* 122 */
	NONE,	"",		"",		"",		/* 123 */
	NONE,	"",		"",		"",		/* 124 */
	NONE,	"",		"",		"",		/* 125 */
	NONE,	"",		"",		"",		/* 126 */
	NONE,	"",		"",		"",		/* 127 */
};



update_led()
{
/*printf("- %x ",	kbd_cmd_read_param(K_READOUTP));
	kbd_drain();
	printf (" kbd %x ", kbd_cmd_read_param(K_READ + K__CMDBYTE));
	kbd_drain();
printf("|");
	kbd_cmd_write_param(K_SIMAUXIN, 0x60);
	kbd_drain();
printf("|");
	while(aux_cmd(0xf4) != KBR_ACK);
	kbd_drain(); */
	key_cmd(KBC_STSIND);			/* LED Command */
	kbd_drain();
	key_cmd(scroll | 2*num | 4*caps);
	kbd_drain();
}

/*
 *   sgetc(noblock):  get  characters  from  the  keyboard.  If
 *   noblock  ==  0  wait  until a key is gotten. Otherwise return a
 *    if no characters are present 0.
 */
char *
sgetc(noblock)
{
	u_char		dt, sts;
	unsigned	key,op;
	static u_char	extended = 0;
	static char	capchar[2];

	/*
	 *   First see if there is something in the keyboard port
	 */
loop:
	sts = inb(KBSTATP);
	/*if ((sts & (KBS_AUXDIB|KBS_DIB)) == (KBS_AUXDIB|KBS_DIB)) {
		printf("ax: ");
	}
	if (sts & KBS_DIB) {
		printf(".");
	}*/

	if (sts & KBS_DIB)
		dt = inb(KBDATAP);
	else
	{
		if (noblock)
			return 0;
		else
			goto loop;
	}

#ifdef futz
	op = kbd_cmd_read_param(0xd0);
	/* if((op & 0x10) == 0)
		printf( "O%x ", op); */
/* "off" on toshiba laptops 4400/4500 (mouse?) */
/* "o7b" on compaq systempro */
	if((op & 0x20) != 0)
		printf( "o%x ", op);
#endif

	if (pc_xmode) { /* XXX char* what X expects to be returned? */
		if (dt == 69)	/*numlock*/
			pc_xmode = 0;
		capchar[0] = dt;
		capchar[1] = 0;
		return (capchar);
	}
	if (dt == 0xe0)
	{
		extended = 1;
		if (noblock)
			return 0;
		else
			goto loop;
	}

	/*
	 *   Check for cntl-alt-del
	 */
	if ((dt == 83) && ctrl_down && alt_down)
		cpu_reset();
	if ((dt == 69) && ctrl_down && alt_down)
		pc_xmode = 1;

#ifdef DDB
	/*
	 *   Check for cntl-alt-esc
	 */
	if ((dt&0x7f) == 1 && ctrl_down && alt_down && _debug_mode_ == 0) {
		if (dt == 0x01) {
			Debugger();
		}
		return (0);
	}
#endif

#ifdef nope
	if (pc_xmode) { /* XXX char* what X expects to be returned? */
		capchar[0] = dt;
		return (capchar);
	}
#endif

	/*
	 *   Check for make/break
	 */
	if (dt & 0x80)
	{
		/*
		 *   break
		 */
		dt = dt & 0x7f;
		switch (scan_codes[dt].type)
		{
			case SHIFT:
				shift_down = 0;
				break;
			case ALT:
				alt_down = 0;
				break;
			case CTL:
				ctrl_down = 0;
				break;
		}
	}
	else
	{
		/*
		 *   Make
		 */
		dt = dt & 0x7f;
		switch (scan_codes[dt].type)
		{
			/*
			 *   Locking keys
			 */
			case NUM:
				num ^= 1;
				update_led();
				break;
			case CAPS:
				caps ^= 1;
				update_led();
				break;
			case SCROLL:
				scroll ^= 1;
				update_led();
				break;

			/*
			 *   Non-locking keys
			 */
			case SHIFT:
				shift_down = 1;
				break;
			case ALT:
				alt_down = 0x80;
				break;
			case CTL:
				ctrl_down = 1;
				break;
			case ASCII:
			case NONE:
			case FUNC:
				if (shift_down)
					more_chars = scan_codes[dt].shift;
				else if (ctrl_down)
					more_chars = scan_codes[dt].ctrl;
				else
					more_chars = scan_codes[dt].unshift;
				/* XXX */
				if (caps && more_chars[1] == 0
					&& (more_chars[0] >= 'a'
						&& more_chars[0] <= 'z')) {
					capchar[0] = *more_chars - ('a' - 'A');
					more_chars = capchar;
				}
				extended = 0;
				return(more_chars);
			case KP:
				if (shift_down || ctrl_down || !num || extended)
					more_chars = scan_codes[dt].shift;
				else
					more_chars = scan_codes[dt].unshift;
				extended = 0;
				return(more_chars);
		}
	}
	extended = 0;
	if (noblock)
		return 0;
	else
		goto loop;
}

u_int pg(char* p, int q, int r, int s, int t, int u, int v, int w, int x, int y, int z)
{
	printf(p,q,r,s,t,u,v,w,x,y,z);
	printf("\n");
	return(console_getchar());
}

/* special characters */
#define bs	8
#define lf	10	
#define cr	13	
#define cntlc	3	
#define del	0177	
#define cntld	4

getchar()
{
	char	thechar;
	register	delay;
	int		x;

	pcconsoftc.cs_flags |= CSF_POLLING;
	x = splhigh();
	sput('>', 1);
	/*while (1) {*/
		thechar = *(sgetc(0));
		pcconsoftc.cs_flags &= ~CSF_POLLING;
		splx(x);
		switch (thechar) {
		    default: if (thechar >= ' ')
			     	sput(thechar, 1);
			     return(thechar);
		    case cr:
		    case lf: sput('\r', 1);
		    		sput('\n', 1);
			     return(lf);
		    case bs:
		    case del:
			     sput('\b', 1);
			     sput(' ', 1);
			     sput('\b', 1);
			     return(thechar);
		    case cntlc:
			     sput('^', 1) ; sput('C', 1) ; sput('\r', 1) ; sput('\n', 1) ;
			     cpu_reset();
		    case cntld:
			     sput('^', 1) ; sput('D', 1) ; sput('\r', 1) ; sput('\n', 1) ;
			     return(0);
		}
	/*}*/
}

#include "machine/stdarg.h"
static nrow;

#define	DPAUSE 1
void
#ifdef __STDC__
dprintf(unsigned flgs, const char *fmt, ...)
#else
dprintf(flgs, fmt /*, va_alist */)
        char *fmt;
	unsigned flgs;
#endif
{	extern unsigned __debug;
	va_list ap;

	if((flgs&__debug) > DPAUSE) {
		__color = ffs(flgs&__debug)+1;
		va_start(ap,fmt);
		kprintf(fmt, 1, (struct tty *)0, ap);
		va_end(ap);
	if (flgs&DPAUSE || nrow%24 == 23) { 
		int x;
		x = splhigh();
		if (nrow%24 == 23) nrow = 0;
		(void)sgetc(0);
		splx(x);
	}
	}
	__color = 0;
}


#include "machine/psl.h"
#include "machine/frame.h"

void pc_xmode_on ()
{
    struct syscframe *fp;

    if (pc_xmode)
            return;
    pc_xmode = 1;

    fp = (struct syscframe *)curproc->p_md.md_regs;
    fp->sf_eflags |= PSL_IOPL;
}

void pc_xmode_off ()
{
    struct syscframe *fp;

    if (pc_xmode == 0)
            return;
    pc_xmode = 0;

    fp = (struct syscframe *)curproc->p_md.md_regs;
    fp->sf_eflags &= ~PSL_IOPL;
}
  
int pcmmap(dev_t dev, int offset, int nprot)
{
	if (offset > 0x20000)
		return -1;
	return i386_btop((0xa0000 + offset));
}

int
pcselect(dev_t dev, int rw, struct proc *p) {

	return (ttselect(&pccons, rw, p));
}


struct devif pc_devif =
{
	{0}, -1, -2, 0, 0, 0, 0, 0, 0,
	pcopen, pcclose, pcioctl, pcread, pcwrite, pcselect, pcmmap,
	0, 0, 0, 0,
	pccngetc,  pccnputc, 
};

DRIVER_MODCONFIG(pccons) {
	char *cfg_string = pc_config;
	
	if (devif_config(&cfg_string, &pc_devif) == 0)
		return;

	/* probe for hardware */
	new_isa_configure(&cfg_string, &pcdriver);
}

CONSOLE_MODCONFIG(pccons) {
	char *cfg1 = pc_console_config;
	char *cfg2 = pc_config;

	if (console_config(&cfg1, &cfg2, &pc_devif) == 0)
		return;
#ifdef unneeded
	/* probe for hardware */
	new_isa_configure(&cfg2, &pcdriver);
#endif
}

#ifdef DEBUG
/*
 *
 */
wpl(int x, int y) {
	short *p = Crtat ;
	int i, clr;
	int sv;

	asm("lahf" : "=a" (sv));

	if (sv & 0x8000)
		clr = 3;
	else
		clr = 5;

	p += (80-17);
	
	*p++ =  (clr<<8) + 'A' + y ;
	for (i=15; i >= 0; i--)
		if (x & (1<<i))
		 *p++ =  (clr<<8) + '1' ;
		else
		 *p++ =  (clr<<8) + '0' ;
	if((x & 3) == 1)
		outb(0x21, x & 0xfe);
}
#endif
