DOAS ?=			doas

SRC_DIR_base ?=		/usr/src
OBJ_DIR_base ?=		/usr/obj
SRC_DIR_kernel ?=	/usr/src/sys
OBJ_DIR_kernel ?=	/usr/obj/k/${KERNEL_CONFIG}
KERNEL_CONFIG ?=	GENERIC

MAKE_ENV =	BSDSRCDIR=${SRC_DIR_base}
MAKE_ENV +=	BSDOBJDIR=${OBJ_DIR_base}

FETCH_CMD ?=	/usr/bin/ftp -C
FETCH_URI ?=	https://cdn.openbsd.org/pub/${OS}/${OS_RELEASE}/

COMPONENTS =	kernel	sys.tar.gz
COMPONENTS +=	base	src.tar.gz


###################
# Kernel specifics

UNPACK_kernel_PREFIX = sys
COOKIE_configure_kernel = ${OBJ_DIR_kernel}/Makefile ${OBJ_DIR_kernel}/options
KERNEL_CONF_DIR =	${SRC_DIR_kernel}/arch/${OS_PLAT_CPU}/conf
KERNEL_HEADERS !=	grep '^Index: sys/.*\.h' ${PATCH_DIR}/cw-kernel.patch | \
			cut -d ' ' -f 2

do-configure-kernel:
	cd ${KERNEL_CONF_DIR}; \
	mkdir -p ${OBJ_DIR_kernel}; \
	config -b ${OBJ_DIR_kernel} -s ${SRC_DIR_kernel} ${KERNEL_CONFIG}

.for _h in ${KERNEL_HEADERS}
/usr/include/${_h}: ${SRC_DIR_kernel}/${_h}
	${DOAS} install -o root -g bin -m 0444 $> $@
.endfor

.PHONY: install-kernel-headers
install-kernel-headers:
.for _h in ${KERNEL_HEADERS}
	cmp -s ${SRC_DIR_kernel}/${_h} /usr/include/${_h} || \
	${DOAS} install -o root -g bin -m 0444 ${SRC_DIR_kernel}/${_h} /usr/include/${_h}
.endfor


####################
# Base OS specifics

BUILD_DIR_base =	${SRC_DIR_base}

BUILD_SUBDIRS_base = \
	sbin/mount \
	sbin/mount_cwfs

BASE_MAN_PAGES != grep '^Index: share/man/.*\.[0-9]$$' ${PATCH_DIR}/cw-base.patch | \
		  cut -d ' ' -f 2

INSTALL_EXTRA_base = \
	etc/etc.${OS_PLAT}/MAKEDEV	root	wheel	0555	/dev \
	lib/libc/sys/mount.2		root	bin	0444	/usr/share/man/man2
.for _man in ${BASE_MAN_PAGES}
INSTALL_EXTRA_base +=	${_man}		root	bin	0444	/usr/${_man:C,/man.\.,/,}
.endfor

do-configure-base:
.for _d in ${BUILD_SUBDIRS_base}
. if exists(${BUILD_DIR_base}/${_d}/Makefile)
	cd ${BUILD_DIR_base}/${_d}; make obj
. endif
.endfor


build-base: ${KERNEL_HEADERS:C,^,/usr/include/,}
