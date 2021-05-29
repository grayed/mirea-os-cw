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

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <mntopts.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <util.h>

#include "mount_cwfs.h"

/* --------------------------------------------------------------------- */

static const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_WXALLOWED,
	MOPT_UPDATE,
	{ NULL },
};

/* --------------------------------------------------------------------- */

static void	usage(void) __dead;
static uid_t	a_uid(const char *);
static gid_t	a_gid(const char *);
static uid_t	a_gid(const char *);
static int	a_num(const char *, const char *);
static mode_t	a_mask(const char *);
static void	pathadj(const char *, char *);

/* --------------------------------------------------------------------- */

void
mount_cwfs_parseargs(int argc, char *argv[],
	struct cwfs_args *args, int *mntflags,
	char *canon_dev, char *canon_dir)
{
	int gidset, modeset, uidset; /* Ought to be 'bool'. */
	int ch;
	gid_t gid;
	uid_t uid;
	mode_t mode;
	int64_t cwnumber;
	struct stat sb;

	/* Set default values for mount point arguments. */
	memset(args, 0, sizeof(*args));
	args->cwa_version = CWFS_ARGS_VERSION;
	*mntflags = 0;

	gidset = 0; gid = 0;
	uidset = 0; uid = 0;
	modeset = 0; mode = 0;

	optind = optreset = 1;
	while ((ch = getopt(argc, argv, "g:m:n:o:s:u:")) != -1 ) {
		switch (ch) {
		case 'g':
			gid = a_gid(optarg);
			gidset = 1;
			break;

		case 'm':
			mode = a_mask(optarg);
			modeset = 1;
			break;

		case 'o':
			getmntopts(optarg, mopts, mntflags);
			break;

		case 'u':
			uid = a_uid(optarg);
			uidset = 1;
			break;

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	strlcpy(canon_dev, argv[0], PATH_MAX);
	pathadj(argv[1], canon_dir);

	if (stat(canon_dir, &sb) == -1)
		err(EXIT_FAILURE, "cannot stat `%s'", canon_dir);

	args->cwa_root_uid = uidset ? uid : sb.st_uid;
	args->cwa_root_gid = gidset ? gid : sb.st_gid;
	args->cwa_root_mode = modeset ? mode : sb.st_mode;
}

/* --------------------------------------------------------------------- */

static void
usage(void)
{
	extern char *__progname;
	(void)fprintf(stderr,
	    "usage: %s [-g group] [-m mode] [-n nodes] [-o options] [-s size]\n"
	    "           [-u user] cwfs mount_point\n", __progname);
	exit(1);
}

/* --------------------------------------------------------------------- */

int
mount_cwfs(int argc, char *argv[])
{
	struct cwfs_args args;
	char canon_dev[PATH_MAX], canon_dir[PATH_MAX];
	int mntflags;

	mount_cwfs_parseargs(argc, argv, &args, &mntflags,
	    canon_dev, canon_dir);

	if (mount(MOUNT_CWFS, canon_dir, mntflags, &args) == -1)
		err(EXIT_FAILURE, "cwfs on %s", canon_dir);

	return EXIT_SUCCESS;
}

int
main(int argc, char *argv[])
{

	/* setprogname(argv[0]); */
	return mount_cwfs(argc, argv);
}

static uid_t
a_uid(const char *s)
{
	struct passwd *pw;

	if ((pw = getpwnam(s)) != NULL)
		return pw->pw_uid;
	return a_num(s, "user");
}

static gid_t
a_gid(const char *s)
{
	struct group *gr;

	if ((gr = getgrnam(s)) != NULL)
		return gr->gr_gid;
	return a_num(s, "group");
}

static int
a_num(const char *s, const char *id_type)
{
	int id;
	char *ep;

	id = strtol(s, &ep, 0);
	if (*ep || s == ep || id < 0)
		errx(1, "unknown %s id: %s", id_type, s);
	return id;
}

static mode_t
a_mask(const char *s)
{
	int rv;
	char *ep;

	rv = strtol(s, &ep, 8);
	if (s == ep || *ep || rv < 0)
		errx(1, "invalid file mode: %s", s);
	return rv;
}

static void
pathadj(const char *input, char *adjusted)
{

	if (realpath(input, adjusted) == NULL)
		err(1, "realpath %s", input);
	if (strncmp(input, adjusted, PATH_MAX)) {
		warnx("\"%s\" is a relative path.", input);
		warnx("using \"%s\" instead.", adjusted);
	}
}
