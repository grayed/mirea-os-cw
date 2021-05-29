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

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/file.h>
#include <sys/ioccom.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/poll.h>
#include <sys/pool.h>
#include <sys/resourcevar.h>
#include <sys/specdev.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <miscfs/fifofs/fifo.h>

#include <cwfs/cwfs_extern.h>
#include <cwfs/cwfs_var.h>

int cwfs_kqfilter(void *v);


/*
 * Setattr call. Only allowed for block and character special devices.
 */
int
cwfs_setattr(void *v)
{
	struct vop_setattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

	if (vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_nsec != VNOVAL ||
	    vap->va_mtime.tv_nsec != VNOVAL || vap->va_mode != (mode_t)VNOVAL ||
	    (vap->va_vaflags & VA_UTIMES_CHANGE))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			return (EROFS);
		case VCHR:
		case VBLK:
		case VSOCK:
		case VFIFO:
			return (0);
		default:
			return (EINVAL);
		}
	}

	return (EINVAL);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
int
cwfs_open(void *v)
{
	// struct vop_open_args *ap = v;
	return (0);
}

/*
 * Close called
 *
 * Update the times on the inode on writeable file systems.
 */
/* ARGSUSED */
int
cwfs_close(void *v)
{
	// TODO struct vop_close_args *ap = v;
	return (0);
}

/*
 * Check mode permission on inode pointer. Mode is READ, WRITE or EXEC.
 * The mode is shifted to select the owner/group/other fields. The
 * super user is granted all permissions.
 */
int
cwfs_access(void *v)
{
	struct vop_access_args *ap = v;
	struct cwfs_node *ip = VTOI(ap->a_vp);

	return (vaccess(ap->a_vp->v_type, ip->i_mode & ALLPERMS,
	    ip->i_uid, ip->i_gid, ap->a_mode, ap->a_cred));
}

int
cwfs_getattr(void *v)
{
	struct vop_getattr_args *ap = v;
	struct vnode *vp = ap->a_vp;
	register struct vattr *vap = ap->a_vap;
	register struct cwfs_node *ip = VTOI(vp);

	vap->va_fsid	= ip->i_dev;
	vap->va_fileid	= ip->i_number;

	// TODO: fill values better way
	vap->va_mode	= /* TODO: full_mode & ALLPERMS */ 0755;
	vap->va_nlink	= 1;
	vap->va_uid	= 0;
	vap->va_gid	= 0;
	vap->va_atime	= ip->i_atime;
	vap->va_mtime	= ip->i_mtime;
	vap->va_ctime	= ip->i_ctime;
	vap->va_rdev	= 0;

	vap->va_size	= (u_quad_t) /* TODO */ 0;
	if (ip->i_size == 0 && vp->v_type  == VLNK) {
		// get length of symbolic link by reading it

		struct vop_readlink_args rdlnk;
		struct iovec aiov;
		struct uio auio;
		char *cp;

		cp = malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
		aiov.iov_base = cp;
		aiov.iov_len = MAXPATHLEN;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = 0;
		auio.uio_rw = UIO_READ;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_procp = ap->a_p;
		auio.uio_resid = MAXPATHLEN;
		rdlnk.a_uio = &auio;
		rdlnk.a_vp = ap->a_vp;
		rdlnk.a_cred = ap->a_cred;
		if (cwfs_readlink(&rdlnk) == 0)
			vap->va_size = MAXPATHLEN - auio.uio_resid;
		free(cp, M_TEMP, 0);
	}
	vap->va_flags	= /* TODO */ 0;
	vap->va_gen	= /* TODO */ 1;
	vap->va_blocksize = /* TODO */ 1;
	vap->va_bytes	= (u_quad_t) /* TODO */ 0;
	vap->va_type	= vp->v_type;
	return (0);
}

/*
 * Vnode op for reading.
 */
int
cwfs_read(void *v)
{
	struct vop_read_args *ap = v;
	struct vnode *vp = ap->a_vp;
	register struct uio *uio = ap->a_uio;		// I/O request & result
	register struct cwfs_node *ip = VTOI(vp);
	register struct cwfs_mnt *cwmp;
	int error = 0;

	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	nanotime(&ip->i_atime);
	cwmp = VFSTOCWFS(ITOV(ip)->v_mount);

	// TODO

	return (error);
}

/* ARGSUSED */
int
cwfs_ioctl(void *v)
{
	return (ENOTTY);
}

/* ARGSUSED */
int
cwfs_poll(void *v)
{
	struct vop_poll_args *ap = v;

	/*
	 * We should really check to see if I/O is possible.
	 */
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
int
cwfs_mmap(void *v)
{
	return (EINVAL);
}

/*
 * Seek on a file
 *
 * Nothing to do, so just return.
 */
/* ARGSUSED */
int
cwfs_seek(void *v)
{
	return (0);
}

/*
 * Vnode op for readdir
 */
int
cwfs_readdir(void *v)
{
	struct vop_readdir_args *ap = v;
	register struct uio *uio = ap->a_uio;
	struct vnode *vdp = ap->a_vp;
	struct cwfs_node *dp;
	struct cwfs_mnt *cwmp;
	struct buf *bp = NULL;
	int error = 0;

	dp = VTOI(vdp);
	cwmp = VFSTOCWFS(vdp->v_mount);

	// TODO

	if (bp)
		brelse (bp);

	uio->uio_offset = /* TODO */ 0;
	*ap->a_eofflag = /* TODO */ 0;

	return (error);
}

/*
 * Return target name of a symbolic link
 */
int
cwfs_readlink(void *v)
{
	struct vop_readlink_args *ap = v;
	struct cwfs_node	*ip;

	ip  = VTOI(ap->a_vp);

	// TODO

	return (0);
}

int
cwfs_link(void *v)
{
	struct vop_link_args *ap = v;

	/* TODO the code below works for read-only FS */
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

int
cwfs_symlink(void *v)
{
	struct vop_symlink_args *ap = v;

	/* TODO */
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

/*
 * Lock an inode.
 */
int
cwfs_lock(void *v)
{
	struct vop_lock_args *ap = v;
	struct vnode *vp = ap->a_vp;

	return rrw_enter(&VTOI(vp)->i_lock, ap->a_flags & LK_RWFLAGS);
}

/*
 * Unlock an inode.
 */
int
cwfs_unlock(void *v)
{
	struct vop_unlock_args *ap = v;
	struct vnode *vp = ap->a_vp;

	rrw_exit(&VTOI(vp)->i_lock);
	return 0;
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
cwfs_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct buf *bp = ap->a_bp;
	struct vnode *vp = bp->b_vp;
	struct cwfs_node *ip;
	int error;
	int s;

	ip = VTOI(vp);
	if (vp->v_type == VBLK || vp->v_type == VCHR)
		panic("cwfs_strategy: spec");
	if (bp->b_blkno == bp->b_lblkno) {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			s = splbio();
			biodone(bp);
			splx(s);
			return (error);
		}
		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}
	if ((long)bp->b_blkno == -1) {
		s = splbio();
		biodone(bp);
		splx(s);
		return (0);
	}
	vp = ip->i_devvp;
	bp->b_dev = vp->v_rdev;
	(vp->v_op->vop_strategy)(ap);
	return (0);
}

/*
 * Print out the contents of an inode.
 */
/*ARGSUSED*/
int
cwfs_print(void *v)
{
	printf("tag VT_CWFS, cwfs vnode\n");
	return (0);
}

/*
 * Check for a locked inode.
 */
int
cwfs_islocked(void *v)
{
	struct vop_islocked_args *ap = v;

	return rrw_status(&VTOI(ap->a_vp)->i_lock);
}

/*
 * Return POSIX pathconf information applicable to cwfs filesystems.
 */
int
cwfs_pathconf(void *v)
{
	struct vop_pathconf_args *ap = v;
	int error = 0;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = /* TODO */ 1;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = /* TODO */ NAME_MAX;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = /* TODO */ 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = /* TODO */ 1;
		break;
	case _PC_TIMESTAMP_RESOLUTION:
		*ap->a_retval = /* TODO */ 1000000000;	/* one billion nanoseconds */
		break;
	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * Bmap converts a the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the data block (extent) for the file.
 */
int
cwfs_bmap(void *v)
{
	struct vop_bmap_args *ap = v;
	struct cwfs_node *ip = VTOI(ap->a_vp);
	daddr_t lblkno = ap->a_bn;
	int bshift;

	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ip->i_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	/*
	 * Compute the requested block number
	 */
	bshift = /* TODO */ DEV_BSHIFT;
	*ap->a_bnp = /* TODO */ 0;

	/*
	 * Determine maximum number of readahead blocks following the
	 * requested block.
	 */
	if (ap->a_runp) {
		int nblk;

		nblk = (ip->i_size >> bshift) - (lblkno + 1);
		if (nblk <= 0)
			*ap->a_runp = 0;
		else if (nblk >= (MAXBSIZE >> bshift))
			*ap->a_runp = (MAXBSIZE >> bshift) - 1;
		else
			*ap->a_runp = nblk;
	}

	return (0);
}

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and iput
 * instead of two iputs.
 *
 * Overall outline of lookup for normal file system:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent inode unlocked.
 */
int
cwfs_lookup(v)
	void *v;
{
	struct vop_lookup_args *ap = v;
	register struct vnode *vdp;	/* vnode for directory being searched */
	register struct cwfs_node *dp;	/* inode for directory being searched */
	register struct cwfs_mnt *cwmp;	/* file system that directory is in */
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int error;
	int flags;

	cnp->cn_flags &= ~PDIRUNLOCK;
	flags = cnp->cn_flags;

	*vpp = NULL;
	vdp = ap->a_dvp;
	dp = VTOI(vdp);
	cwmp = VFSTOCWFS(vdp->v_mount);

	if ((error = VOP_ACCESS(vdp, VEXEC, cred, cnp->cn_proc)) != 0)
		return (error);

	if ((flags & ISLASTCN) && (vdp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	if ((error = cache_lookup(vdp, vpp, cnp)) >= 0)
		return (error);

	// TODO

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
	return (0);
}


/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
cwfs_reclaim(void *v)
{
	struct vop_reclaim_args *ap = v;
	register struct vnode *vp = ap->a_vp;
	register struct cwfs_node *ip = VTOI(vp);

#ifdef DIAGNOSTIC
	if (vp->v_usecount != 0)
		vprint("cwfs_reclaim: pushing active", vp);
#endif

	// TODO

	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}
	free(ip, M_CWFSNODE, sizeof(*ip));
	vp->v_data = NULL;
	return (0);
}

/*
 * Last reference to an inode, write the inode out and if necessary,
 * truncate and deallocate the file.
 */
int
cwfs_inactive(void *v)
{
	struct vop_inactive_args *ap = v;
	struct vnode *vp = ap->a_vp;
	int error = 0;

#ifdef DIAGNOSTIC
	if (vp->v_usecount != 0)
		vprint("cwfs_inactive: pushing active", vp);
#endif

	VOP_UNLOCK(vp);
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (/* TODO */ 1)
		vrecycle(vp, ap->a_p);

	return (error);
}

/*
 * Global vfs data structures for cwfs
 */
#define	cwfs_create	eopnotsupp
#define	cwfs_mknod	eopnotsupp
#define	cwfs_write	eopnotsupp
#define	cwfs_fsync	nullop
#define	cwfs_remove	eopnotsupp
#define	cwfs_rename	eopnotsupp
#define	cwfs_mkdir	eopnotsupp
#define	cwfs_rmdir	eopnotsupp
#define	cwfs_advlock	eopnotsupp
#define	cwfs_valloc	eopnotsupp
#define	cwfs_vfree	eopnotsupp
#define	cwfs_truncate	eopnotsupp
#define	cwfs_update	eopnotsupp
#define	cwfs_bwrite	eopnotsupp
#define cwfs_revoke	vop_generic_revoke

/* Global vfs data structures for cwfs. */
const struct vops cwfs_vops = {
	.vop_lookup	= cwfs_lookup,
	.vop_create	= cwfs_create,
	.vop_mknod	= cwfs_mknod,
	.vop_open	= cwfs_open,
	.vop_close	= cwfs_close,
	.vop_access	= cwfs_access,
	.vop_getattr	= cwfs_getattr,
	.vop_setattr	= cwfs_setattr,
	.vop_read	= cwfs_read,
	.vop_write	= cwfs_write,
	.vop_ioctl	= cwfs_ioctl,
	.vop_poll	= cwfs_poll,
	.vop_kqfilter	= cwfs_kqfilter,
	.vop_revoke	= cwfs_revoke,
	.vop_fsync	= cwfs_fsync,
	.vop_remove	= cwfs_remove,
	.vop_link	= cwfs_link,
	.vop_rename	= cwfs_rename,
	.vop_mkdir	= cwfs_mkdir,
	.vop_rmdir	= cwfs_rmdir,
	.vop_symlink	= cwfs_symlink,
	.vop_readdir	= cwfs_readdir,
	.vop_readlink	= cwfs_readlink,
	.vop_abortop	= vop_generic_abortop,
	.vop_inactive	= cwfs_inactive,
	.vop_reclaim	= cwfs_reclaim,
	.vop_lock	= cwfs_lock,
	.vop_unlock	= cwfs_unlock,
	.vop_bmap	= cwfs_bmap,
	.vop_strategy	= cwfs_strategy,
	.vop_print	= cwfs_print,
	.vop_islocked	= cwfs_islocked,
	.vop_pathconf	= cwfs_pathconf,
	.vop_advlock	= cwfs_advlock,
	.vop_bwrite	= vop_generic_bwrite
};

/* Special device vnode ops */
const struct vops cwfs_specvops = {
	.vop_access	= cwfs_access,
	.vop_getattr	= cwfs_getattr,
	.vop_setattr	= cwfs_setattr,
	.vop_inactive	= cwfs_inactive,
	.vop_reclaim	= cwfs_reclaim,
	.vop_lock	= cwfs_lock,
	.vop_unlock	= cwfs_unlock,
	.vop_print	= cwfs_print,
	.vop_islocked	= cwfs_islocked,

	/* XXX: Keep in sync with spec_vops. */
	.vop_lookup	= vop_generic_lookup,
	.vop_create	= spec_badop,
	.vop_mknod	= spec_badop,
	.vop_open	= spec_open,
	.vop_close	= spec_close,
	.vop_read	= spec_read,
	.vop_write	= spec_write,
	.vop_ioctl	= spec_ioctl,
	.vop_poll	= spec_poll,
	.vop_kqfilter	= spec_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_fsync	= spec_fsync,
	.vop_remove	= spec_badop,
	.vop_link	= spec_badop,
	.vop_rename	= spec_badop,
	.vop_mkdir	= spec_badop,
	.vop_rmdir	= spec_badop,
	.vop_symlink	= spec_badop,
	.vop_readdir	= spec_badop,
	.vop_readlink	= spec_badop,
	.vop_abortop	= spec_badop,
	.vop_bmap	= vop_generic_bmap,
	.vop_strategy	= spec_strategy,
	.vop_pathconf	= spec_pathconf,
	.vop_advlock	= spec_advlock,
	.vop_bwrite	= vop_generic_bwrite,
};

#ifdef FIFO
const struct vops cwfs_fifovops = {
	.vop_access	= cwfs_access,
	.vop_getattr	= cwfs_getattr,
	.vop_setattr	= cwfs_setattr,
	.vop_inactive	= cwfs_inactive,
	.vop_reclaim	= cwfs_reclaim,
	.vop_lock	= cwfs_lock,
	.vop_unlock	= cwfs_unlock,
	.vop_print	= cwfs_print,
	.vop_islocked	= cwfs_islocked,
	.vop_bwrite	= vop_generic_bwrite,

	/* XXX: Keep in sync with fifo_vops. */
	.vop_lookup	= vop_generic_lookup,
	.vop_create	= fifo_badop,
	.vop_mknod	= fifo_badop,
	.vop_open	= fifo_open,
	.vop_close	= fifo_close,
	.vop_read	= fifo_read,
	.vop_write	= fifo_write,
	.vop_ioctl	= fifo_ioctl,
	.vop_poll	= fifo_poll,
	.vop_kqfilter	= fifo_kqfilter,
	.vop_revoke	= vop_generic_revoke,
	.vop_fsync	= nullop,
	.vop_remove	= fifo_badop,
	.vop_link	= fifo_badop,
	.vop_rename	= fifo_badop,
	.vop_mkdir	= fifo_badop,
	.vop_rmdir	= fifo_badop,
	.vop_symlink	= fifo_badop,
	.vop_readdir	= fifo_badop,
	.vop_readlink	= fifo_badop,
	.vop_abortop	= fifo_badop,
	.vop_bmap	= vop_generic_bmap,
	.vop_strategy	= fifo_badop,
	.vop_pathconf	= fifo_pathconf,
	.vop_advlock	= fifo_advlock,
};
#endif /* FIFO */

void filt_cwfsdetach(struct knote *kn);
int filt_cwfsread(struct knote *kn, long hint);
int filt_cwfswrite(struct knote *kn, long hint);
int filt_cwfsvnode(struct knote *kn, long hint);

const struct filterops cwfsread_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_cwfsdetach,
	.f_event	= filt_cwfsread,
};

const struct filterops cwfswrite_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_cwfsdetach,
	.f_event	= filt_cwfswrite,
};

const struct filterops cwfsvnode_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_cwfsdetach,
	.f_event	= filt_cwfsvnode,
};

int
cwfs_kqfilter(void *v)
{
	struct vop_kqfilter_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct knote *kn = ap->a_kn;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &cwfsread_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &cwfswrite_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &cwfsvnode_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)vp;

	klist_insert(&vp->v_selectinfo.si_note, kn);

	return (0);
}

void
filt_cwfsdetach(struct knote *kn)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;

	klist_remove(&vp->v_selectinfo.si_note, kn);
}

int
filt_cwfsread(struct knote *kn, long hint)
{
	struct vnode *vp = (struct vnode *)kn->kn_hook;
	struct cwfs_node *node = VTOI(vp);

	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

	kn->kn_data = node->i_size - foffset(kn->kn_fp);
	if (kn->kn_data == 0 && kn->kn_sfflags & NOTE_EOF) {
		kn->kn_fflags |= NOTE_EOF;
		return (1);
	}

	if (kn->kn_flags & __EV_POLL)
		return (1);

	return (kn->kn_data != 0);
}

int
filt_cwfswrite(struct knote *kn, long hint)
{
	/*
	 * filesystem is gone, so set the EOF flag and schedule 
	 * the knote for deletion.
	 */
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}

	kn->kn_data = 0;
	return (1);
}

int
filt_cwfsvnode(struct knote *kn, long hint)
{
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	return (kn->kn_fflags != 0);
}
