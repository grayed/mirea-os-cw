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
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/lock.h>
#include <sys/specdev.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/stat.h>

#include <cwfs/cwfs_extern.h>
#include <cwfs/cwfs_var.h>

const struct vfsops cwfs_vfsops = {
	.vfs_mount	= cwfs_mount,
	.vfs_start	= cwfs_start,
	.vfs_unmount	= cwfs_unmount,
	.vfs_root	= cwfs_root,
	.vfs_quotactl	= cwfs_quotactl,
	.vfs_statfs	= cwfs_statfs,
	.vfs_sync	= cwfs_sync,
	.vfs_vget	= cwfs_vget,
	.vfs_fhtovp	= cwfs_fhtovp,
	.vfs_vptofh	= cwfs_vptofh,
	.vfs_init	= cwfs_init,
	.vfs_sysctl	= cwfs_sysctl,
	.vfs_checkexp	= cwfs_check_export,
};

static int	cwfs_mountfs(register struct vnode *, struct mount *, struct proc *, struct cwfs_args *);

static int	cwfs_vget_internal(struct mount *, ino_t, struct vnode **);

int	cwfs_debug;

/*
 * VFS Operations
 */

/*
 * Called once at kernel startup
 */
int
cwfs_init(struct vfsconf *vfsp) {
	// TODO: perform any initial setup here, like global variables initialization
	return (0);
}

/*
 * mount system call
 */
int
cwfs_mount(register struct mount *mp,
	   const char *path,		// path to directory where to mount this FS
	   void *data,			// mount parameters (FS-specific)
	   struct nameidata *ndp,	// describes mount source, see namei(9)
	   struct proc *p)		// the calling userspace process
{
	struct cwfs_mnt *cwmp = NULL;
	struct cwfs_args *args = data;
	struct vnode *devvp;		// vnode of device containing our FS
	char fspec[MNAMELEN];
	int error;

	// TODO uncomment the following for read-only file system
	// if ((mp->mnt_flag & MNT_RDONLY) == 0)
	//	return (EROFS);

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		cwmp = VFSTOCWFS(mp);
		if (args && args->cwa_fspec == NULL)
			return (vfs_export(mp, &cwmp->cwm_export,
			    &args->cwa_export_info));
		return (0);
	}

// TODO uncomment if willing to mount on real device
#if 0
	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	error = copyinstr(args->cwa_fspec, fspec, sizeof(fspec), NULL);
	if (error)
		return (error);
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_SYSSPACE, fspec, p);
	if ((error = namei(ndp)) != 0)
		return (error);

	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}
#else
	devvp = NULL;
#endif

	if ((mp->mnt_flag & MNT_UPDATE) == 0)
		error = cwfs_mountfs(devvp, mp, p, args);
	// TODO if needed
	// else {
	// 	if (devvp != cwmp->cwm_devvp)
	// 		error = EINVAL;	/* needs translation */
	// 	else
	// 		vrele(devvp);
	// }

	if (error) {
		// TODO if needed: vrele(devvp);
		return (error);
	}

	bzero(mp->mnt_stat.f_mntonname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntonname, path, MNAMELEN);
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromname, fspec, MNAMELEN);
	bzero(mp->mnt_stat.f_mntfromspec, MNAMELEN);
	strlcpy(mp->mnt_stat.f_mntfromspec, fspec, MNAMELEN);
	bcopy(args, &mp->mnt_stat.mount_info.cwfs_args, sizeof(*args));

	cwfs_statfs(mp, &mp->mnt_stat, p);

	return (0);
}

/*
 * Common code for mount and mountroot
 */
static int
cwfs_mountfs(register struct vnode *devvp, struct mount *mp, struct proc *p, struct cwfs_args *argp)
{
	register struct cwfs_mnt *cwmp = NULL;
	dev_t dev = devvp ? devvp->v_rdev : 0;
	int error = EINVAL;
	int ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
	extern struct vnode *rootvp;	// device containing root FS

	// TODO uncomment the following for read-only file system
	// if (!ronly)
	//	return (EROFS);

	cwmp = malloc(sizeof *cwmp, M_CWFSMNT, M_WAITOK);
	bzero(cwmp, sizeof *cwmp);

	// TODO: fill cwmp, goto out on error
	if (0)
		goto out;

	mp->mnt_data = cwmp;
	mp->mnt_stat.f_fsid.val[0] = (long)dev;
	mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
	mp->mnt_stat.f_namemax = /* TODO */ NAME_MAX;
	mp->mnt_flag |= MNT_LOCAL;
	cwmp->cwm_mountp = mp;
	cwmp->cwm_dev = dev;
	cwmp->cwm_devvp = devvp;

	devvp->v_specmountpoint = mp;

	return (0);

out:
	if (devvp->v_specinfo)
		devvp->v_specmountpoint = NULL;

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
	VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, NOCRED, p);
	VOP_UNLOCK(devvp);

	if (cwmp) {
		free(cwmp, M_CWFSMNT, sizeof(*cwmp));
		mp->mnt_data = NULL;
	}
	return (error);
}

/*
 * Make a filesystem operational; called at the end of (non-update) mount operation.
 */
/* ARGSUSED */
int
cwfs_start(struct mount *mp, int flags, struct proc *p)
{
	return (0);
}

/*
 * unmount system call
 */
int
cwfs_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	register struct cwfs_mnt *cwmp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	if ((error = vflush(mp, NULLVP, flags)) != 0)
		return (error);

	cwmp = VFSTOCWFS(mp);

	if (cwmp->cwm_devvp) {
		cwmp->cwm_devvp->v_specmountpoint = NULL;
		vn_lock(cwmp->cwm_devvp, LK_EXCLUSIVE | LK_RETRY);
		(void)VOP_CLOSE(cwmp->cwm_devvp, FREAD, NOCRED, p);
		vput(cwmp->cwm_devvp);
	}
	free(cwmp, M_CWFSMNT, sizeof(*cwmp));
	mp->mnt_data = NULL;
	mp->mnt_flag &= ~MNT_LOCAL;
	return (0);
}

/*
 * Return root vnode of a filesystem
 */
int
cwfs_root(struct mount *mp, struct vnode **vpp)
{
	// struct cwfs_mnt *cwmp = VFSTOCWFS(mp);
	ino_t ino = /* TODO */ 1;

	// TODO

	return (cwfs_vget_internal(mp, ino, vpp));
}

/*
 * Do operations associated with quotas.
 */
/* ARGSUSED */
int
cwfs_quotactl(struct mount *mp, int cmd, uid_t uid, caddr_t arg, struct proc *p)
{
	return (EOPNOTSUPP);
}

/*
 * Get file system statistics.
 */
int
cwfs_statfs(struct mount *mp, register struct statfs *sbp, struct proc *p)
{
	register struct cwfs_mnt *cwmp;

	cwmp = VFSTOCWFS(mp);

	sbp->f_bsize = /* TODO */ 512;
	sbp->f_iosize = sbp->f_bsize;	/* XXX */
	sbp->f_blocks = /* TODO */ 1;	/* total blocks */
	sbp->f_bfree = /* TODO */ 0;	/* total free blocks */
	sbp->f_bavail = /* TODO */ 0;	/* blocks free for non superuser */
	sbp->f_files =  /* TODO */ 0;	/* total files */
	sbp->f_ffree = /* TODO */ 0;	/* free file nodes */
	sbp->f_favail = /* TODO */ 0;	/* file nodes free for non superuser */
	copy_statfs_info(sbp, mp);

	return (0);
}

/* ARGSUSED */
int
cwfs_sync(struct mount *mp, int waitfor, int stall, struct ucred *cred, struct proc *p)
{
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is in range
 * - call iget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the generation number matches
 */

struct ifid {
	ushort	ifid_len;
	ushort	ifid_pad;
	ino_t	ifid_ino;
};

/* ARGSUSED */
int
cwfs_fhtovp(register struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid *ifhp = (struct ifid *)fhp;
	struct vnode *nvp;
	int error;

	d_printf("fhtovp: ino %lld\n", (long long)ifhp->ifid_ino);

	if ((error = VFS_VGET(mp, ifhp->ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	*vpp = nvp;
	return (0);
}

int
cwfs_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	// TODO: check that ino value is in valid range, panic() otherwise

	return (cwfs_vget_internal(mp, ino, vpp));
}

static int
cwfs_vget_internal(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	register struct cwfs_mnt *cwmp;
	struct cwfs_node *ip;
	struct vnode *vp, *nvp;
	dev_t dev;
	int error;

	cwmp = VFSTOCWFS(mp);
	dev = cwmp->cwm_dev;

	/* Allocate a new vnode/cwfs_node. */
	if ((error = getnewvnode(VT_CWFS, mp, &cwfs_vops, &vp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}
	ip = malloc(sizeof(*ip), M_CWFSNODE, M_WAITOK | M_ZERO);
	rrw_init_flags(&ip->i_lock, "cwinode", RWL_DUPOK | RWL_IS_VNODE);
	vp->v_data = ip;
	ip->i_vnode = vp;
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_devvp = cwmp->cwm_devvp;
	vref(ip->i_devvp);

	// TODO fill cwfs_node in 'ip'
	nanotime(&ip->i_atime);
	ip->i_ctime = ip->i_mtime = ip->i_atime;

	vp->v_type = /* TODO */ VNON;

	switch (vp->v_type) {
	case VFIFO:
#ifdef	FIFO
		vp->v_op = &cwfs_fifovops;
		break;
#else
		vput(vp);
		return (EOPNOTSUPP);
#endif	/* FIFO */

	case VCHR:
	case VBLK:
		/*
		 * if device, look at device number table for translation
		 */
		vp->v_op = &cwfs_specvops;
		if ((nvp = checkalias(vp, ip->i_dev, mp)) != NULL) {
			/*
			 * Discard unneeded vnode, but save its cwfs_node.
			 * Note that the lock is carried over in the cwfs_node
			 */
			nvp->v_data = vp->v_data;
			vp->v_data = NULL;
			vp->v_op = &spec_vops;
			vrele(vp);
			vgone(vp);
			/*
			 * Reinitialize aliased inode.
			 */
			vp = nvp;
			ip->i_vnode = vp;
		}
		break;

	case VLNK:
	case VNON:
	case VSOCK:
	case VDIR:
	case VBAD:
		break;
	case VREG:
		uvm_vnp_setsize(vp, ip->i_size);
		break;
	}

	// TODO: rules may differ
	if (ip->i_number == 1)
		vp->v_flag |= VROOT;

	/*
	 * XXX need generation number?
	 */

	*vpp = vp;
	return (0);
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
int
cwfs_vptofh(struct vnode *vp, struct fid *fhp)
{
	register struct cwfs_node *ip = VTOI(vp);
	register struct ifid *ifhp;

	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);

	ifhp->ifid_ino = ip->i_number;

	d_printf("vptofh: ino %lld\n", (long long)ifhp->ifid_ino);

	return (0);
}

/*
 * Verify a remote client has export rights and return these rights via
 * exflagsp and credanonp.
 */
int
cwfs_check_export(register struct mount *mp, struct mbuf *nam, int *exflagsp, struct ucred **credanonp)
{
	register struct netcred *np;
	register struct cwfs_mnt *imp = VFSTOCWFS(mp);

	/*
	 * Get the export permission structure for this <mp, client> tuple.
	 */
	np = vfs_export_lookup(mp, &imp->cwm_export, nam);
	if (np == NULL)
		return (EACCES);

	*exflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}
