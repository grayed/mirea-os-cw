/*	$OpenBSD$	*/

/*
 * Copyright (c) 2017 YOUR NAME <email@example.org>
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
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/pledge.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/filedesc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/fcntl.h>

#include <sys/domain.h>
#include <netinet/in.h>
#include <net/route.h>

#define BUFFERSIZE 255

struct s_echo {
	char msg[BUFFERSIZE + 1];
	int len;
};

static struct s_echo echomsg;
static int socketfd;
//static struct sockaddr serv_addr;

uint16_t mydnsjackport;

static int
mysockargs(struct mbuf **mp, const void *buf, size_t buflen, int type)
{
	struct sockaddr *sa;
	struct mbuf *m;
	//int error;

	/*
	* We can't allow socket names > UCHAR_MAX in length, since that
	* will overflow sa_len. Also, control data more than MCLBYTES in
	* length is just too much.
	*/
	if (buflen > (type == MT_SONAME ? UCHAR_MAX : MCLBYTES))
		return (EINVAL);

	/* Allocate an mbuf to hold the arguments. */
	m = m_get(M_WAIT, type);
	if (buflen > MLEN) {
		MCLGET(m, M_WAITOK);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return ENOBUFS;
		}
	}
	m->m_len = buflen;
	/*error = copyin(buf, mtod(m, caddr_t), buflen);
	if (error) {
		(void)m_free(m);
		return (error);
	}*/
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);
		sa->sa_len = buflen;
	}
	return (0);
}

int
cwopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct sys_socket_args uap_socket = { .domain = {AF_INET}, .type = {SOCK_STREAM}, .protocol = {0} };
	struct filedesc *fdp = p->p_fd;
	struct socket *so;
	struct file *fp;
	int type = SCARG(&uap_socket, type);
	int domain = SCARG(&uap_socket, domain);
	int fd, error, ss = 0;
	
	if ((type & SOCK_DNS) && !(domain == AF_INET || domain == AF_INET6))
		return (EINVAL);

	if (ISSET(type, SOCK_DNS))
		ss |= SS_DNS;

	//error = pledge_socket(p, domain, ss);
	//if (error)
	//{
	//	printf("Error pledge %d\n", error);
	//	return (error);
	//}

	//fdplock(fdp);
	error = falloc(p, &fp, &fd);
	//fdpunlock(fdp);
	if (error != 0)
		goto out;

	fp->f_flag = FREAD | FWRITE | (type & SOCK_NONBLOCK ? FNONBLOCK : 0);
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	error = socreate(SCARG(&uap_socket, domain), &so,
	    type & ~(SOCK_CLOEXEC | SOCK_NONBLOCK | SOCK_DNS), SCARG(&uap_socket, protocol));
	if (error) {
		//fdplock(fdp);
		fdremove(fdp, fd);
		closef(fp, p);
		//fdpunlock(fdp);
		return (error);
	}

	fp->f_data = so;
	if (type & SOCK_NONBLOCK)
		(*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&type, p);
	so->so_state |= ss;
	fdinsert(fdp, fd, fdp->fd_ofileflags[fd], fp);
	socketfd = fd;
	fdpunlock(fdp);
	FRELE(fp, p);

	// ##### CONNECT #####

	/*serv_addr.sa_family = AF_INET;
	memset(serv_addr.sa_data, 0, 14);
	serv_addr.sa_data[0] = 127;
	serv_addr.sa_data[1] = 0;
	serv_addr.sa_data[2] = 0;
	serv_addr.sa_data[3] = 1;*/

	printf("%d\n", __LINE__);
	//const struct sockaddr* serv_addr_p = &serv_addr;

	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(22);
	sa.sin_addr.s_addr = htonl(0x7F000001);

	struct sys_connect_args uap_connect = { .s = { socketfd },.name = { .be = { .datum = (const struct sockaddr *)&sa } },.namelen = { sizeof(sa) } };

	//struct file *fp;
	//struct socket *so;
	struct mbuf *nam = NULL;
	int s, interrupted = 0;

	printf("%d\n", __LINE__);
	if ((error = getsock(p, SCARG(&uap_connect, s), &fp)) != 0)
		return (error);
	so = fp->f_data;
	if (so->so_state & SS_ISCONNECTING) {
		FRELE(fp, p);
		return (EALREADY);
	}
	printf("%d\n", __LINE__);
	error = mysockargs(&nam, SCARG(&uap_connect, name), SCARG(&uap_connect, namelen),
		MT_SONAME);
	printf("%d\n", __LINE__);
	if (error)
		goto bad;
	printf("%d\n", __LINE__);

	error = soconnect(so, nam);
	if (error)
		goto bad;
	printf("%d\n", __LINE__);
	if ((fp->f_flag & FNONBLOCK) && (so->so_state & SS_ISCONNECTING)) {
		FRELE(fp, p);
		m_freem(nam);
		return (EINPROGRESS);
	}
	printf("%d\n", __LINE__);
	s = solock(so);
	printf("%d\n", __LINE__);
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = sosleep_nsec(so, &so->so_timeo, PSOCK | PCATCH,
			"netcon2", INFSLP);
		printf("3\n");
		if (error) {
			if (error == EINTR || error == ERESTART)
				interrupted = 1;
			break;
		}
	}
	printf("%d\n", __LINE__);
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	printf("%d\n", __LINE__);
	sounlock(so, s);
	printf("%d\n", __LINE__);

	printf("Opened device \"cw\" successfully.\n");
	
	return 0;
out:
	return (error);
bad:
	if (!interrupted)
		so->so_state &= ~SS_ISCONNECTING;
	FRELE(fp, p);
	m_freem(nam);
	if (error == ERESTART)
		error = EINTR;

	fdremove(fdp, fd);

	return (error);
}

int
cwclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	printf("Closing device \"cw\".\n");
	
	return 0;
}

int
cwioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	int error = 0;

	switch (cmd) {
	// TODO ...
	default:
		error = ENOTTY;
	}

	return error;
}

int
cwread(dev_t dev, struct uio *uio, int flag)
{
	printf("Read %s\n", echomsg.msg);
	
	size_t amt;
	int error;

	/*
	 * How big is this read operation?  Either as big as the user wants,
	 * or as big as the remaining data.  Note that the 'len' does not
	 * include the trailing null character.
	 */
	amt = MIN(uio->uio_resid, uio->uio_offset >= echomsg.len + 1 ? 0 :
	    echomsg.len + 1 - uio->uio_offset);

	if ((error = uiomove(echomsg.msg, amt, uio)) != 0)
		printf("uiomove failed!\n");

	return (error);
}

int
cwwrite(dev_t dev, struct uio *uio, int flag)
{
	printf("Write %s\n", echomsg.msg);
	
	size_t amt;
	int error;

	/*
	 * We either write from the beginning or are appending -- do
	 * not allow random access.
	 */
	if (uio->uio_offset != 0 && (uio->uio_offset != echomsg.len))
		return (EINVAL);

	/* This is a new message, reset length */
	if (uio->uio_offset == 0)
		echomsg.len = 0;

	/* Copy the string in from user memory to kernel memory */
	amt = MIN(uio->uio_resid, (BUFFERSIZE - echomsg.len));

	error = uiomove(echomsg.msg + uio->uio_offset, amt, uio);

	/* Now we need to null terminate and record the length */
	echomsg.len = uio->uio_offset;
	echomsg.msg[echomsg.len] = 0;

	if (error != 0)
		printf("Write failed: bad address!\n");
	return (error);
}
