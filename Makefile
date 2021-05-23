#
# For documentation how to use this file, see README.md.
#

ERRORS =

OS !=			uname -s
OS_RELEASE !=		uname -r
OS_PLAT !=		uname -m
OS_PLAT_CPU !=		uname -p

ALL_OS =	OpenBSD
# XXX Gross hack due to bug in make with :M and :N matching outside loop.
# XXX Should be instead:
# XXX .if empty((ALL_OS:M${OS})
# XXX ERRORS += ...
# XXX .endif
_VALID_OSES =
.for _os in ${ALL_OS}
. if !empty(OS:M${_os})
_VALID_OSES +=	${_os}
. endif
.endfor
.if empty(_VALID_OSES) || !exists(${.CURDIR}/${OS}/${OS}.mk)
ERRORS +=	"The ${OS} operating system is not supported"
.endif
.if !exists(${.CURDIR}/${OS}/${OS_RELEASE})
ERRORS +=	"${OS} version ${OS_RELEASE} is not supported"
.endif

FETCH_URI ?=	${FETCH_URI_${OS}}
FETCH_DIR ?=	${HOME}/${OS}/${OS_RELEASE}
PATCH_DIR =	${.CURDIR}/${OS}/${OS_RELEASE}
MAKE_CMD =	env ${MAKE_ENV_${OS}} ${.MAKE}

GENERATE_UNPACK_CAT_CMD = \
	unpack_cat() { \
		echo "Unpacking $${1##*/}:" >&2; \
		case $$1 in \
		*.gz|*.tgz)	unpacker=gzcat;; \
		*.bz2|*.tbz)	unpacker=bzcat;; \
		*.xz|*.txz)	unpacker=xzcat;; \
		*)		unpacker=cat;; \
		esac; \
		if [ -t 2 -a -x /usr/bin/ftp -a OpenBSD = "${OS}" ]; then \
			/usr/bin/ftp -mo - "file://$$1" | $unpacker; \
		elif curl=$$(command -v curl || true); [ -t 2 -a -n "$curl"  ]; then \
			"$curl" -o - "file://$$1" | $unpacker; \
		else \
			$unpacker <"$$1"; \
		fi; \
	}

.PHONY: all base build clean distclean fetch kernel install patch unpack unpatch
.PHONY: kernel-config kernel-headers
.MAIN: all

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

all: ${_c}

.PHONY: build-${_c} clean-${_c} distclean-${_c} fetch-${_c} install-${_c}
.PHONY: patch-${_c} unpack-${_c} unpatch-${_c}
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

patch: patch-${_c}
patch-${_c}: ${PATCH_${_c}_COOKIE}
${PATCH_${_c}_COOKIE}: ${UNPACK_${_c}_COOKIE}
	@test ! -e ${PATCH_INDICATOR_${_c}} || \
	    { echo called $@ in patched directory ${SRC_DIR_${_c}} >&2; exit 1; }
	cd ${SRC_DIR_${_c}}; \
	patch -C <${PATCH_DIR}/cw-${_c}.patch; \
	patch -E -s <${PATCH_DIR}/cw-${_c}.patch
	@touch $@

unpatch: unpatch-${_c}
unpatch-${_c}:
	@test -e ${PATCH_INDICATOR_${_c}} || \
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

.if !empty(ERRORS)
.BEGIN:
. for _e in ${ERRORS}
	@printf "Error: %s\n" ${_e} >&2
	@false
. endfor
.endif
