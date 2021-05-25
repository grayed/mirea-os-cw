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
MAKE_CMD =	env ${MAKE_ENV_${OS}} ${.MAKE}

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

.PHONY: all build clean distclean fetch install patch skel unpack unpatch
.PHONY: kernel-config kernel-headers
.MAIN: all

all: patch build
skel:

.include "${OS}/${OS}.mk"

#################################
# Component and OS-agnostic code

.for _c _s in ${COMPONENTS}

${_c}_FETCHED_SET =	${FETCH_DIR}/${_s}
UNPACK_${_c}_COOKIE =	${SRC_DIR_${_c}}/.cw.unpacked
PATCH_${_c}_COOKIE =	${SRC_DIR_${_c}}/.cw.patched
ALL_${_c}_COOKIES = \
	${UNPACK_${_c}_COOKIE} \
	${PATCH_${_c}_COOKIE}


.PHONY: build-${_c} clean-${_c} distclean-${_c} fetch-${_c} install-${_c}
.PHONY: patch-${_c} skel-${_c} unpack-${_c} unpatch-${_c}
.PHONY: do-build-${_c} do-unpack-${_c}

fetch: fetch-${_c}
fetch-${_c}: ${${_c}_FETCHED_SET}
${${_c}_FETCHED_SET}:
	mkdir -p ${FETCH_DIR}
	${FETCH_CMD} ${FETCH_URI}${_s} >$@.tmp
	mv $@.tmp $@

unpack: unpack-${_c}
unpack-${_c}: ${UNPACK_${_c}_COOKIE}
${UNPACK_${_c}_COOKIE}: ${${_c}_FETCHED_SET}
	@${.MAKE} do-unpack-${_c}
	@touch $@

# Skeleton directory should contain actual course work files.
# SKEL_${_c} must be a relative to student's flow folder, i.e.,
# to the directory of the Makefile that included this file.
SKEL_${_c} ?=
skel: skel-${_c}

# Make sure the skeleton directory exists before copying files.
# The ${.CURDIR}/ is mandatory, otherwise, if ${SKEL_${_c}}
# expands to "skel", which is perfectly valid itself, then
# circular dependency between 'skel' and 'skel-foo' targets
# will be created.
skel-${_c}: ${.CURDIR}/${SKEL_${_c}}

. if !empty(SKEL_${_c})
SKEL_FILES_${_c} != \
	cd ${.CURDIR}/${SKEL_${_c}}; \
	find . -type f \! -name '*.orig' \! -name '*.rej' | \
	cut -c 3-

.  for _f in ${SKEL_FILES_${_c}}
skel-${_c}: ${SRC_DIR_${_c}}/${_f}
${SRC_DIR_${_c}}/${_f}: ${UNPACK_${_c}_COOKIE} ${SKEL_${_c}}/${_f}
.   if exists(${SRC_DIR_${_c}}/${_f})
	@cmp -s ${SKEL_${_c}}/${_f} ${SRC_DIR_${_c}}/${_f} && \
	 touch ${SRC_DIR_${_c}}/${_f} || { \
	  echo "Warning: ${SKEL_${_c}}/${_f} was updated and likely needs to be merged" >&2; \
	  echo "into ${SRC_DIR_${_c}}/${_f}" >&2; \
	 }
.   else
	install "${.CURDIR}/${SKEL_${_c}}/${_f}" "$@"
.   endif
.  endfor
. endif

patch: patch-${_c}
patch-${_c}: skel-${_c} ${PATCH_${_c}_COOKIE}
${PATCH_${_c}_COOKIE}: ${UNPACK_${_c}_COOKIE}
	@! ${PATCH_INDICATOR_${_c}} ||
	    { echo called $@ in patched directory ${SRC_DIR_${_c}} >&2; exit 1; }
	cd ${SRC_DIR_${_c}}; \
	patch -C <${PATCH_DIR}/cw-${_c}.patch; \
	patch -E -s <${PATCH_DIR}/cw-${_c}.patch
	@touch $@

unpatch: unpatch-${_c}
unpatch-${_c}:
	@${PATCH_INDICATOR_${_c}} ||
	    { echo called $@ in unpatched directory ${SRC_DIR_${_c}} >&2; exit 1; }
	@rm -f ${PATCH_${_c}_COOKIE}
	cd ${SRC_DIR_${_c}}; \
	patch -R -C <${PATCH_DIR}/cw-${_c}.patch; \
	patch -R -E -s <${PATCH_DIR}/cw-${_c}.patch

build: build-${_c}
build-${_c}: ${UNPACK_${_c}_COOKIE}
	@${.MAKE} do-build-${_c}

${_c}: patch-${_c} build-${_c}

install: install-${_c}

clean: clean-${_c}

distclean: distclean-${_c}
distclean-${_c}:
	rm -f ${ALL_${_c}_COOKIES}
	rm -Rf ${OBJ_DIR_${_c}} ${SRC_DIR_${_c}}

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
