##########################
# Tweakable values

BASE_SRC_DIR ?=		/usr/src
BASE_OBJ_DIR ?=		/usr/obj
KERNEL_SRC_DIR ?=	${BASE_SRC_DIR}/sys
KERNEL_OBJ_DIR ?=	/usr/obj/k/${KERNEL_CONFIG}
KERNEL_CONFIG ?=	GENERIC

FETCH_CMD ?=		ftp -C
FETCH_DIR ?=		${HOME}/Downloads
FETCH_URI ?=		https://cdn.openbsd.org/pub/OpenBSD/6.8/

# End of tweakable values
##########################

# for OS build system
MAKE_CMD =	env BSDSRCDIR=${BASE_SRC_DIR} BSDOBJDIR=${BASE_OBJ_DIR} ${.MAKE}

GENERATE_UNPACK_CAT_CMD = \
	unpack_cat() { \
		echo "Unpacking $$@:" >&2; \
		if [ -t 2 ]; then \
			ftp -mo - $$(for a in "$$@"; do echo "file://$$a"; done); \
		else \
			cat -- "$$@"; \
		fi; \
	}

COMPONENTS =	kernel	sys
COMPONENTS +=	base	src

all: base kernel

.PHONY: all base build clean distclean fetch kernel kernel-config install patch
.PHONY: unpack unpatch


###############################################
# Variables used in component-specific targets

.for _c _s in ${COMPONENTS}
${_c:U}_FETCHED_SET =	${FETCH_DIR}/${_s}.tar.gz
UNPACK_${_c:U}_COOKIE =	${${_c:U}_SRC_DIR}/.cw.unpacked
PATCH_${_c:U}_COOKIE =	${${_c:U}_SRC_DIR}/.cw.patched
ALL_${_c:U}_COOKIES = \
	${UNPACK_${_c:U}_COOKIE} \
	${PATCH_${_c:U}_COOKIE}
.endfor


###################
# Kernel specifics

PATCH_KERNEL_INDICATOR =	${KERNEL_SRC_DIR}/dev/cw.c
KERNEL_CONFIG_INDICATOR =	${KERNEL_OBJ_DIR}/Makefile ${KERNEL_OBJ_DIR}/options
KERNEL_CONF_DIR =		${KERNEL_SRC_DIR}/arch/${MACHINE_CPU}/conf

do-unpack-kernel:
	mkdir -p ${KERNEL_SRC_DIR}
	${GENERATE_UNPACK_CAT_CMD}; \
	cd ${KERNEL_SRC_DIR}/..; \
	unpack_cat ${FETCH_DIR}/sys.tar.gz | \
	    pax -rz -s '@./sys@./${KERNEL_SRC_DIR:C,.*/,,}@'
	mkdir -p ${KERNEL_SRC_DIR}/cwfs

do-build-kernel: ${KERNEL_CONFIG_INDICATOR}
	cd ${KERNEL_OBJ_DIR}; ${MAKE_CMD}

${KERNEL_CONFIG_INDICATOR} kernel-config:
	cd ${KERNEL_CONF_DIR}; \
	config -b ${KERNEL_OBJ_DIR} -s ${KERNEL_SRC_DIR} ${KERNEL_CONFIG}

install-kernel:
	cd ${KERNEL_OBJ_DIR}; ${MAKE_CMD} install

clean-kernel:
	cd ${KERNEL_OBJ_DIR} && ${MAKE_CMD} clean || true

distclean-kernel:
	rm -f ${KERNEL_CONFIG_INDICATOR}
	rm -Rf ${KERNEL_OBJ_DIR} ${KERNEL_SRC_DIR}

install-sys-headers:
	cd ${KERNEL_SRC_DIR}; \
	install -o root -g bin -m 0444 \
	    $$(grep ^'Index: sys/.*\.h' ${.CURDIR}/cw-kernel.patch | cut -d ' ' -f 2) \
	    /usr/include/sys/


####################
# Base OS specifics

PATCH_BASE_INDICATOR =		${BASE_SRC_DIR}/sbin/mount_cwfs/mount_cwfs.c

BASE_DIRS = \
	sbin/mount \
	sbin/mount_cwfs \
	share/man

BASE_INSTALL_EXTRA = \
	etc/etc.${MACHINE}/MAKEDEV	root	wheel	0555	/dev \
	lib/libc/sys/mount.2		root	bin	0444	/usr/share/man/man2

do-unpack-base:
	mkdir -p ${BASE_SRC_DIR}
	${GENERATE_UNPACK_CAT_CMD}; \
	cd ${BASE_SRC_DIR}; \
	unpack_cat ${FETCH_DIR}/src.tar.gz | pax -rz
	mkdir -p ${BASE_SRC_DIR}/sbin/mount_cwfs

# ignores e.g. sbin/mount_cwfs/ in unpatched mode
do-build-base: install-sys-headers
	mkdir -p ${BASE_OBJ_DIR}
.for _d in ${BASE_DIRS}
. if exists(${BASE_SRC_DIR}/${_d}/Makefile)
	cd ${BASE_SRC_DIR}/${_d}; ${MAKE_CMD} obj all
. endif
.endfor

install-base:
.for _d in ${BASE_DIRS}
	cd ${BASE_SRC_DIR}/${_d}; ${MAKE_CMD} install
.endfor
.for _f _o _g_ _m _d in ${BASE_INSTALL_EXTRA}
	install -c -o ${_o} -g ${_g} -m ${_m} ${_f} ${DESTDIR}${_d}
.endfor

clean-base:
.for _d in ${BASE_DIRS}
	cd ${BASE_SRC_DIR}/${_d} && ${MAKE_CMD} clean || true
.endfor


#############################
# Component-agnostic targets

.for _c _s in ${COMPONENTS}
.PHONY: build-${_c} clean-${_c} distclean-${_c} fetch-${_c} install-${_c}
.PHONY: patch-${_c} unpack-${_c} unpatch-${_c}
.PHONY: do-build-${_c} do-unpack-${_c}

fetch: fetch-${_c}
fetch-${_c}: ${${_c:U}_FETCHED_SET}
${${_c:U}_FETCHED_SET}:
	mkdir -p ${FETCH_DIR}
	${FETCH_CMD} -o $@.tmp ${FETCH_URI}${_s}.tar.gz
	mv $@.tmp $@

unpack: unpack-${_c}
unpack-${_c}: ${UNPACK_${_c:U}_COOKIE}
${UNPACK_${_c:U}_COOKIE}: ${${_c:U}_FETCHED_SET}
	@${.MAKE} do-unpack-${_c}
	@touch $@

patch: patch-${_c}
patch-${_c}: ${PATCH_${_c:U}_COOKIE}
${PATCH_${_c:U}_COOKIE}: ${UNPACK_${_c:U}_COOKIE}
	@test ! -e ${PATCH_${_c:U}_INDICATOR} || \
	    { echo called $@ in patched directory ${${_c:U}_SRC_DIR} >&2; exit 1; }
	cd ${${_c:U}_SRC_DIR}; \
	patch -C <${.CURDIR}/cw-${_c}.patch; \
	patch -E -s <${.CURDIR}/cw-${_c}.patch
	@touch $@

unpatch: unpatch-${_c}
unpatch-${_c}:
	@test -e ${PATCH_${_c:U}_INDICATOR} || \
	    { echo called $@ in unpatched directory ${${_c:U}_SRC_DIR} >&2; exit 1; }
	@rm -f ${PATCH_${_c:U}_COOKIE}
	cd ${${_c:U}_SRC_DIR}; \
	patch -R -C <${.CURDIR}/cw-${_c}.patch; \
	patch -R -E -s <${.CURDIR}/cw-${_c}.patch

build: build-${_c}
build-${_c}: ${UNPACK_${_c:U}_COOKIE}
	@${.MAKE} do-build-${_c}

${_c}: patch-${_c} build-${_c}

install: install-${_c}

clean: clean-${_c}

distclean: distclean-${_c}
distclean-${_c}:
	rm -f ${ALL_${_c:U}_COOKIES}
	rm -Rf ${${_c:U}_OBJ_DIR} ${${_c:U}_SRC_DIR}

.endfor
