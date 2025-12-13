/*
 *	$Id: isofs_node.h,v 1.2 1993/07/20 03:27:31 jkh Exp $
 */


typedef	struct	{
	unsigned	iso_cln;	/* Child link */
	unsigned	iso_pln;	/* Parents link */
	struct timeval	iso_atime;	/* time of last access */
	struct timeval	iso_mtime;	/* time of last modification */
	struct timeval	iso_ctime;	/* time file changed */
	u_short		iso_mode;	/* files access mode and type */
	uid_t		iso_uid;	/* owner user id */
	gid_t		iso_gid;	/* owner group id */
	dev_t		iso_dev;	/* if special device, dev number */
	int		iso_links;	/* number of hard links */
} ISO_RRIP_INODE;

struct iso_node {
	struct	iso_node *i_chain[2]; /* hash chain, MUST be first */
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
	struct	iso_mnt *i_mnt;	/* filesystem associated with this inode */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
	long	i_diroff;	/* offset in dir, where we found last entry */
	off_t	i_endoff;	/* end of useful stuff in directory */
	long	i_spare0;
	long	i_spare1;


	int iso_reclen;
	int iso_extlen;
	int iso_extent;
	int i_size;
	int iso_flags;
	int iso_unit_size;
	int iso_interleave_gap;
	int iso_volume_seq;
	int iso_namelen;	/* ISO9660/RRIP name len */
	int iso_parent;		/* byte offset in beginning of dir record */
	int iso_parent_ext;	/* block number of dir record */
	ISO_RRIP_INODE  inode;
	char *iso_sl;		/* symbolic link */
	int iso_sl_len;		/* symbolic link */
};

#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]

/* flags */
#define	ILOCKED		0x0001		/* inode is locked */
#define	IWANT		0x0002		/* some process waiting on lock */
#define	IACC		0x0020		/* inode access time to be updated */

#define VTOI(vp) ((struct iso_node *)(vp)->v_data)
#define ITOV(ip) ((ip)->i_vnode)

#define ISO_ILOCK(ip)	iso_ilock(ip)
#define ISO_IUNLOCK(ip)	iso_iunlock(ip)

extern int	isofs_inactive(struct vnode *, struct proc *);
extern int	isofs_reclaim(struct vnode *);
extern int	iso_iget(struct iso_node *, ino_t, struct iso_node **, struct iso_directory_record *);
extern int	iso_iput(struct iso_node *);
extern int	iso_ilock(struct iso_node *);
extern int	iso_iunlock(struct iso_node *);
extern int	iso_blkatoff(struct iso_node*, off_t, char**, struct buf**);
extern int	iso_bmap(struct iso_node *, int, int *);
extern unsigned char	isonum_711(char *);
extern unsigned int		isonum_712(char *);
extern unsigned short	isonum_721(char *);
extern unsigned short	isonum_722(char *);
extern unsigned short	isonum_723(char *);
extern unsigned int		isonum_731(unsigned char *);
extern unsigned int		isonum_732(unsigned char *);
extern unsigned int		isonum_733(unsigned char *);
extern int	isofncmp(char *, int, char *, short);
extern int	isofntrans(char*, int, char*, short*);

/*
 * Prototypes for ISOFS vnode operations
 */
int isofs_lookup __P((struct vnode *, struct nameidata *, struct proc *));
int isofs_open __P((struct vnode *, int, struct ucred *, struct proc *));
int isofs_close __P((struct vnode *, int, struct ucred *, struct proc *));
int isofs_access __P((struct vnode *, int, struct ucred *, struct proc *));
int isofs_getattr __P((struct vnode *, struct vattr *, struct ucred *, struct proc *));
int isofs_read __P((struct vnode *, struct uio *, int, struct ucred *));
int isofs_ioctl __P((struct vnode *, int, caddr_t, int, struct ucred *, struct proc *));
int isofs_select __P((struct vnode *, int, int, struct ucred *, struct proc *));
int isofs_mmap __P((struct vnode *, int, struct ucred *, struct proc *));
int isofs_seek __P((struct vnode *, off_t, off_t, struct ucred *));
int isofs_readdir __P((struct vnode *, struct uio *, struct ucred *, int *));
int isofs_abortop __P((struct nameidata *));
int isofs_inactive __P((struct vnode *, struct proc *));
int isofs_reclaim __P((struct vnode *));
int isofs_lock __P((struct vnode *));
int isofs_unlock __P((struct vnode *));
int isofs_strategy __P((struct buf *));
int isofs_print __P((struct vnode *));
int isofs_islocked __P((struct vnode *));

