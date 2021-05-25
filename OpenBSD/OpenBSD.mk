DOAS ?=			doas

SRC_DIR_base ?=		/usr/src
OBJ_DIR_base ?=		/usr/obj
SRC_DIR_kernel ?=	/usr/obj/sys
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

PATCH_INDICATOR_kernel =	fgrep -w cwfs ${SRC_DIR_kernel}/conf/files
KERNEL_CONFIG_INDICATOR =	${OBJ_DIR_kernel}/Makefile ${OBJ_DIR_kernel}/options
KERNEL_CONF_DIR =	${SRC_DIR_kernel}/arch/${OS_PLAT_CPU}/conf
KERNEL_HEADERS !=	grep ^'Index: sys/.*\.h' ${PATCH_DIR}/cw-kernel.patch | \
			cut -d ' ' -f 2

do-unpack-kernel:
	mkdir -p ${SRC_DIR_kernel}
	${GENERATE_UNPACK_CAT_CMD}; \
	cd ${SRC_DIR_kernel}/..; \
	unpack_cat ${FETCH_DIR}/sys.tar.gz | \
	    pax -r -s ',^\(./\)*sys,${SRC_DIR_kernel:C,.*/,,},'
	mkdir -p ${SRC_DIR_kernel}/cwfs

do-build-kernel: ${KERNEL_CONFIG_INDICATOR}
	cd ${OBJ_DIR_kernel}; ${MAKE_CMD}

${KERNEL_CONFIG_INDICATOR} kernel-config:
	cd ${KERNEL_CONF_DIR}; \
	mkdir -p ${OBJ_DIR_kernel}; \
	config -b ${OBJ_DIR_kernel} -s ${SRC_DIR_kernel} ${KERNEL_CONFIG}

install-kernel:
	cd ${OBJ_DIR_kernel}; ${DOAS} ${MAKE_CMD} install

clean-kernel:
	cd ${OBJ_DIR_kernel} && ${MAKE_CMD} clean || true

distclean-kernel:
	rm -f ${KERNEL_CONFIG_INDICATOR}
	rm -Rf ${OBJ_DIR_kernel} ${SRC_DIR_kernel}

.for _h in ${KERNEL_HEADERS}
kernel-headers: /usr/include/${_h}
/usr/include/${_h}: ${SRC_DIR_kernel}/${_h}
	${DOAS} install -o root -g bin -m 0444 $> $@
.endfor


####################
# Base OS specifics

PATCH_INDICATOR_base =		fgrep -w mount_cwfs ${SRC_DIR_base}/sbin/Makefile

BASE_DIRS = \
	sbin/mount \
	sbin/mount_cwfs \
	share/man

BASE_INSTALL_EXTRA = \
	etc/etc.${OS_PLAT}/MAKEDEV	root	wheel	0555	/dev \
	lib/libc/sys/mount.2		root	bin	0444	/usr/share/man/man2

do-unpack-base:
	mkdir -p ${SRC_DIR_base}
	${GENERATE_UNPACK_CAT_CMD}; \
	cd ${SRC_DIR_base}; \
	unpack_cat ${FETCH_DIR}/src.tar.gz | pax -r
	mkdir -p ${SRC_DIR_base}/sbin/mount_cwfs

# ignores e.g. sbin/mount_cwfs/ in unpatched mode
do-build-base: kernel-headers
	mkdir -p ${OBJ_DIR_base}
.for _d in ${BASE_DIRS}
. if exists(${SRC_DIR_base}/${_d}/Makefile)
	cd ${SRC_DIR_base}/${_d}; ${MAKE_CMD} obj all
. endif
.endfor

install-base:
.for _d in ${BASE_DIRS}
	cd ${SRC_DIR_base}/${_d}; ${DOAS} ${MAKE_CMD} install
.endfor
.for _f _o _g _m _d in ${BASE_INSTALL_EXTRA}
	cd ${SRC_DIR_base}; ${DOAS} install -c -o ${_o} -g ${_g} -m ${_m} ${_f} ${DESTDIR}${_d}
.endfor

clean-base:
.for _d in ${BASE_DIRS}
	cd ${SRC_DIR_base}/${_d} && ${MAKE_CMD} clean || true
.endfor
