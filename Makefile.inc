#
# For documentation how to use this file, see README.md.
#

ERRORS =

OS !=			uname -s
OS_RELEASE !=		uname -r
OS_PLAT !=		uname -m
OS_PLAT_CPU !=		uname -p

# Validity checks
OS_IS_SUPPORTED = no
UNIMPLEMENTED_OS =
.for _os _rel _cpu in ${SUPPORTED_OS}
. if "${_os}" == "${OS}" && "${_rel}" == "${OS_RELEASE}" && "${_cpu}" == "${OS_PLAT_CPU}"
OS_IS_SUPPORTED = yes
. endif
. if !exists(${.CURDIR}/../${_os}/${_rel}) || !exists(${.CURDIR}/../${_os}/${_os}.mk)
UNIMPLEMENTED_OS += ${_os} ${_rel}
. endif
.endfor
.if "${OS_IS_SUPPORTED}" != "yes"
ERRORS +=	"The ${OS} ${OS_RELEASE}/${OS_PLAT} is not supported."
.endif
.if !empty(UNIMPLEMENTED_OS)
ERRORS +=	"Internal error: \${SUPPORTED_OS} lists ${UNIMPLEMENTED_OS} which are not implemented!"
.endif

FETCH_DIR ?=	${HOME}/${OS}/${OS_RELEASE}
PATCH_DIR =	${.CURDIR}/../${OS}/${OS_RELEASE}
MAKE_CMD =	env ${MAKE_ENV} ${.MAKE}

GENERATE_UNPACK_CAT_CMD = \
	unpack_cat() { \
		echo "Unpacking $${1\#\#*/}:" >&2; \
		case $$1 in \
		*.gz|*.tgz)	unpacker=gzcat;; \
		*.bz2|*.tbz)	unpacker=bzcat;; \
		*.xz|*.txz)	unpacker=xzcat;; \
		*)		unpacker=cat;; \
		esac; \
		if [ -t 2 -a -x /usr/bin/ftp -a OpenBSD = "${OS}" ]; then \
			/usr/bin/ftp -mo - "file://$$1" | $$unpacker; \
		elif curl=$$(command -v curl || true); [ -t 2 -a -n "$$curl"  ]; then \
			"$$curl" -o - "file://$$1" | $$unpacker; \
		else \
			$$unpacker <"$$1"; \
		fi; \
	}

# Component-specific targets
COMP_TARGETS =	build clean configure distclean extract fetch install
COMP_TARGETS +=	patch skel unpatch

# Targets which are implemented as do-* wrappers
RICH_TARGETS =	fetch extract skel patch configure

.PHONY: all ${COMP_TARGETS}
.MAIN: all
all: patch build

.include "${OS}/${OS}.mk"

#################################
# Component and OS-agnostic code

.for _c _s in ${COMPONENTS}

# E.g. set to "sys" on OpenBSD for kernel sources.
# The "./" at the very beginning will be stripped always.
UNPACK_${_c}_PREFIX ?=

# Skeleton directory, if present, contains actual course work files.
# SKEL_${_c} must be a relative to student's flow folder, i.e.,
# to the directory of the Makefile that included this file.
SKEL_${_c} ?=
. if empty(SKEL_${_c})
SKEL_FILES_${_c} =
. else
SKEL_FILES_${_c} != \
	cd ${.CURDIR}/${SKEL_${_c}}; \
	find . -type f \! -name '*.orig' \! -name '*.rej' | \
	cut -c 3-
. endif

# List paths to the (to be) installed skeleton files
SKEL_INSTALLED_${_c} = \
	${SKEL_FILES_${c}:C,^,${SRC_DIR_${_c}}/,}

${_c}_FETCHED_SET =	${FETCH_DIR}/${_s}
COOKIE_fetch_${_c} =	${${_c}_FETCHED_SET}
COOKIE_extract_${_c} =	${SRC_DIR_${_c}}/.cw.unpacked
COOKIE_skel_${_c} =	${SKEL_INSTALLED_${_c}}
COOKIE_patch_${_c} =	${SRC_DIR_${_c}}/.cw.patched
COOKIE_configure_${_c} ?=	${OBJ_DIR_${_c}}/.cw.configured
REMOVABLE_COOKIES_${_c} += \
	${COOKIE_extract_${_c}} \
	${COOKIE_patch_${_c}} \
	${COOKIE_configure_${_c}}
REMOVABLE_COOKIES +=	${REMOVABLE_COOKIES_${_c}}

# Where actual make to be invoked
BUILD_DIR_${_c} ?=	${OBJ_DIR_${_c}}

# What subdirectories of build tree to visit during build
BUILD_SUBDIRS_${_c} ?=	.

# Targets to run when building
BUILD_TARGET_${_c} ?=	all


do-fetch-${_c}:
	mkdir -p ${FETCH_DIR}
	cd ${FETCH_DIR}; ${FETCH_CMD} -o ${${_c}_FETCHED_SET}.tmp ${FETCH_URI}${_s}
	cd ${FETCH_DIR}; mv ${${_c}_FETCHED_SET}.tmp ${${_c}_FETCHED_SET}

${COOKIE_extract_${_c}}: ${COOKIE_fetch_${_c}}
do-extract-${_c}:
	mkdir -p ${SRC_DIR_${_c}}
        ${GENERATE_UNPACK_CAT_CMD}; \
        cd ${SRC_DIR_${_c}}; \
        unpack_cat ${FETCH_DIR}/${${_c}_FETCHED_SET} | \
	    pax -r -s ',^\(\./\)*${UNPACK_${_c}_PREFIX},,'

${COOKIE_skel_${_c}}: ${COOKIE_extract_${_c}}
do-skel-${_c}:
. for _f in ${SKEL_FILES_${_c}}
.  if "${FORCE_SKEL_INSTALL}" != "yes"
	@cd ${.CURDIR}; \
	if [ -e ${SRC_DIR_${_c}}/${_f} ]; then \
		if cmp -s ${SKEL_${_c}}/${_f} ${SRC_DIR_${_c}}/${_f}; then \
			echo touch ${SRC_DIR_${_c}}/${_f}; \
			touch ${SRC_DIR_${_c}}/${_f}; \
		else \
			echo "ВНИМАНИЕ: ${SKEL_${_c}}/${_f} был обновлён и, скорее всего," >&2; \
			echo "должен быть повторно слит с ${SRC_DIR_${_c}}/${_f}." >&2; \
			echo "При необходимости, попросите преподавателя о помощи." >&2; \
		fi; \
	else \
		echo install -D ${SKEL_${_c}}/${_f} ${SRC_DIR_${_c}}/${_f}; \
		     install -D ${SKEL_${_c}}/${_f} ${SRC_DIR_${_c}}/${_f}; \
	fi
.  else
	cd ${.CURDIR}; install -D ${SKEL_${_c}}/${_f} ${SRC_DIR_${_c}}/${_f}
.  endif
. endfor

${COOKIE_patch_${_c}}: ${COOKIE_skel_${_c}}
do-patch-${_c}:
	cd ${SRC_DIR_${_c}}; \
	patch -E <${PATCH_DIR}/cw-${_c}.patch

${COOKIE_configure_${_c}}: ${COOKIE_extract_${_c}}
do-configure-${_c}:

# Special: no cookies and do-* here, runs unconditionally
unpatch: unpatch-${_c}
unpatch-${_c}:
	@rm -f ${COOKIE_patch_${_c}}
	cd ${SRC_DIR_${_c}}; \
	patch -R -E <${PATCH_DIR}/cw-${_c}.patch

build-${_c}: ${COOKIE_configure_${_c}}
	mkdir -p ${OBJ_DIR_${_c}}
. for _d in ${BUILD_SUBDIRS_${_c}}
.  if exists(${BUILD_DIR_${_c}}/${_d}/Makefile)
	cd ${BUILD_DIR_${_c}}/${_d}; ${MAKE_CMD} ${BUILD_TARGET_${_c}}
.  endif
. endfor

clean-${_c}:
. for _d in ${BUILD_SUBDIRS_${_c}}
.  if exists(${BUILD_DIR_${_c}}/${_d}/Makefile)
	-cd ${BUILD_DIR_${_c}}/${_d}; ${MAKE_CMD} clean
.  endif
. endfor

install-${_c}:
. for _d in ${BUILD_SUBDIRS_${_c}}
.  if exists(${BUILD_DIR_${_c}}/${_d}/Makefile)
	cd ${BUILD_DIR_${_c}}/${_d}; ${DOAS} ${MAKE_CMD} install
.  endif
. endfor

# Make sure extras are installed last
install-extra-${_c}: .USE
.  for _f _o _g _m _d in ${INSTALL_EXTRA_${_c}}
	cd ${BUILD_DIR_${_c}}; ${DOAS} install -c -o ${_o} -g ${_g} -m ${_m} ${_f} ${DESTDIR}${_d}
.  endfor
install-${_c}: install-extra-${_c}

${_c}: ${COOKIE_patch_${_c}} .WAIT build-${_c}

distclean-${_c}:
	@rm -f ${REMOVABLE_COOKIES_${_c}}
	rm -Rf ${OBJ_DIR_${_c}} ${SRC_DIR_${_c}}

# Generate aggregating targets
. for _t in ${COMP_TARGETS}
.PHONY: ${_t}-${_c}
${_t}: .WAIT ${_t}-${_c}
. endfor
. for _t in ${RICH_TARGETS}
.PHONY: re${_t}-${_c} do-${_t}-${_c}
${_t}-${_c}: ${COOKIE_${_t}_${_c}}
re${_t}: .WAIT re${_t}-${_c}
re${_t}-${_c} ${COOKIE_${_t}_${_c}}:
	@rm -f ${COOKIE_${_t}_${_c}}
	@${.MAKE} do-${_t}-${_c}
	@touch ${COOKIE_${_t}_${_c}}
. endfor

.endfor



##############################
# Display any errors detected

.BEGIN:
.if !empty(ERRORS)
. for _e in ${ERRORS}
	@printf "Error: %s\n" ${_e} >&2
	@false
. endfor
.endif
