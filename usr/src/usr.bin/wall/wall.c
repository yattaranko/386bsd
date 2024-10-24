/*
 * Copyright (c) 1988, 1990 Regents of the University of California.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)wall.c	5.14 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * This program is not related to David Wall, whose Stanford Ph.D. thesis
 * is entitled "Mechanisms for Broadcast and Selective Broadcast".
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <utmp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>

#define	IGNOREUSER	"sleeper"

int nobanner;
int mbufsize;
char *mbuf;

static void makemsg(char* fname);
int _mkstemp(char* path);

/* ARGSUSED */
int main(int argc, char** argv)
{
	extern int optind;
	int ch;
	struct iovec iov;
	struct utmp utmp;
	FILE *fp;
	char *p, *ttymsg();

fprintf(stderr, "main1\n");
	while ((ch = getopt(argc, argv, "n")) != EOF)
	{
		switch (ch)
		{
		case 'n':
			/* undoc option for shutdown: suppress banner */
			if (geteuid() == 0)
				nobanner = 1;
			break;
		case '?':
		default:
usage:
			(void)fprintf(stderr, "usage: wall [file]\n");
			exit(1);
		}
	}
fprintf(stderr, "main2\n");
	argc -= optind;
	argv += optind;
	if (argc > 1)
		goto usage;

	makemsg(*argv);

fprintf(stderr, "main3\n");
	if (!(fp = fopen(_PATH_UTMP, "r"))) {
		(void)fprintf(stderr, "wall: cannot read %s.\n", _PATH_UTMP);
		exit(1);
	}
fprintf(stderr, "main4\n");
	iov.iov_base = mbuf;
	iov.iov_len = mbufsize;
	/* NOSTRICT */
	while (fread((char *)&utmp, sizeof(utmp), 1, fp) == 1) {
		if (!utmp.ut_name[0] ||
		    !strncmp(utmp.ut_name, IGNOREUSER, sizeof(utmp.ut_name)))
			continue;
		if (p = ttymsg(&iov, 1, utmp.ut_line))
			(void)fprintf(stderr, "wall: %s\n", p);
	}
	exit(0);
}

static void makemsg(char* fname)
{
	register int ch, cnt;
	struct tm *lt;
	struct passwd *pw;
	struct stat sbuf;
	time_t now, time();
	FILE *fp;
	int fd;
	char *p, *whom, hostname[MAXHOSTNAMELEN], lbuf[100], tmpname[17];
	char *getlogin(), *strcpy(), *ttyname();

	(void)strcpy(tmpname, _PATH_TMP);
	(void)strcat(tmpname, "wall.XXXXXX");
	if (!(fd = _mkstemp(tmpname)) || !(fp = fdopen(fd, "r+"))) {
		(void)fprintf(stderr, "wall: can't open temporary file.\n");
		exit(1);
	}
	(void)unlink(tmpname);

	if (!nobanner) {
		if (!(whom = getlogin()))
			whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";
fprintf("whom = %s\n", whom);
		(void)gethostname(hostname, sizeof(hostname));
		(void)time(&now);
		lt = localtime(&now);

		/*
		 * all this stuff is to blank out a square for the message;
		 * we wrap message lines at column 79, not 80, because some
		 * terminals wrap after 79, some do not, and we can't tell.
		 * Which means that we may leave a non-blank character
		 * in column 80, but that can't be helped.
		 */
		(void)fprintf(fp, "\r%79s\r\n", " ");
		(void)sprintf(lbuf, "Broadcast Message from %s@%s",
		    whom, hostname);
		(void)fprintf(fp, "%-79.79s\007\007\r\n", lbuf);
		(void)sprintf(lbuf, "        (%s) at %d:%02d ...", ttyname(2),
		    lt->tm_hour, lt->tm_min);
		(void)fprintf(fp, "%-79.79s\r\n", lbuf);
	}
	(void)fprintf(fp, "%79s\r\n", " ");

	if (*fname && !(freopen(fname, "r", stdin))) {
		(void)fprintf(stderr, "wall: can't read %s.\n", fname);
		exit(1);
	}
	while (fgets(lbuf, sizeof(lbuf), stdin))
		for (cnt = 0, p = lbuf; ch = *p; ++p, ++cnt) {
			if (cnt == 79 || ch == '\n') {
				for (; cnt < 79; ++cnt)
					putc(' ', fp);
				putc('\r', fp);
				putc('\n', fp);
				cnt = 0;
			} else
				putc(ch, fp);
		}
	(void)fprintf(fp, "%79s\r\n", " ");
	rewind(fp);

	if (fstat(fd, &sbuf)) {
		(void)fprintf(stderr, "wall: can't stat temporary file.\n");
		exit(1);
	}
	mbufsize = sbuf.st_size;
	if (!(mbuf = malloc((u_int)mbufsize))) {
		(void)fprintf(stderr, "wall: out of memory.\n");
		exit(1);
	}
	if (fread(mbuf, sizeof(*mbuf), mbufsize, fp) != mbufsize) {
		(void)fprintf(stderr, "wall: can't read temporary file.\n");
		exit(1);
	}
	(void)close(fd);
}


#include <sys/fcntl.h>
#include "sys/errno.h"
static int _gettemp(char *path, register int *doopen);

int _mkstemp(char* path)
{
	int fd;

	return (_gettemp(path, &fd) ? fd : -1);
}

static int _gettemp(char *path, register int *doopen)
{
	extern int errno;
	register char *start, *trv;
	struct stat sbuf;
	u_int pid;

	pid = getpid();
fprintf(stderr, "path = %s, pid = %d, ", path, pid);
	for (trv = path; *trv; ++trv);		/* extra X's get set to 0's */
	while (*--trv == 'X') {
		*trv = (pid % 10) + '0';
		pid /= 10;
	}
fprintf(stderr, "path = %s, trv = %s\n", path, trv);

	/*
	 * check the target directory; if you have six X's and it
	 * doesn't exist this runs for a *very* long time.
	 */
	for (start = trv + 1;; --trv) {
		if (trv <= path)
			break;
		if (*trv == '/') {
			*trv = '\0';
			if (stat(path, &sbuf))
				return(0);
			if (!S_ISDIR(sbuf.st_mode)) {
				errno = ENOTDIR;
				return(0);
			}
			*trv = '/';
			break;
		}
	}

	for (;;)
	{
		if (doopen) {
fprintf(stderr, "path = %s\n", path);
			if ((*doopen = open(path, O_CREAT|O_EXCL|O_RDWR, 0600)) >= 0)
			{
fprintf(stderr, "*doopen = %x\n", *doopen);
				return(1);
			}
fprintf(stderr, "*doopen = %x\n", *doopen);
			if (errno != EEXIST)
			{
				return(0);
			}
		}
		else
		{
fprintf(stderr, "stat\n");
			if (stat(path, &sbuf))
			{
				return(errno == ENOENT ? 1 : 0);
			}
		}
		/* tricky little algorithm for backward compatibility */
		for (trv = start;;) {
			if (!*trv)
				return(0);
			if (*trv == 'z')
				*trv++ = 'a';
			else {
				if (isdigit(*trv))
					*trv = 'a';
				else
					++*trv;
				break;
			}
		}
	}
	/*NOTREACHED*/
}
