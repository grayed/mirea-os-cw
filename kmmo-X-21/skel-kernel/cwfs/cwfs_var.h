/*	$OpenBSD$	*/

/*
 * Copyright (c) YYYY YOUR NAME HERE <user@your.dom.ain>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file contains definitions of cwfs internals.
 * Everything listed here should not be used in FS-independent code.
 */

// #define	CWFS_DEBUG

#ifdef CWFS_DEBUG
#define d_printf(...)	printf(__VA_ARGS__)
#else
#define d_printf(...)	(void)0
#endif

struct cwfs_node {
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	struct	rrwlock i_lock;
	ino_t	i_number;

	// TODO
	uid_t	i_uid;
	gid_t	i_gid;
	mode_t	i_mode;
	u_quad_t i_size;
	dev_t	i_dev;		/* in case our FS supports devices */
	struct	timespec i_atime;
	struct	timespec i_mtime;
	struct	timespec i_ctime;
};

// #define VTOI(vp) ((struct cwfs_node *)(vp)->v_data)
// #define ITOV(ip) ((ip)->i_vnode)

static inline struct cwfs_node* VTOI(struct vnode *vp) {
	return vp->v_data;
}

static inline struct vnode* ITOV(struct cwfs_node *ip) {
	return ip->i_vnode;
}

struct cwfs_mnt {
	struct	mount *cwm_mountp;
	struct	netexport cwm_export;

	// When mounting to real device, otherwise could be removed
	struct	vnode *cwm_devvp;
	dev_t	cwm_dev;

	// TODO
};

// #define VFSTOCWFS(mp)	((struct cwfs_mnt *)((mp)->mnt_data))

static inline struct cwfs_mnt *VFSTOCWFS(struct mount *mp) {
	return mp->mnt_data;
}

/*
 * Prototypes for CWFS vnode operations
 */
int	cwfs_lookup(void *);
int	cwfs_open(void *);
int	cwfs_close(void *);
int	cwfs_access(void *);
int	cwfs_getattr(void *);
int	cwfs_setattr(void *);
int	cwfs_read(void *);
int	cwfs_ioctl(void *);
int	cwfs_poll(void *);
int	cwfs_mmap(void *);
int	cwfs_seek(void *);
int	cwfs_readdir(void *);
int	cwfs_readlink(void *);
int	cwfs_abortop(void *);
int	cwfs_inactive(void *);
int	cwfs_reclaim(void *);
int	cwfs_link(void *);
int	cwfs_symlink(void *);
int	cwfs_bmap(void *);
int	cwfs_lock(void *);
int	cwfs_unlock(void *);
int	cwfs_strategy(void *);
int	cwfs_print(void *);
int	cwfs_islocked(void *);
int	cwfs_pathconf(void *);

extern const struct vops	cwfs_vops;
extern const struct vops	cwfs_specvops;
#ifdef FIFO
extern const struct vops	cwfs_fifovops;
#endif

/*
 * Prototypes for CWFS mount point operations
 */
int cwfs_mount(struct mount *, const char *, void *,
                      struct nameidata *, struct proc *);
int cwfs_start(struct mount *, int, struct proc *);
int cwfs_unmount(struct mount *, int, struct proc *);
int cwfs_root(struct mount *, struct vnode **);
int cwfs_quotactl(struct mount *, int, uid_t, caddr_t, struct proc *);
int cwfs_statfs(struct mount *, struct statfs *, struct proc *);
int cwfs_sync(struct mount *, int, int, struct ucred *, struct proc *);
int cwfs_vget(struct mount *, ino_t, struct vnode **);
int cwfs_fhtovp(struct mount *, struct fid *, struct vnode **);
int cwfs_vptofh(struct vnode *, struct fid *);
int cwfs_init(struct vfsconf *);
int cwfs_check_export(struct mount *, struct mbuf *, int *,
                             struct ucred **);

#define cwfs_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
                                    size_t, struct proc *))eopnotsupp)
