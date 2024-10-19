/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	$Id$
 */
#define WD_COUNT_RETRIES
#define	NWDC			64
#define	IO_WDCSIZE		8
static int wdtest = 0;

/* standard ISA/AT configuration: */
static char *wd_config =
	"wd 0 3 1 (0x1f0 14).	# ide driver $Revision$ ";
#define	NWD 2	/* XXX dynamic */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <dkbad.h>
#include <disklabel.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <vnode.h>
#include <buf.h>
#include <uio.h>
#include <malloc.h>
#include <machine/cpu.h>
#include <isa_driver.h>
#include <isa_irq.h>
#include <machine/icu.h>
#include "wdreg.h"
#include "atapi.h"
#include <vm.h>
#include <modconfig.h>
#include <prototypes.h>
#include <machine/inline/io.h>	/* inline io port functions */
#include <proc.h>

#define	RETRIES			5	/* number of retries before giving up */
#define RECOVERYTIME	500000	/* usec for controller to recover after err */
//#define	MAXTRANSFER	32	/* max size of transfer in page clusters */
#define	MAXTRANSFER		255	/* max size of transfer in page clusters */
//#define TIMEOUT		30
#define TIMEOUT			10000

#ifdef notyet
#define wdnoreloc(dev)	(minor(dev) & 0x80)	/* ignore partition table */
#endif
#define wddospart(dev)	(minor(dev) & 0x40)	/* use dos partitions */
#define wdunit(dev)	((minor(dev) & 0x38) >> 3)
#define wdpart(dev)	(minor(dev) & 0x7)
#define makewddev(maj, unit, part)	(makedev(maj,((unit<<3)+part)))
#define WDRAW	3		/* 'd' partition isn't a partition! */

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

/*
 * Drive states.  Used to initialize drive.
 */

#define	CLOSED		0		/* disk is closed. */
#define	WANTOPEN	1		/* open requested, not started */
#define	RECAL		2		/* doing restore */
#define	OPEN		3		/* done with open */

/*
 * The structure of a disk drive.
 */
struct	disk {
	long				dk_bc;		/* byte count left */
	short				dk_skip;	/* blocks already transferred */
	char				dk_ctrlr;	/* physical controller number */
	u_long				dk_unit;	/* physical unit number */
	u_long				dk_lunit;	/* logical unit number */
	char				dk_state;	/* control state */
	u_char				dk_status;	/* copy of status reg. */
	u_char				dk_error;	/* copy of error reg. */
	u_char				dk_timeout;	/* countdown to next timeout */
	short				dk_port;	/* i/o port base */
	
    u_long				dk_copenpart;   /* character units open on this drive */
    u_long				dk_bopenpart;   /* block units open on this drive */
    u_long				dk_openpart;    /* all units open on this drive */
	short				dk_wlabel;	/* label writable? */
	short				dk_flags;	/* drive characteistics found */
#define	DKFL_DOSPART	0x00001	 /* has DOS partition table */
#define	DKFL_QUIET		0x00002	 /* report errors back, but don't complain */
#define	DKFL_SINGLE		0x00004	 /* sector at a time mode */
#define	DKFL_ERROR		0x00008	 /* processing a disk error */
#define	DKFL_BSDLABEL	0x00010	 /* has a BSD disk label */
#define	DKFL_BADSECT	0x00020	 /* has a bad144 badsector table */
#define	DKFL_WRITEPROT	0x00040	 /* manual unit write protect */
#define	DKFL_LABELLING	0x00080	 /* readdisklabel() in progress */
	struct wdparams		dk_params; /* ESDI/IDE drive/controller parameters */
	struct disklabel	dk_dd;	/* device configuration data */
	struct dos_partition
		dk_dospartitions[NDOSPART];	/* DOS view of disk */
	struct	dkbad		dk_bad;	/* bad sector table */
	int					dk_alive;
	long    			dk_badsect[127];        /* 126 plus trailing -1 marker */
};

static struct disk	*wddrives[NWD];		/* table of units */
static struct buf	wdtab;
static struct buf	wdutab[NWD];		/* head of queue per drive */
static struct buf	rwdbuf[NWD];		/* buffers for raw IO */
static long			wdxfer[NWD];		/* count of transfers */
#ifdef	WDDEBUG
int	wddebug;
#endif

/* per driver interface structure */
struct	isa_driver wddriver = {
	wdprobe, wdattach, wdintr, "wd", &biomask
};


/* default per device */
static struct isa_device wd_default_devices[] = {
{ &wddriver,    0x1f0, IRQ14, -1,  0x00000,     0,  0 },
{ &wddriver,    0x170, IRQ14, -1,  0x00000,     0,  2 },
{ 0 }
};

extern int   splbio(void);
extern char* sgetc(int);

static void  wdustart(struct disk*);
static void  wdstart(void);
static void  wdfinished(struct buf*, struct buf*);
static int   wdcommand(struct disk*, int, int, int, int, int);
static int   wdcontrol(struct buf*);
static int   wdsetctlr(struct disk*);
static int   wdwsetctlr(struct disk*);
static int   wdgetctlr(int, struct disk*);
static int   wdwait(struct disk*, u_char, int);
static int   wdunwedge(struct disk*);
static void  wdflushirq(struct disk*, int);
static int   wdreset(struct disk *du);
static int   wdcwait(struct disk*, int);
static void  wdsleep(int, char*);
static int   wdidentify(struct disk*, int);
static void  wderror(struct buf*, struct disk*, char*);

/*
 * Probe for controller.
 */
static int
wdprobe(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	struct disk *du;
	static int lastunit;
	int wdc, ok = 0;
	int rv;

	/* if wildcard unit number, use last to be allocated unit */
	if (unit == '?')
	{
		dvp->id_unit = unit = lastunit;
	}

	/* check for both drives/controllers */
	for (; unit <= dvp->id_unit+1; unit++)
	{
		if (unit > NWD)
		{
			break;
		}

		if ((du = wddrives[unit]) == 0)
		{
			du = wddrives[unit] =
				(struct disk *)malloc(sizeof(struct disk), M_TEMP, M_WAITOK);
			memset(du, 0, sizeof(struct disk));
			du->dk_unit = unit;
		}

		wdc = du->dk_port = dvp->id_iobase;

		/* IDENTIFYコマンドを実行 */
		rv = wdidentify(du, unit);
		if (rv != 0)
		{
			goto nodevice;
		}
		
		/* is there a controller: execute a controller only command */
		if (wdcommand(du, 0, 0, 0, 0, WDCC_DIAGNOSE) < 0 ||
			wdwait(du, 0, TIMEOUT) < 0)
		{
			goto nodevice;
		}

		(void) inb(wdc + wd_error);	/* XXX! */
		outb(wdc + wd_ctlr, WDCTL_4BIT);
		ok |= 1;
		continue;

nodevice:
		free(du, M_TEMP);
		wddrives[unit] = 0;
	}

	lastunit = unit + 1;
	return (ok);
}

static int wdidentify(struct disk* du, int unit)
{
#define	LOOPMAX		1000
	int		sts;
	int 	wdc = du->dk_port;
	int		count;
	int		LBAmid, LBAhi;
	u_char 	data[DEV_BSIZE];

	outb(wdc + wd_sdh, unit == 0 ? WDSD_IBM : 0xb0);
	outb(wdc + wd_sector,  0);
	outb(wdc + wd_cyl_lo,  0);
	outb(wdc + wd_cyl_hi,  0);
	outb(wdc + wd_command, WDCC_READP);

	count = 0;
	do
	{
		sts = inb(wdc + wd_status); 
		count++;
		if ((sts == 0) || (count > LOOPMAX))
		{
			// ステータスが0、またはポーリングが
			// LOOPMAXを超えたらドライブは存在しない
			du->dk_alive = 0;
			return (-1);
		}
	}
	while ((sts & WDCS_BUSY) != 0 &&
		   ((sts & WDCS_DRQ) == 0) && ((sts & WDCS_ERR) == 0));

#ifdef IN_PROGRESS
	LBAmid = inb(wdc + wd_cyl_lo);
	LBAhi  = inb(wdc + wd_cyl_hi);
	if (LBAmid == 0x14 && LBAhi == 0xeb)
	{
		// ATAPIドライブ
		break;
	}
#endif

	insw(wdc + wd_data, (caddr_t)data, DEV_BSIZE / sizeof(short));
	(void)memcpy(&du->dk_params, data, sizeof(struct wdparams));
	du->dk_alive = 1;

/*
printf("heads = %d, ", du->dk_params.wdp_heads);
printf("sectors = %d, ", du->dk_params.wdp_sectors);
printf("cylinders = %d\n", du->dk_params.wdp_fixedcyl+du->dk_params.wdp_removcyl);
*/

	return (0);
}

/*
 * Attach each drive if possible.
 */
static void wdattach(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	struct disk *du;

	for (; unit <= dvp->id_unit + 1; unit++)
	{
		du = wddrives[unit];
		if (du == 0)
			continue;
		printf("wd%d", unit);

		if(wdgetctlr(unit, du) == 0)
		{
		int i, blank;
			char c;

			printf(" <");
			for (i = blank = 0 ; i < sizeof(du->dk_params.wdp_model); i++)
			{
				c = du->dk_params.wdp_model[i];

				if (blank && c == ' ') continue;
				if (blank && c != ' ') {
					printf(" %c", c);
					blank = 0;
					continue;
				} 
				if (c == ' ')
					blank = 1;
				else
					printf("%c", c);
			}
			printf("> ");
		}
		/* check for index pulses from each drive. if present, report and
		   allocate a bios drive position to it, which will be used by read disklabel */
		du->dk_ctrlr = dvp->id_unit;
		du->dk_unit = unit;
		du->dk_alive = 1;
	}
}

static inline void wddisksort(struct buf* dp, struct buf* bp)
{
	struct buf *ap;

	while ((ap = dp->b_actf) && ap->b_flags & B_XXX)
	{
		dp = ap;
	}
	disksort(dp, bp);
}

/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
static int wdstrategy(register struct buf *bp)
{
	register struct buf *dp;
	struct disk *du;	/* Disk unit to do the IO.	*/
	int	unit = wdunit(bp->b_dev);
	int	s;

	/* valid unit, controller, and request?  */
	if (unit >= NWD || bp->b_blkno < 0 || (du = wddrives[unit]) == 0)
	{
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* "soft" write protect check */
	if ((du->dk_flags & DKFL_WRITEPROT) && (bp->b_flags & B_READ) == 0)
	{
		bp->b_error = EROFS;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* have partitions and want to use them? */
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW)
	{
		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &du->dk_dd, du->dk_wlabel) <= 0)
		{
			goto done;
		}
		/* otherwise, process transfer request */
	}

	/* queue transfer on drive, activate drive and controller if idle */
	dp = &wdutab[unit];
	s = splbio();
	wddisksort(dp, bp);
	if (dp->b_active == 0)
	{
		wdustart(du);		/* start drive */
	}

	if (du->dk_flags & DKFL_LABELLING && du->dk_state > RECAL)
	{
		wdsleep(du->dk_ctrlr, "wdlab");
		du->dk_state = WANTOPEN;
	}

	if (wdtab/*[ctrlr]*/.b_active == 0)
	{
		wdstart(/*du->dk_ctrlr*/);		/* start controller */
	}
	splx(s);
	return ( 0 );

done:
	/* toss transfer, we're done early */
	s = splbio();
	biodone(bp);
	splx(s);
	return ( 0 );
}

/*
 * Routine to queue a command to the controller.  The unit's
 * request is linked into the active list for the controller.
 * If the controller is idle, the transfer is started.
 */
static void
wdustart(register struct disk *du)
{
	register struct buf *bp, *dp = &wdutab[du->dk_unit];
	int ctrlr = du->dk_ctrlr;

	/* unit already active? */
	if (dp->b_active)
	{
		return;
	}

	/* anything to start? */
	bp = dp->b_actf;
	if (bp == NULL)
	{
		return;	
	}

	/* link onto controller queue */
	dp->b_forw = NULL;
	if (wdtab.b_actf  == NULL)
	{
		wdtab/*[ctrlr]*/.b_actf = dp;
	}
	else
	{
		wdtab/*[ctrlr]*/.b_actl->b_forw = dp;
	}
	wdtab/*[ctrlr]*/.b_actl = dp;

	/* mark the drive unit as busy */
	dp->b_active = 1;
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1.	The transfer length must be an exact multiple of the sector size.
 */

static void
wdstart(void)
{
	register struct disk	*du;	/* disk unit for IO */
	register struct buf		*bp;
	struct disklabel		*lp;
	struct buf				*dp;
	register struct bt_bad	*bt_ptr;
	long					blknum, pagcnt;
	long					cylin, head, sector;
	long					secpertrk, secpercyl, i;
	int						unit, s, wdc;

loop:
	/* is there a drive for the controller to do a transfer with? */
	dp = wdtab.b_actf;
	if (dp == NULL)
	{
		return;
	}

	/* is there a transfer to this drive ? if so, link it on
	   the controller's queue */
	bp = dp->b_actf;
	if (bp == NULL)
	{
		wdtab.b_actf = dp->b_forw;
		goto loop;
	}

	/* obtain controller and drive information */
	unit = wdunit(bp->b_dev);
	du = wddrives[unit];

	/* if not really a transfer, do control operations specially */
	if (du->dk_state < OPEN)
	{
		if (du->dk_state != WANTOPEN)
			printf("wd%d: wdstart: weird dk_state %d\n",
			       du->dk_unit, du->dk_state);
		if (wdcontrol(bp) != 0)
		{
			printf("wd%d: wdstart: wdcontrol returned nonzero, state = %d\n",
			       du->dk_unit, du->dk_state);
		}
		return;
	}

#if 0
	if (du->dk_flags & DKFL_ERROR)
	{
		du->dk_flags &= ~DKFL_ERROR;
		if ((du->dk_flags & DKFL_SINGLE) == 0)
		{
			du->dk_flags |= DKFL_SINGLE;
			du->dk_skip = 0;
		}
	}
#endif

	/* calculate transfer details */
	blknum = bp->b_blkno + du->dk_skip;
/*if(wddebug)printf("bn%d ", blknum);*/
#ifdef	WDDEBUG
	if (du->dk_skip == 0)
		printf("\nwdstart %d: %s %d@%d; map ", unit,
			(bp->b_flags & B_READ) ? "read" : "write",
			bp->b_bcount, blknum);
	else
		printf(" %d)%x", du->dk_skip, inb(wdc+wd_altsts));
#endif

	if (du->dk_skip == 0)
	{
		du->dk_bc = min(bp->b_bcount, MAXTRANSFER * NBPG);
		bp->b_resid = bp->b_bcount - du->dk_bc;
	}

	lp = &du->dk_dd;
	secpertrk = lp->d_nsectors;
	secpercyl = lp->d_secpercyl;
/*
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW)
	{
		blknum += lp->d_partitions[wdpart(bp->b_dev)].p_offset;
	}
*/
	if ((du->dk_flags & (DKFL_SINGLE | DKFL_BADSECT)) == (DKFL_SINGLE | DKFL_BADSECT))
	{
		int i;
	
		for(i=0; du->dk_badsect[i] != -1 && du->dk_badsect[i] <= blknum; i++)
		{

			if( du->dk_badsect[i] == blknum)
			{
			/*
			 * XXX the offset of the bad sector table ought
			 * to be stored in the in-core copy of the table.
			 */
#define BAD144_PART	2	/* XXX scattered magic numbers */
#define BSD_PART	0	/* XXX should be 2 but bad144.c uses 0 */
				if (lp->d_partitions[BSD_PART].p_offset != 0)
				{
					blknum = lp->d_partitions[BAD144_PART].p_offset
						 + lp->d_partitions[BAD144_PART].p_size;
				}
				else
				{
					blknum = lp->d_secperunit;
				}
				blknum -= lp->d_nsectors + i + 1;
				break;
			}
		}
	}

	cylin  = blknum / secpercyl;
	head   = (blknum % secpercyl) / secpertrk;
	sector = blknum % secpertrk;

#if 0
	if ((du->dk_flags & (DKFL_SINGLE | DKFL_BADSECT))
						== (DKFL_SINGLE|DKFL_BADSECT))
	{
	    for (bt_ptr = du->dk_bad.bt_bad; bt_ptr->bt_cyl != 0xffff; bt_ptr++)
		{
			if (bt_ptr->bt_cyl > cylin)
			{
				/* Sorted list, and we passed our cylinder. quit. */
				break;
			}

			if (bt_ptr->bt_cyl == cylin &&
					bt_ptr->bt_trksec == (head << 8) + sector)
			{
				/*
				* Found bad block.  Calculate new block addr.
				* This starts at the end of the disk (skip the
				* last track which is used for the bad block list),
				* and works backwards to the front of the disk.
				*/
#ifdef	WDDEBUG
			    printf("--- badblock code -> Old = %d; ",
				blknum);
#endif
				blknum = lp->d_secperunit - lp->d_nsectors
							- (bt_ptr - du->dk_bad.bt_bad) - 1;
				cylin  = blknum / secpercyl;
				head   = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
#ifdef	WDDEBUG
			    printf("new = %d\n", blknum);
#endif
				break;
			}
		}
	}
#endif

/*if(wddebug)pg("c%d h%d s%d ", cylin, head, sector);*/
	sector += 1;	/* sectors begin with 1, not 0 */

	wdtab.b_active = 1;		/* mark controller active */
	wdc = du->dk_port;

	/* if starting a multisector transfer, or doing single transfers */
	if (du->dk_skip == 0 || (du->dk_flags & DKFL_SINGLE))
	{
		if (wdtab.b_errcnt && (bp->b_flags & B_READ) == 0)
		{
			du->dk_bc += DEV_BSIZE;
		}

		/* controller idle? */
		int cmd, count;

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT)
		{
			sector = lp->d_gap3;
			count = lp->d_nsectors;
			cmd = WDCC_FORMAT;
		}
		else
		{
			count = (du->dk_flags & DKFL_SINGLE) ? 1 : howmany(du->dk_bc, DEV_BSIZE);
			cmd = (bp->b_flags & B_READ) ? WDCC_READ : WDCC_WRITE;
		}
#else	// B_FORMAT
		count = (du->dk_flags & DKFL_SINGLE) ? 1 : howmany(du->dk_bc, DEV_BSIZE);
		cmd   = (bp->b_flags & B_READ) ? WDCC_READ : WDCC_WRITE;
#endif	// B_FORMAT


		/*
		 * XXX this loop may never terminate.  The code to handle
		 * counting down of retries and eventually failing the i/o
		 * is in wdintr() and we can't get there from here.
		 */
		if (wdtest != 0) {
			if (--wdtest == 0) {
				wdtest = 100;
				printf("dummy wdunwedge\n");
				wdunwedge(du);
			}
		}

		while (wdcommand(du, cylin, head, sector, count, cmd) != 0)
		{
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;	/* XXX needs translation */
			wderror(bp, du, "wdstart: timeout waiting to give command");
			wdunwedge(du);
//			wdfinished(dp, bp);
//			goto loop;
		}

#ifdef	WDDEBUG
		printf("sector %d cylin %d head %d addr %x sts %x\n",
	    		sector, cylin, head, bp->b_un.b_addr, inb(wdc+wd_altsts));
#endif
	}

	du->dk_timeout = 1 + 3;

	/* if this is a read operation, just go away until it's done.	*/
	if (bp->b_flags & B_READ)
	{
		return;
	}

	/* ready to send data?	*/
#if 0
	while ((inb(wdc+wd_status) & WDCS_DRQ) == 0)
		;
#else
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT) < 0)
	{
		wderror(bp, du, "wdstart: timeout waiting for DRQ");
	}
#endif

	/* then send it! */
	outsw(wdc + wd_data,
		  bp->b_un.b_addr + du->dk_skip * DEV_BSIZE,
		  DEV_BSIZE / sizeof(short));
	du->dk_bc -= DEV_BSIZE;
}

/* Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start
 * the next request.  Also check for a partially done transfer, and
 * continue with the next chunk if so.
 */
void
wdintr(int dev)
{
	register struct	disk *du;
	register struct buf *bp, *dp;
	int status, wdc;
	char partch ;

	if (!wdtab.b_active)
	{
#ifdef nyet
		printf("wd: extra interrupt\n");
#endif
		return;
	}

	dp = wdtab.b_actf;
	bp = dp->b_actf;
	du = wddrives[wdunit(bp->b_dev)];
	wdc = du->dk_port;

#ifdef	WDDEBUG
	printf("I ");
#endif

	/* controller still busy? */
#if 0
	while ((status = inb(wdc+wd_status)) & WDCS_BUSY) ;
#else
	(void)wdwait(du, 0, TIMEOUT);
#endif

	/* is it not a transfer, but a control operation? */
	if (du->dk_state < OPEN)
	{
		wdtab.b_active = 0;
		if (wdcontrol(bp))
		{
			wdstart();
		}
		return;
	}

	/* have we an error? */
	if (status & (WDCS_ERR | WDCS_ECCCOR))
	{
oops:
		du->dk_status = status;
		du->dk_error = inb(wdc + wd_error);

#ifdef	WDDEBUG
		printf("status %x error %x\n", status, du->dk_error);
#endif
		if((du->dk_flags & DKFL_SINGLE) == 0)
		{
			du->dk_flags |=  DKFL_ERROR;
			goto outt;
		}
#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			goto done;
		}
#endif
		
		/* error or error correction? */
		if (status & WDCS_ERR)
		{
			if (++wdtab.b_errcnt < RETRIES)
			{
				wdtab.b_active = 0;
			}
			else
			{
				if((du->dk_flags&DKFL_QUIET) == 0)
				{
					diskerr(bp, "wd", "hard error",
						LOG_PRINTF, du->dk_skip,
						&du->dk_dd);
#ifdef WDDEBUG
					printf( "status %b error %b\n",
						status, WDCS_BITS,
						inb(wdc+wd_error), WDERR_BITS);
#endif
				}
				bp->b_flags |= B_ERROR;	/* flag the error */
				bp->b_error = EIO;
			}
		}
		else if((du->dk_flags&DKFL_QUIET) == 0)
		{
			diskerr(bp, "wd", "soft ecc", 0,
				du->dk_skip, &du->dk_dd);
		}
	}
outt:

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ) && wdtab.b_active)
	{
		int chk, dummy;

#if 0
		chk = min(du->dk_dd.d_secsize / sizeof(short), du->dk_bc / sizeof(short));

		/* ready to receive data? */
		while ((inb(wdc+wd_status) & WDCS_DRQ) == 0)
			;

		/* suck in data */
		insw (wdc + wd_data,
			  bp->b_un.b_addr + du->dk_skip * du->dk_dd.d_secsize, chk);
		du->dk_bc -= chk * sizeof(short);
#else
		chk = min(DEV_BSIZE / sizeof(short), du->dk_bc / sizeof(short));

		if ((du->dk_status & (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
		    != (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
		{
			wderror(bp, du, "wdintr: read intr arrived early\n");
//			return;
		}
		/* ready to receive data? */
		if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT) != 0)
		{
			wderror(bp, du, "wdintr: read error detected late");
			goto oops;
		}

		insw(wdc + wd_data, bp->b_un.b_addr + du->dk_skip * DEV_BSIZE, chk);
		du->dk_bc -= chk * sizeof(short);
#endif

		/* for obselete fractional sector reads */
		while (chk++ < DEV_BSIZE / sizeof(short))
		{
			insw(du->dk_port + wd_data, (caddr_t)&dummy, 1);
		}
	}

	wdxfer[du->dk_unit]++;
	if (wdtab.b_active)
	{
		if ((bp->b_flags & B_ERROR) == 0)
		{
			du->dk_skip++;		/* Add to successful sectors. */
			if (wdtab.b_errcnt && (du->dk_flags & DKFL_QUIET) == 0)
			{
				diskerr(bp, "wd", "soft error", 0,
					du->dk_skip, &du->dk_dd);
			}
			wdtab.b_errcnt = 0;

			/* see if more to transfer */
			if (du->dk_bc > 0 && (du->dk_flags & DKFL_ERROR) == 0)
			{
				wdtab.b_active = 0;
				wdstart();
				return;		/* next chunk is started */
			}
			else if ((du->dk_flags & (DKFL_SINGLE|DKFL_ERROR)) == DKFL_ERROR)
			{
				du->dk_skip = 0;
				du->dk_flags &= ~DKFL_ERROR;
				du->dk_flags |=  DKFL_SINGLE;
				wdtab.b_active = 0;
				wdstart();
				return;		/* redo xfer sector by sector */
			}
		}

done:
		/* done with this transfer, with or without error */
		du->dk_flags &= ~DKFL_SINGLE;
		wdtab.b_actf = dp->b_forw;
		wdtab.b_errcnt = 0;
		bp->b_resid = bp->b_bcount - du->dk_skip * DEV_BSIZE;
		du->dk_skip = 0;
		dp->b_active = 0;
		dp->b_actf = bp->av_forw;
		dp->b_errcnt = 0;
		biodone(bp);
	}

	/* controller idle */
	wdtab.b_active = 0;

	/* anything more on drive queue? */
	if (dp->b_actf)
	{
		wdustart(du);
	}
	/* anything more for controller to do? */
	if (wdtab.b_actf)
	{
		wdstart();
	}
}

/*
static void
wdfinished(struct buf *dp, struct buf *bp)
{
	wdtab.b_actf = dp->b_forw;
	wdtab.b_errcnt = 0;
	dp->b_active = 0;
	dp->b_actf = bp->av_forw;
	dp->b_errcnt = 0;
	biodone(bp);
}
*/

/*
 * Initialize a drive.
 */
static int
wdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	register unsigned int unit;
	register struct disk *du;
    int part = wdpart(dev), mask = 1 << part;
    struct partition *pp;
	struct dkbad *db;
	int i, error = 0;
	char *msg;

	unit = wdunit(dev);
	if (unit >= NWD)
	{
		return (ENXIO) ;
	}

	du = wddrives[unit];
	if (du == 0 || du->dk_alive == 0)
	{
		return (ENXIO) ;
	}

	/* Finish flushing IRQs left over from wdattach(). */
	if (wdtab/*[ctrlr]*/.b_active == 2)
	{
		wdtab/*[ctrlr]*/.b_active = 0;
	}

	if ((du->dk_flags & DKFL_BSDLABEL) == 0)
	{
		du->dk_flags |= DKFL_LABELLING | DKFL_WRITEPROT;
		wdutab[unit].b_actf = NULL;

		/*
		 * Use the default sizes until we've read the label,
		 * or longer if there isn't one there.
		 */
		(void)memset(&du->dk_dd, 0, sizeof(du->dk_dd));
		du->dk_dd.d_type = DTYPE_ST506;
		du->dk_dd.d_ncylinders = 1024;
		du->dk_dd.d_secsize = DEV_BSIZE;
		du->dk_dd.d_ntracks = 8;
		du->dk_dd.d_nsectors = 17;
		du->dk_dd.d_secpercyl = 17*8;
		du->dk_state = WANTOPEN;
		du->dk_unit = unit;
		if (part == WDRAW)
		{
			du->dk_flags |= DKFL_QUIET;
			du->dk_state = OPEN+1;
			du->dk_dd.d_secsize = 2048;
goto nope;
		}
		else
		{
			du->dk_flags &= ~DKFL_QUIET;
		}

		/* read label using "c" partition */
		msg = readdisklabel(makewddev(major(dev), wdunit(dev), WDRAW),
							wdstrategy, &du->dk_dd, du->dk_dospartitions,
							&du->dk_bad, 0);

		du->dk_flags &= ~DKFL_LABELLING;

		if (msg != 0)
		{
			if((du->dk_flags&DKFL_QUIET) == 0)
			{
				log(LOG_WARNING, "wd%d: cannot find label (%s)\n", unit, msg);
				error = EINVAL;		/* XXX needs translation */
			} /* else printf("quiet ");*/
			goto done;
		}
		else
		{
			wdsetctlr(du);
			du->dk_flags |= DKFL_BSDLABEL;
			du->dk_flags &= ~DKFL_WRITEPROT;
			if (du->dk_dd.d_flags & D_BADSECT)
			{
				du->dk_flags |= DKFL_BADSECT;
			}
		}

done:
		if (error)
		{
			return(error);
		}
	}
nope:
	/*
		* Warn if a partion is opened
		* that overlaps another partition which is open
		* unless one is the "raw" partition (whole disk).
		*/
	if ((du->dk_openpart & mask) == 0 /*&& part != RAWPART*/ && part != WDRAW)
	{
		int	start, end;

		pp = &du->dk_dd.d_partitions[part];
		start = pp->p_offset;
		end = pp->p_offset + pp->p_size;
		for (pp = du->dk_dd.d_partitions;
			pp < &du->dk_dd.d_partitions[du->dk_dd.d_npartitions]; pp++)
		{
			if (pp->p_offset + pp->p_size <= start ||
				pp->p_offset >= end)
			{
				continue;
			}
			/*if (pp - du->dk_dd.d_partitions == RAWPART)
				continue; */
			if (pp - du->dk_dd.d_partitions == WDRAW)
			{
				continue;
			}
			if (du->dk_openpart & (1 << (pp -
				du->dk_dd.d_partitions)))
			{
				log(LOG_WARNING,
					"wd%d%c: overlaps open partition (%c)\n",
					unit, part + 'a',
					pp - du->dk_dd.d_partitions + 'a');
			}
		}
	}
	if (part >= du->dk_dd.d_npartitions && part != WDRAW)
	{
		return (ENXIO);
	}

	/* insure only one open at a time */
	du->dk_openpart |= mask;
	switch (fmt)
	{
	case S_IFCHR:
		du->dk_copenpart |= mask;
		break;
	case S_IFBLK:
		du->dk_bopenpart |= mask;
		break;
	}

	return (0);
}

/*
 * Implement operations other than read/write.
 * Called from wdstart or wdintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed.
 */
#if 0
static int
wdcontrol(register struct buf *bp)
{
	register struct disk *du;
	int unit;
	unsigned char  stat;
	int s, cnt;
	extern int bootdev;
	int cyl, trk, sec, i, wdc;

	du = wddrives[wdunit(bp->b_dev)];
	unit = du->dk_unit;
	wdc = du->dk_port;
	
	switch (du->dk_state) {

	case WANTOPEN:			/* set SDH, step rate, do restore */
tryagainrecal:
		wdtab/*[du->ck_ctrlr]*/.b_active = 1;
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) < 0)
		{
			wderror(bp, du, "wdcontrol: wdcommand failed");
			goto badopen;
		}
		du->dk_state = RECAL;
		return(0);

	case RECAL:
		stat = inb(wdc + wd_status);
		if (stat & WDCS_ERR)
		{
			if ((du->dk_flags & DKFL_QUIET) == 0)
			{
				printf("wd%d: recal", du->dk_unit);
				printf(": status %b error %b\n",
					stat, WDCS_BITS, inb(wdc+wd_error),
					WDERR_BITS);
			}
			if (++wdtab.b_errcnt < RETRIES)
			{
				goto tryagainrecal;
			}
			goto badopen;
		}

		/* some controllers require this ... */
		wdsetctlr(du);

		wdtab/*[du->ck_ctrlr]*/.b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done
		 * by normal means.
		 */
		return(1);

	default:
		panic("wdcontrol");
	}
	/* NOTREACHED */

badopen:
	if ((du->dk_flags & DKFL_QUIET) == 0) 
		printf(": status %b error %b\n",
			stat, WDCS_BITS, inb(wdc + wd_error), WDERR_BITS);
	bp->b_flags |= B_ERROR;
	bp->b_error = ENXIO;	/* XXX needs translation */
	return(1);
}
#else
static int
wdcontrol(register struct buf *bp)
{
	register struct disk *du;
	int	ctrlr;

	du = wddrives[wdunit(bp->b_dev)];
	ctrlr = du->dk_ctrlr;

	switch (du->dk_state) {
	case WANTOPEN:
tryagainrecal:
		wdtab/*[ctrlr]*/.b_active = 1;
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0) {
			wderror(bp, du, "wdcontrol: wdcommand failed");
			goto maybe_retry;
		}
		du->dk_state = RECAL;
		return (0);
	case RECAL:
		if (du->dk_status & WDCS_ERR || wdsetctlr(du) != 0) {
			wderror(bp, du, "wdcontrol: recal failed");
maybe_retry:
			if (du->dk_status & WDCS_ERR)
				wdunwedge(du);
			du->dk_state = WANTOPEN;
			if (++wdtab/*[ctrlr]*/.b_errcnt < RETRIES)
				goto tryagainrecal;
			bp->b_error = ENXIO;	/* XXX needs translation */
			bp->b_flags |= B_ERROR;
			return (2);
		}
		wdtab/*[ctrlr]*/.b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done by normal
		 * means.
		 */
		return (1);
	}
	panic("wdcontrol");
	return (2);
}
#endif

/*
 * send a command and optionally wait uninterruptibly until controller
 * is finished.
 * return -1 if controller busy for too long, otherwise
 * return status. intended for brief controller commands at critical points.
 * assumes interrupts are blocked if wait requested.
 */
static int
wdcommand(struct disk* du, int cylin, int head, int sector, int count, int cmd)
{
	int stat;
	int wdc;
    

	/* Wait for it to become ready to accept a command. */
	stat = wdwait(du,  0, TIMEOUT);
	if (stat != 0)
	{
		return (-1);
    }
	wdc = du->dk_port;

	/* Load parameters. */
	outb(wdc + wd_precomp, du->dk_dd.d_precompcyl / 4);
	outb(wdc + wd_cyl_lo,  cylin);
	outb(wdc + wd_cyl_hi,  cylin >> 8);
	outb(wdc + wd_sdh, WDSD_IBM | (du->dk_unit << 4) | head);
	outb(wdc + wd_sector,  sector);
	outb(wdc + wd_seccnt,  count);

	stat = wdwait(du, cmd == WDCC_DIAGNOSE || cmd == WDCC_IDC ?
				  0 : WDCS_READY, TIMEOUT);
	if (stat < 0)
	{
		return (-1);
	}

	/* Send command, await results. */
	outb(wdc + wd_command, cmd);

	return (0);
}

/*
 * issue IDC to drive to tell it just what geometry it is to be.
 */
static int
wdsetctlr(struct disk *du)
{
	int wdc;
	int error = 0;

	if (du->dk_dd.d_ntracks == 0 || du->dk_dd.d_ntracks > 16) {
		struct wdparams *wp;

		printf("wd%d: can't handle %lu heads from partition table ",
		       du->dk_unit, du->dk_dd.d_ntracks);
		/* obtain parameters */
		wp = &du->dk_params;
		if (wp->wdp_heads > 0 && wp->wdp_heads <= 16) {
			printf("(controller value %lu restored)\n",
				wp->wdp_heads);
			du->dk_dd.d_ntracks = wp->wdp_heads;
		}
		else {
			printf("(truncating to 16)\n");
			du->dk_dd.d_ntracks = 16;
		}
	}

	if (du->dk_dd.d_nsectors == 0 || du->dk_dd.d_nsectors > 255) {
		printf("wd%d: cannot handle %lu sectors (max 255)\n",
		       du->dk_lunit, du->dk_dd.d_nsectors);
		error = 1;
	}
	if (error) {
		wdtab/*[du->dk_ctrlr]*/.b_errcnt += RETRIES;
		return (1);
	}

	if (wdcommand(du, du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks - 1, 0,
		      du->dk_dd.d_nsectors, WDCC_IDC) != 0
	    || wdwait(du, WDCS_READY, TIMEOUT) < 0) {
		wderror((struct buf *)NULL, du, "wdsetctlr failed");
		return (1);
	}

	return (0);
}

static int
wdwsetctlr(struct disk* du)
{
	int	stat;
	int	x;

	wdsleep(du->dk_ctrlr, "wdwset");
	x = splbio();
	stat = wdsetctlr(du);
	wdflushirq(du, x);
	splx(x);

	return stat;

}

/*
 * issue READP to drive to ask it what it is.
 */
static int
wdgetctlr(int u, struct disk *du)
{
	int stat, x, i, wdc;
	char tb[DEV_BSIZE];
	struct wdparams *wp;

//	x = splbio();		/* not called from intr level ... */
	wdc = du->dk_port;
#if 0
	outb(wdc+wd_sdh, WDSD_IBM | (u << 4));
	stat = wdcommand(du, 0, 0, 0, 0, WDCC_READP);
	(void)wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT);

	if (stat < 0) {
		splx(x);
		return(stat);
	}

	/* obtain parameters */
	wp = &du->dk_params;
	insw(wdc+wd_data, (caddr_t) tb, sizeof(tb)/sizeof(short));
	(void)memcpy(wp, tb, sizeof(struct wdparams));
#endif
	wp = &du->dk_params;

	/* shuffle string byte order */
	for (i = 0; i < sizeof(wp->wdp_model) ;i += 2)
	{
		u_short *p;
		p = (u_short *)(wp->wdp_model + i);
		*p = ntohs(*p);
	}

	/* update disklabel given drive information */
	du->dk_dd.d_ncylinders = wp->wdp_fixedcyl + wp->wdp_removcyl /*+- 1*/;
	du->dk_dd.d_ntracks = wp->wdp_heads;
	du->dk_dd.d_nsectors = wp->wdp_sectors;
	du->dk_dd.d_secpercyl = du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
	du->dk_dd.d_partitions[1].p_size =
				du->dk_dd.d_secpercyl * wp->wdp_sectors;
	du->dk_dd.d_partitions[1].p_offset = 0;
	/* dubious ... */
	(void)memcpy(du->dk_dd.d_typename, "ESDI/IDE", 9);
	(void)memcpy(du->dk_dd.d_packname, wp->wdp_model+20, 14-1);
	/* better ... */
	du->dk_dd.d_type = DTYPE_ESDI;
	du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;

	/* XXX sometimes possibly needed */
	(void) inb(wdc+wd_status);
	return (0);
}


/* ARGSUSED */
static int
wdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	register struct disk *du;
	int part = wdpart(dev), mask = 1 << part;

	du = wddrives[wdunit(dev)];

	/* insure only one open at a time */
        du->dk_openpart &= ~mask;
        switch (fmt) {
        case S_IFCHR:
                du->dk_copenpart &= ~mask;
                break;
        case S_IFBLK:
                du->dk_bopenpart &= ~mask;
                break;
        }
	return(0);
}

static int
wdioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = wdunit(dev);
	register struct disk *du;
	int error = 0;
	struct uio auio;
	struct iovec aiov;

	du = wddrives[unit];

	switch (cmd)
	{
	case DIOCSBAD:
		if ((flag & FWRITE) == 0)
		{
			error = EBADF;
		}
		else
		{
			du->dk_bad = *(struct dkbad *)addr;
		}
		break;

	case DIOCGDINFO:
		*(struct disklabel *)addr = du->dk_dd;
		break;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = &du->dk_dd;
		((struct partinfo *)addr)->part =
			&du->dk_dd.d_partitions[wdpart(dev)];
		break;

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else
			error = setdisklabel(&du->dk_dd,
		(struct disklabel *)addr,
				/*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart : */0,
		du->dk_dospartitions);
		if (error == 0) {
			du->dk_flags |= DKFL_BSDLABEL;
			wdwsetctlr(du);
		}
		break;

	case DIOCWLABEL:
		du->dk_flags &= ~DKFL_WRITEPROT;
		if ((flag & FWRITE) == 0)
		{
			error = EBADF;
		}
		else
		{
			du->dk_wlabel = *(int *)addr;
		}
		break;

	case DIOCWDINFO:
		du->dk_flags &= ~DKFL_WRITEPROT;
		if ((flag & FWRITE) == 0)
		{
			error = EBADF;
		}
		else if ((error = setdisklabel(&du->dk_dd, (struct disklabel *)addr,
					/*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart :*/ 0,
					du->dk_dospartitions)) == 0)
		{
			int wlab;

			du->dk_flags |= DKFL_BSDLABEL;
			wdwsetctlr(du);

			/* simulate opening partition 0 so write succeeds */
			du->dk_openpart |= (1 << 0);            /* XXX */
			wlab = du->dk_wlabel;
			du->dk_wlabel = 1;
			error = writedisklabel(dev, wdstrategy,
					&du->dk_dd, du->dk_dospartitions);
			du->dk_openpart = du->dk_copenpart | du->dk_bopenpart;
			du->dk_wlabel = wlab;
		}
		break;

#ifdef notyet
	case DIOCGDINFOP:
		*(struct disklabel **)addr = &(du->dk_dd);
		break;

	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
		{
			error = EBADF;
		}
		else
		{
			register struct format_op *fop;

			fop = (struct format_op *)addr;
			aiov.iov_base = fop->df_buf;
			aiov.iov_len = fop->df_count;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_resid = fop->df_count;
			auio.uio_segflg = 0;
			auio.uio_offset =
				fop->df_startblk * du->dk_dd.d_secsize;
			error = physio(wdformat, &rwdbuf[unit], dev, B_WRITE,
				minphys, &auio);
			fop->df_count -= auio.uio_resid;
			fop->df_reg[0] = du->dk_status;
			fop->df_reg[1] = du->dk_error;
		}
		break;
#endif

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

#ifdef	B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return (wdstrategy(bp));
}
#endif

int
wdsize(dev_t dev)
{
	int unit = wdunit(dev), part = wdpart(dev), val;
	struct disk *du;

	if (unit >= NWD)
	{
		return(-1);
	}

	du = wddrives[unit];
	if (du == 0 || du->dk_state == 0)
	{
		val = wdopen (makewddev(major(dev), unit, WDRAW), FREAD, S_IFBLK, 0);
	}
	if (du == 0 || val != 0 || du->dk_flags & DKFL_WRITEPROT)
	{
		return (-1);
	}

	return((int)du->dk_dd.d_partitions[part].p_size);
}

extern        char *vmmap;            /* poor name! */

int
wddump(dev_t dev)			/* dump core after a system crash */
{
	register struct disk *du;	/* disk unit to do the IO */
	register struct bt_bad *bt_ptr;
	long	num;			/* number of sectors to write */
	int	unit, part, wdc;
	long	blkoff, blknum, blkcnt;
	long	cylin, head, sector, stat;
	long	secpertrk, secpercyl, nblocks, i;
	char	*addr;
	extern	int maxmem;
	static  int wddoingadump = 0 ;
	extern	caddr_t CADDR1;

	addr = (char *) 0;		/* starting address */

	/* toss any characters present prior to dump */
	while (sgetc(1))
		;

	/* size of memory to dump */
	num = maxmem;
	unit = wdunit(dev);		/* eventually support floppies? */
	part = wdpart(dev);		/* file system */
	/* check for acceptable drive number */
	if (unit >= NWD) return(ENXIO);

	du = wddrives[unit];
	if (du == 0) return(ENXIO);
	/* was it ever initialized ? */
	if (du->dk_state < OPEN) return (ENXIO) ;
	if (du->dk_flags & DKFL_WRITEPROT) return(ENXIO);
	wdc = du->dk_port;

	/* Convert to disk sectors */
	num = (u_long) num * NBPG / du->dk_dd.d_secsize;

	/* check if controller active */
	/*if (wdtab.b_active) return(EFAULT); */
	if (wddoingadump) return(EFAULT);

	secpertrk = du->dk_dd.d_nsectors;
	secpercyl = du->dk_dd.d_secpercyl;
	nblocks   = du->dk_dd.d_partitions[part].p_size;
	blkoff    = du->dk_dd.d_partitions[part].p_offset;

/*pg("xunit %x, nblocks %d, dumplo %d num %d\n", part,nblocks,dumplo,num);*/
	/* check transfer bounds against partition size */
	if (num > nblocks)
		return(EINVAL);

	/*wdtab.b_active = 1;*/		/* mark controller active for if we
					   panic during the dump */
	wddoingadump = 1;

	DELAY(5);		/* ATA spec XXX NOT */
	if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0
	    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0
	    || wdsetctlr(du) != 0) {
		wderror((struct buf *)NULL, du, "wddump: recalibrate failed");
		return (EIO);
	}

#if 0
	if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) < 0)
		return(EIO);

	/* some controllers require this ... */
	wdsetctlr(du);
#endif

	blknum = blkoff;
	while (num > 0)
	{
#ifdef notdef
		if (blkcnt > MAXTRANSFER) blkcnt = MAXTRANSFER;
		if ((blknum + blkcnt - 1) / secpercyl != blknum / secpercyl)
			blkcnt = secpercyl - (blknum % secpercyl);
			    /* keep transfer within current cylinder */
#endif
		pmap_enter(kernel_pmap, (vm_offset_t)vmmap, trunc_page(addr),
			VM_PROT_READ, TRUE, AM_NONE);

		/* compute disk address */
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;

#ifdef notyet
		/* 
		 * See if the current block is in the bad block list.
		 * (If we have one.)
		 */
	    for (bt_ptr = du->dk_bad.bt_bad;
			bt_ptr->bt_cyl != -1; bt_ptr++) {
			if (bt_ptr->bt_cyl > cylin)
				/* Sorted list, and we passed our cylinder.
					quit. */
				break;
			if (bt_ptr->bt_cyl == cylin &&
				bt_ptr->bt_trksec == (head << 8) + sector) {
			/*
			 * Found bad block.  Calculate new block addr.
			 * This starts at the end of the disk (skip the
			 * last track which is used for the bad block list),
			 * and works backwards to the front of the disk.
			 */
				blknum = (du->dk_dd.d_secperunit)
					- du->dk_dd.d_nsectors
					- (bt_ptr - du->dk_bad.bt_bad) - 1;
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
				break;
			}
		}
#endif
		sector++;		/* origin 1 */

		/* transfer some blocks */
		outb(wdc+wd_sector, sector);
		outb(wdc+wd_seccnt,1);
		outb(wdc+wd_cyl_lo, cylin);
		outb(wdc+wd_cyl_hi, cylin >> 8);
#ifdef notdef
		/* lets just talk about this first...*/
		pg ("sdh 0%o sector %d cyl %d addr 0x%x",
			inb(wdc+wd_sdh), inb(wdc+wd_sector),
			inb(wdc+wd_cyl_hi)*256+inb(wdc+wd_cyl_lo), addr) ;
#endif
		if (wdcommand(du, 0, 0, 0, 0, WDCC_WRITE) < 0)
			return(EIO);

		outsw (wdc+wd_data, vmmap+((int)addr&(NBPG-1)), 256);

		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;
		/* Check data request (should be done).         */
		if (inb(wdc+wd_status) & WDCS_DRQ) return(EIO) ;

		/* wait for completion */
		for ( i = 1000000 ; inb(wdc+wd_status) & WDCS_BUSY ; i--) {
				if (i < 0) return (EIO) ;
		}
		/* error check the xfer */
		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;

		if ((unsigned)addr % (1024*1024) == 0) printf("%d ", num/2048) ;
		/* update block count */
		num--;
		blknum++ ;
		/* (int) */  addr += 512;

		/* operator aborting dump? */
		if (sgetc(1))
			return(EINTR);
	}
	return(0);
}

static void
wderror(struct buf *bp, struct disk *du, char *mesg)
{
	if (bp == NULL)
	{
		printf("wd%d: %s:\n", du->dk_lunit, mesg);
	}
	else
	{
		diskerr(bp, "wd", mesg, LOG_PRINTF, du->dk_skip, &du->dk_dd);
	}
	printf("wd%d: status %b error %b\n", du->dk_lunit,
	       du->dk_status, WDCS_BITS, du->dk_error, WDERR_BITS);
}

#ifdef WD_COUNT_RETRIES
static int min_retries[NWDC];
#endif

static int
wdwait(struct disk *du, u_char bits_wanted, int timeout)
{
	const int POLLING = 1000;
	int	wdc;
	u_char	status;

	wdc = du->dk_port;
	timeout += POLLING;
	do
	{
#ifdef WD_COUNT_RETRIES
		if (min_retries[du->dk_ctrlr] > timeout
		    || min_retries[du->dk_ctrlr] == 0)
			min_retries[du->dk_ctrlr] = timeout;
#endif
		DELAY(5);	/* ATA spec XXX NOT */
		du->dk_status = status = inb(wdc + wd_status);
		if (!(status & WDCS_BUSY))
		{
			if (status & WDCS_ERR)
			{
				du->dk_error = inb(wdc + wd_error);
			}
			if ((status & bits_wanted) == bits_wanted)
			{
				return (status & WDCS_ERR);
			}
		}

		DELAY((timeout < TIMEOUT) ? 1000 : 1);

	} while (--timeout != 0);
	return (-1);
}

static int
wdunwedge(struct disk *du)
{
	struct disk *du1;
	int	unit;

	/* Schedule other drives for recalibration. */
	for (unit = 0; unit < NWD; unit++)
	{
		if ((du1 = wddrives[unit]) != NULL && du1 != du
		    && du1->dk_ctrlr == du->dk_ctrlr
		    && du1->dk_state > WANTOPEN)
		{
			du1->dk_state = WANTOPEN;
		}
	}

	DELAY(RECOVERYTIME);
	if (wdreset(du) == 0)
	{
		/*
		 * XXX - recalibrate current drive now because some callers
		 * aren't prepared to have its state change.
		 */
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) == 0
		    && wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) == 0
		    && wdsetctlr(du) == 0)
			return (0);
	}
	wderror((struct buf *)NULL, du, "wdunwedge failed");
	return (1);
}

/*
 * Discard any interrupts that were latched by the interrupt system while
 * we were doing polled i/o.
 */
static void wdflushirq(struct disk *du, int old_ipl)
{
	wdtab/*[du->dk_ctrlr]*/.b_active = 2;
	splx(old_ipl);
	(void)splbio();
	wdtab/*[du->dk_ctrlr]*/.b_active = 0;
}

/*
 * Reset the controller.
 */
static int wdreset(struct disk *du)
{
	int	wdc;

	wdc = du->dk_port;
	(void)wdwait(du, 0, TIMEOUT);
	outb(wdc + wd_ctlr, WDCTL_IDS | WDCTL_RST);
	DELAY(10 * 1000);
	outb(wdc + wd_ctlr, WDCTL_IDS);
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0
	    || (du->dk_error = inb(wdc + wd_error)) != 0x01)
		return (1);
	outb(wdc + wd_ctlr, WDCTL_4BIT);
	return (0);
}

static int
wdcwait(struct disk* du, int mask)
{
	const int WDCNDELAY = 100000;
	const int WDCDELAY = 100;
	u_short wdc = du->dk_port;
	int timeout = 0;
	u_char status;

	for (;;) {
		du->dk_status = status = inb(wdc + wd_status);
		if ((status & WDCS_BUSY) == 0 && (status & mask) == mask)
			break;
		if (++timeout > WDCNDELAY)
			return -1;
		DELAY(WDCDELAY);
	}
	if (status & WDCS_ERR) {
		du->dk_error = inb(wdc + wd_error);
		return WDCS_ERR;
	}

	return 0;
}

/*
 * Sleep until driver is inactive.
 * This is used only for avoiding rare race conditions, so it is unimportant
 * that the sleep may be far too short or too long.
 */
static void wdsleep(int ctrlr, char* wmesg)
{
#define PZERO	((48) + 20)
	int s = splbio();
	while (wdtab.b_active)
	{
		tsleep((caddr_t)&wdtab.b_active, PZERO - 1, wmesg, 1);
	}
	splx(s);
}

static struct devif wd_devif =
{
	{0}, -1, -1, 0x38, 3, 7, 0, 0, 0,
	wdopen,	wdclose, wdioctl, 0, 0, 0, 0,
	wdstrategy,	0, wddump, wdsize,
};

DRIVER_MODCONFIG(wd) {
	int nctl;
	char *cfg_string = wd_config;
	
#ifdef nope
	/* find configuration string. */
	if (!config_scan(wd_config, &cfg_string)) 
		return;

	/* configure driver into kernel program */
	if (!spec_config(&cfg_string, &wd_bdevsw, (struct cdevsw *) -1, (struct cdevsw *)0))
		return;
#else

	if (devif_config(&cfg_string, &wd_devif) == 0)
		return;
#endif

	/* allocate driver resources for controllers */
	nctl = 1;
	(void)cfg_number(&cfg_string, &nctl);
#ifdef notyet
	/* ? = malloc(vec[2]*?); */

	/* reserve dkn statistics */

	/* if not root device, postpone hardware configuration till open */
	if ( ... != bdev.bd_major)
#endif

	/* probe for hardware */
	new_isa_configure(&cfg_string, &wddriver);
}
