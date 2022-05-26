#	$OpenBSD$

.include <bsd.own.mk>

PROG=		mount_cwfs
SRCS=		mount_cwfs.c getmntopts.c
MAN=		mount_cwfs.8

MOUNT=		${.CURDIR}/../mount
.PATH:		${MOUNT}

CPPFLAGS+=	-I${MOUNT}
DPADD+=		${LIBUTIL}
LDADD+=		-lutil

.include <bsd.prog.mk>
