/*
 * System call switch table  XXX (holdover from 0.1).
 */

#include "sys/param.h"
#include "systm.h"

int	nosys();
int	nullop();

int	rexit();
int	fork();
int	read();
int	write();
int	open();
int	close();
int	wait4();
int	link();
int	unlink();
int	chdir();
int	fchdir();
int	mknod();
int	chmod();
int	chown();
int	obreak();
int	getfsstat();
int	lseek();
int	getpid();
int	mount();
int	unmount();
int	setuid();
int	getuid();
int	geteuid();
int	ptrace();
int	recvmsg();
int	sendmsg();
int	recvfrom();
int	accept();
int	getpeername();
int	getsockname();
int	saccess();
int	chflags();
int	fchflags();
int	sync();
int	kill();
int	stat();
int	getppid();
int	lstat();
int	dup();
int	pipe();
int	getegid();
int	profil();
#ifdef KTRACE
int	ktrace();
#endif
int	sigaction();
int	getgid();
int	sigprocmask();
int	getlogin();
int	setlogin();
int	sigpending();
#ifdef notyet
int	sigaltstack();
#endif
int	ioctl();
int	reboot();
int	revoke();
int	symlink();
int	readlink();
int	execve();
int	umask();
int	chroot();
int	fstat();
int	getkerninfo();
int	getpagesize();
int	msync();
int	vfork();
int	sstk();
int	smmap();
int	ovadvise();
int	munmap();
int	mprotect();
int	madvise();
int	mincore();
int	getgroups();
int	setgroups();
int	getpgrp();
int	setpgid();
int	setitimer();
int	swapon();
int	getitimer();
int	gethostname();
int	sethostname();
int	getdtablesize();
int	dup2();
int	fcntl();
int	select();
int	fsync();
int	setpriority();
int	socket();
int	connect();
int	getpriority();
int	sigreturn();
int	bind();
int	setsockopt();
int	listen();
int	sigsuspend();
int	sigstack();
int	gettimeofday();
int	getrusage();
int	getsockopt();
int	readv();
int	writev();
int	settimeofday();
int	fchown();
int	fchmod();
int	rename();
int	truncate();
int	ftruncate();
int	flock();
int	mkfifo();
int	sendto();
int	shutdown();
int	socketpair();
int	mkdir();
int	rmdir();
int	utimes();
int	adjtime();
int	gethostid();
int	sethostid();
int	getrlimit();
int	setrlimit();
int	setsid();
int	quotactl();
#ifdef NFS
int	nfssvc();
#endif
int	getdirentries();
int	statfs();
int	fstatfs();
#ifdef NFS
int	async_daemon();
#endif
int	getfh();
int	setgid();
int	setegid();
int	seteuid();

#define compat(n, name) 0, nosys
#define ncompat(n, name) 0, nosys

struct sysent sysent[] = {
	0, nosys,				/* 0 = indir or out-of-range */
	1, rexit,				/* 1 = exit */
	0, fork,				/* 2 = fork */
	3, read,				/* 3 = read */
	3, write,				/* 4 = write */
	3, open,				/* 5 = open */
	1, close,				/* 6 = close */
	4, wait4,				/* 7 = wait4 */
	compat(2,creat),		/* 8 = old creat */
	2, link,				/* 9 = link */
	1, unlink,				/* 10 = unlink */
	0, nosys,				/* 11 = obsolete execv */
	1, chdir,				/* 12 = chdir */
	1, fchdir,				/* 13 = fchdir */
	3, mknod,				/* 14 = mknod */
	2, chmod,				/* 15 = chmod */
	3, chown,				/* 16 = chown */
	1, obreak,				/* 17 = break */
	3, getfsstat,			/* 18 = getfsstat */
	3, lseek,				/* 19 = lseek */
	0, getpid,				/* 20 = getpid */
	4, mount,				/* 21 = mount */
	2, unmount,				/* 22 = unmount */
	1, setuid,				/* 23 = setuid */
	0, getuid,				/* 24 = getuid */
	0, geteuid,				/* 25 = geteuid */
	4, ptrace,				/* 26 = ptrace */
	3, recvmsg,				/* 27 = recvmsg */
	3, sendmsg,				/* 28 = sendmsg */
	6, recvfrom,			/* 29 = recvfrom */
	3, accept,				/* 30 = accept */
	3, getpeername,			/* 31 = getpeername */
	3, getsockname,			/* 32 = getsockname */
	2, saccess,				/* 33 = access */
	2, chflags,				/* 34 = chflags */
	2, fchflags,			/* 35 = fchflags */
	0, sync,				/* 36 = sync */
	2, kill,				/* 37 = kill */
	2, stat,				/* 38 = stat */
	0, getppid,				/* 39 = getppid */
	2, lstat,				/* 40 = lstat */
	2, dup,					/* 41 = dup */
	0, pipe,				/* 42 = pipe */
	0, getegid,				/* 43 = getegid */
	4, profil,				/* 44 = profil */
#ifdef KTRACE
	4, ktrace,				/* 45 = ktrace */
#else
	0, nosys,				/* 45 = ktrace */
#endif
	3, sigaction,			/* 46 = sigaction */
	0, getgid,				/* 47 = getgid */
	2, sigprocmask,			/* 48 = sigprocmask */
	2, getlogin,			/* 49 = getlogin */
	1, setlogin,			/* 50 = setlogin */
	0, nosys,				/* 51 = acct */
	0, sigpending,			/* 52 = sigpending */
#ifdef notyet
	3, sigaltstack,			/* 53 = sigaltstack */
#else
	0, nosys,				/* 53 = sigaltstack */
#endif
	3, ioctl,				/* 54 = ioctl */
	1, reboot,				/* 55 = reboot */
	1, revoke,				/* 56 = revoke */
	2, symlink,				/* 57 = symlink */
	3, readlink,			/* 58 = readlink */
	3, execve,				/* 59 = execve */
	1, umask,				/* 60 = umask */
	1, chroot,				/* 61 = chroot */
	2, fstat,				/* 62 = fstat */
	4, getkerninfo,			/* 63 = getkerninfo */
	0, getpagesize,			/* 64 = getpagesize */
	2, msync,				/* 65 = msync */
	0, vfork,				/* 66 = vfork */
	0, nosys,				/* 67 = obsolete vread */
	0, nosys,				/* 68 = obsolete vwrite */
	0, nosys,				/* 69 = obselete sbrk */
	1, sstk,				/* 70 = sstk */
	6, smmap,				/* 71 = mmap */
	1, ovadvise,			/* 72 = vadvise */
	2, munmap,				/* 73 = munmap */
	3, mprotect,			/* 74 = mprotect */
	3, madvise,				/* 75 = madvise */
	0, nosys,				/* 76 = obsolete vhangup */
	0, nosys,				/* 77 = obsolete vlimit */
	3, mincore,				/* 78 = mincore */
	2, getgroups,			/* 79 = getgroups */
	2, setgroups,			/* 80 = setgroups */
	1, getpgrp,				/* 81 = getpgrp */
	2, setpgid,				/* 82 = setpgid */
	3, setitimer,			/* 83 = setitimer */
	compat(0,wait),			/* 84 = old wait */
	1, swapon,				/* 85 = swapon */
	2, getitimer,			/* 86 = getitimer */
	2, gethostname,			/* 87 = gethostname */
	2, sethostname,			/* 88 = sethostname */
	0, getdtablesize,		/* 89 = getdtablesize */
	2, dup2,				/* 90 = dup2 */
	0, nosys,				/* 91 = getdopt */
	3, fcntl,				/* 92 = fcntl */
	5, select,				/* 93 = select */
	0, nosys,				/* 94 = setdopt */
	1, fsync,				/* 95 = fsync */
	3, setpriority,			/* 96 = setpriority */
	3, socket,				/* 97 = socket */
	3, connect,				/* 98 = connect */
	compat(3,accept),		/* 99 = old accept */
	2, getpriority,			/* 100 = getpriority */
	compat(4,send),			/* 101 = old send */
	compat(4,recv),			/* 102 = old recv */
	1, sigreturn,			/* 103 = sigreturn */
	3, bind,				/* 104 = bind */
	5, setsockopt,			/* 105 = setsockopt */
	2, listen,				/* 106 = listen */
	0, nosys,				/* 107 = obsolete vtimes */
	ncompat(3,sigvec),		/* 108 = old sigvec */
	ncompat(1,sigblock),	/* 109 = old sigblock */
	ncompat(1,sigsetmask),	/* 110 = old sigsetmask */
	1, sigsuspend,			/* 111 = sigsuspend */
	2, sigstack,			/* 112 = sigstack */
	compat(3,recvmsg),		/* 113 = old recvmsg */
	compat(3,sendmsg),		/* 114 = old sendmsg */
	0, nosys,				/* 115 = obsolete vtrace */
	2, gettimeofday,		/* 116 = gettimeofday */
	2, getrusage,			/* 117 = getrusage */
	5, getsockopt,			/* 118 = getsockopt */
	0, nosys,				/* 119 = nosys */
	3, readv,				/* 120 = readv */
	3, writev,				/* 121 = writev */
	2, settimeofday,		/* 122 = settimeofday */
	3, fchown,				/* 123 = fchown */
	2, fchmod,				/* 124 = fchmod */
	compat(6,recvfrom),		/* 125 = old recvfrom */
	/* compat(2,setreuid), */	/* 126 = old setreuid */
	2, nullop ,				/* 126 = old setreuid */
	/* 2, osetreuid , */		/* 126 = old setreuid */
	compat(2,setregid),		/* 127 = old setregid */
	/* 2, osetregid , */		/* 127 = old setregid */
	2, rename,				/* 128 = rename */
	2, truncate,			/* 129 = truncate */
	2, ftruncate,			/* 130 = ftruncate */
	2, flock,				/* 131 = flock */
	2, mkfifo,				/* 132 = mkfifo */
	6, sendto,				/* 133 = sendto */
	2, shutdown,			/* 134 = shutdown */
	5, socketpair,			/* 135 = socketpair */
	2, mkdir,				/* 136 = mkdir */
	1, rmdir,				/* 137 = rmdir */
	2, utimes,				/* 138 = utimes */
	0, nosys,				/* 139 = obsolete 4.2 sigreturn */
	2, adjtime,				/* 140 = adjtime */
	compat(3,getpeername),	/* 141 = old getpeername */
	0, gethostid,			/* 142 = gethostid */
	1, sethostid,			/* 143 = sethostid */
	2, getrlimit,			/* 144 = getrlimit */
	2, setrlimit,			/* 145 = setrlimit */
	ncompat(2,killpg),		/* 146 = old killpg */
	0, setsid,				/* 147 = setsid */
	4, quotactl,			/* 148 = quotactl */
	compat(4,quota),		/* 149 = old quota */
	compat(3,getsockname),	/* 150 = old getsockname */
	0, nosys,				/* 151 = nosys */
	0, nosys,				/* 152 = nosys */
	0, nosys,				/* 153 = nosys */
	0, nosys,				/* 154 = nosys */
#ifdef NFS
	5, nfssvc,				/* 155 = nfssvc */
#else
	0, nosys,				/* 155 = nosys */
#endif
	4, getdirentries,		/* 156 = getdirentries */
	2, statfs,				/* 157 = statfs */
	2, fstatfs,				/* 158 = fstatfs */
	0, nosys,				/* 159 = nosys */
#ifdef NFS
	0, async_daemon,		/* 160 = async_daemon */
#else
	0, nosys,				/* 160 = nosys */
#endif
	2, getfh,				/* 161 = getfh */
	0, nosys,				/* 162 = nosys */
	0, nosys,				/* 163 = nosys */
	0, nosys,				/* 164 = nosys */
	0, nosys,				/* 165 = nosys */
	0, nosys,				/* 166 = nosys */
	0, nosys,				/* 167 = nosys */
	0, nosys,				/* 168 = nosys */
	0, nosys,				/* 169 = nosys */
	0, nosys,				/* 170 = nosys */
	0, nosys,				/* 171 = nosys */
	0, nosys,				/* 172 = nosys */
	0, nosys,				/* 173 = nosys */
	0, nosys,				/* 174 = nosys */
	0, nosys,				/* 175 = nosys */
	0, nosys,				/* 176 = nosys */
	0, nosys,				/* 177 = nosys */
	0, nosys,				/* 178 = nosys */
	0, nosys,				/* 179 = nosys */
	0, nosys,				/* 180 = nosys */
	1, setgid,				/* 181 = setgid */
	1, setegid,				/* 182 = setegid */
	1, seteuid,				/* 183 = seteuid */
	0, nosys,				/* 184 = nosys */
	0, nosys,				/* 185 = nosys */
	0, nosys,				/* 186 = nosys */
	0, nosys,				/* 187 = nosys */
	0, nosys,				/* 188 = nosys */
	0, nosys,				/* 189 = nosys */
	0, nosys,				/* 190 = nosys */
};

int	nsysent = sizeof(sysent) / sizeof(sysent[0]);
