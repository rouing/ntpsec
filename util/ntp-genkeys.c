/*
 * Program to generate MD5 and RSA keys for NTP clients and servers
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>		/* PATH_MAX */
#include <sys/stat.h>		/* for umask() */
#include <sys/time.h>

#ifdef HAVE_NETINFO
#include <netinfo/ni.h>
#endif

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_string.h"
#include "ntp_filegen.h"
#include "ntp_unixtime.h"
#include "ntp_config.h"

#ifdef PUBKEY
# include "ntp_crypto.h"
#endif

#ifndef PATH_MAX
# ifdef _POSIX_PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
# else
#  define PATH_MAX 255
# endif
#endif

/*
 * Cryptodefines
 */
#define MAXKEYLEN	1024	/* maximum encoded key length */
#define MODULUSLEN	512	/* length of RSA modulus */
#define PRIMELEN	512	/* length of D_H prime, generator */

/*
 * This program generates (up to) four files:
 *
 *	ntp.keys    containing the DES/MD5 private keys,
 *	ntpkey      containing the RSA private key,
 *	ntpkey_HOST containing the RSA public key
 *		     where HOST is the DNS name of the generating machine,
 *	ntpkey_dh   containing the parameters for the Diffie-Hellman
 *		    key-agreement algorithm.
 *
 * The files contain cryptographic values generated by the algorithms of
 * the rsaref20 package and are in printable ASCII format.  Since the
 * algorythms are seeded by the system clock, each run of this program
 * will produce a different outcome.  There are no options or frills of
 * any sort, although a number of options would seem to be appropriate.
 * Waving this program in the breeze will no doubt bring a cast of
 * thousands to wiggle the options this way and that for various useful
 * purposes.
 *
 * The names of all files begin with "ntp" and end with an extension
 * consisting of the seconds value of the current NTP timestamp, which
 * appears in the form ".*".  This provides a way to distinguish between
 * key generations, since the host name and timestamp can be fetched by
 * a client during operation.
 *
 * The ntp.keys.* file contains 16 MD5 keys.  Each key consists of 16
 * characters randomized over the ASCII 95-character printing subset.
 * The file is read by the daemon at the location specified by the keys
 * configuration file command and made visible only to root.  An
 * additional key consisting of a easily remembered password should be
 * added by hand for use with the ntpdc program.  The file must be
 * distributed by secure means to other servers and clients sharing the
 * same security compartment.
 *
 * The key identifiers for MD5 and DES keys must be less than 65536,
 * although this program uses only the identifiers from 1 to 16.  The key
 * identifier for each association is specified as the key argument in
 * the server or peer configuration file command.
 *
 * The ntpkey.* file contains the RSA private key.  It is read by the
 * daemon at the location specified by the private argument of the
 * crypto configuration file command and made visible only to root.
 * This file is useful only to the machine that generated it and never
 * shared with any other daemon or application program.
 *
 * The ntpkey_host.* file contains the RSA public key, where host is the
 * DNS name of the host that generated it.  The file is read by the
 * daemon at the location specified by the public argument to the server
 * or peer configuration file command.  This file can be widely
 * distributed and stored without using secure means, since the data are
 * public values.
 *
 * The ntp_dh.* file contains two Diffie-Hellman parameters, the prime
 * modulus and the generator.  The file is read by the daemon at the
 * location specified by the dhparams argument of the crypto
 * configuration file command.  This file can be widely distributed and
 * stored without using secure means, since the data are public values.
 *
 * The file formats all begin with two lines.  The first line contains
 * the file name and decimal timestamp, while the second contains the
 * readable datestamp.  Lines beginning with # are considered comments
 * and ignored by the daemon.  In the ntp.keys.* file, the next 16 lines
 * contain the MD5 keys in order.  In the ntpkey.* and ntpkey_host.*
 * files, the next line contains the modulus length in bits followed by
 * the key as a PEM encoded string.  In the ntpkey_dh.* file, the next
 * line contains the prime length in bytes followed by the prime as a
 * PEM encoded string, and the next and final line contains the
 * generator length in bytes followed by the generator as a PEM encoded
 * string.
 *
 * Note: See the file ./source/rsaref.h in the rsaref20 package for
 * explanation of return values, if necessary.
 */


char *config_file;

#ifdef HAVE_NETINFO
struct netinfo_config_state *config_netinfo = NULL;
int check_netinfo = 1;
#endif /* HAVE_NETINFO */

#ifdef SYS_WINNT
char *alt_config_file;
LPTSTR temp;
char config_file_storage[MAX_PATH];
char alt_config_file_storage[MAX_PATH];
#endif /* SYS_WINNT */

int make_dh = 0;		/* Make D-H parameter file? */
int make_md5 = 0;		/* Make MD5 keyfile? */
int make_rsa = 0;		/* Make RSA pair? */
int force = 0;			/* Force the installation? */
int nosymlinks = 0;		/* Just create the (timestamped) files? */
int trash = 0;			/* Trash old files? */
int errflag = 0;

char *path_keys;
char *link_keys;

char *path_keysdir = NTP_KEYSDIR;
char *link_keysdir;

char *path_publickey;
char *link_publickey;

char *path_privatekey;
char *link_privatekey;

char *path_dhparms;
char *link_dhparms;


/* Stubs and hacks so we can link with ntp_config.o */
u_long	sys_automax;		/* maximum session key lifetime */
int	sys_bclient;		/* we set our time to broadcasts */
int	sys_manycastserver;	/* 1 => respond to manycast client pkts */
u_long	client_limit_period;
char *	req_file;		/* name of the file with configuration info */
keyid_t	ctl_auth_keyid;		/* keyid used for authenticating write requests */
struct interface *any_interface;	/* default interface */
keyid_t	info_auth_keyid;	/* keyid used to authenticate requests */
u_long	current_time;		/* current time (s) */
const char *Version = "";	/* version declaration */
keyid_t	req_keyid;		/* request keyid */
u_long	client_limit;
u_long	client_limit_period;
l_fp	sys_revoketime;
u_long	sys_revoke;		/* keys revoke timeout */
volatile int debug = 1;		/* debugging flag */

struct peer *
peer_config(
	struct sockaddr_in *srcadr,
	struct interface *dstadr,
	int hmode,
	int version,
	int minpoll,
	int maxpoll,
	u_int flags,
	int ttl,
	keyid_t key,
	u_char *keystr
	)
{
	if (debug) printf("peer_config...\n");
	return 0;
}


void
set_sys_var(
	char *data,
	u_long size,
	int def
	)
{
	if (debug) printf("set_sys_var...\n");
	return;
}


void
ntp_intres (void)
{
	if (debug) printf("ntp_intres...\n");
	return;
}


int
ctlsettrap(
	struct sockaddr_in *raddr,
	struct interface *linter,
	int traptype,
	int version
	)
{
	if (debug) printf("ctlsettrap...\n");
	return 0;
}


#ifdef PUBKEY
void
crypto_config(
	int item,		/* configuration item */
	char *cp		/* file name */
	)
{
	switch (item) {
	    case CRYPTO_CONF_DH:
		if (debug) printf("crypto_config: DH/<%d> <%s>\n", item, cp);
		path_dhparms = strdup(cp);
		break;
	    case CRYPTO_CONF_PRIV:
		if (debug) printf("crypto_config: PRIVATEKEY/<%d> <%s>\n", item, cp);
		path_privatekey = strdup(cp);
		break;
	    case CRYPTO_CONF_PUBL:
		if (debug) printf("crypto_config: PUBLICKEY/<%d> <%s>\n", item, cp);
		path_publickey = strdup(cp);
		break;
	    default:
		if (debug) printf("crypto_config: <%d> <%s>\n", item, cp);
		break;
	}
	return;
}
#endif


struct interface *
findinterface(
	struct sockaddr_in *addr
	)
{
 	if (debug) printf("findinterface...\n");
	return 0;
}


void
refclock_control(
	struct sockaddr_in *srcadr,
	struct refclockstat *in,
	struct refclockstat *out
	)
{
	if (debug) printf("refclock_control...\n");
	return;
}


void
loop_config(
	int item,
	double freq
	)
{
	if (debug) printf("loop_config...\n");
	return;
}


void
filegen_config(
	FILEGEN *gen,
	char    *basename,
	u_int   type,
	u_int   flag
	)
{
	if (debug) printf("filegen_config...\n");
	return;
}


void
stats_config(
	int item,
	char *invalue	/* only one type so far */
	)
{
	if (debug) printf("stats_config...\n");
	return;
}


void
hack_restrict(
	int op,
	struct sockaddr_in *resaddr,
	struct sockaddr_in *resmask,
	int mflags,
	int flags
	)
{
	if (debug) printf("hack_restrict...\n");
	return;
}


void
kill_asyncio (void)
{
	if (debug) printf("kill_asyncio...\n");
	return;
}


void
proto_config(
	int item,
	u_long value,
	double dvalue
	)
{
	if (debug) printf("proto_config...\n");
	return;
}

void
getauthkeys(
	char *keyfile
	)
{
	if (debug) printf("getauthkeys: got <%s>\n", keyfile);
	path_keys = strdup(keyfile);
	return;
}


FILEGEN *
filegen_get(
	char *name
	)
{
	if (debug) printf("filegen_get...\n");
	return 0;
}


/* End of stubs and hacks */


static void
usage(
	void
	)
{
	printf("Usage: %s [ -c ntp.conf ] [ -k key_file ]\n", progname);
	printf("       [ -f ] [ -l ] [ -t ] [ -d ] [ -m ] [ -r ]\n");
	printf(" where:\n");
	printf("  -c /etc/ntp.conf   Location of ntp.conf file\n");
	printf("  -k key_file        Location of key file\n");
	printf("  -f     force installation of generated keys.\n");
	printf("  -l     Don't make the symlinks\n");
	printf("  -t     Trash the (old) files at the end of symlink\n");
	printf("  -d     Generate D-H parameter file\n");
	printf("  -m     Generate MD5 key file\n");
	printf("  -r     Generate RSA keys\n");

	exit(1);
}


void
getCmdOpts (
	int argc,
	char *argv[]
	)
{
	int i;

	while ((i = ntp_getopt(argc, argv, "c:dflmrt")) != EOF)
		switch (i) {
		    case 'c':
			config_file = ntp_optarg;
#ifdef HAVE_NETINFO
			check_netinfo = 0;
#endif
			break;
		    case 'd':
			++make_dh;
			break;
		    case 'f':
			++force;
			break;
		    case 'l':
			++nosymlinks;
			break;
		    case 'm':
			++make_md5;
			break;
		    case 'r':
			++make_rsa;
			break;
		    case 't':
			++trash;
			break;
		    case '?':
			++errflag;
			break;
		}

	if (errflag)
		usage();

	/* If no file type was specified, make them all. */
	if (!(make_dh | make_md5 | make_rsa)) {
		++make_dh;
		++make_md5;
		++make_rsa;
	}
}


void
snifflink(
	const char *file,
	char **linkdata
	)
{
#ifdef HAVE_READLINK
	char buf[PATH_MAX];
	int rc;

	if (!file)
		return;

	rc = readlink(file, buf, sizeof buf);
	if (-1 == rc) {
		switch (errno) {
		    case EINVAL:	/* Fall thru */
		    case ENOENT:
			return;
		}
		fprintf(stderr, "%s: readlink(%s) failed: (%d) %s\n",
			progname, file, errno, strerror(errno));
		exit(1);
	}
	buf[rc] = '\0';
	*linkdata = strdup(buf);
	/* XXX: make sure linkdata is not 0... */
#endif /* not HAVE_READLINK */
	return;
}

int
main(
	int argc,
	char *argv[]
	)
{
#ifdef PUBKEY
	R_RSA_PRIVATE_KEY rsaref_private; /* RSA private key */
	R_RSA_PUBLIC_KEY rsaref_public;	/* RSA public key */
	R_RSA_PROTO_KEY protokey;	/* RSA prototype key */
	R_DH_PARAMS dh_params;		/* Diffie-Hellman parameters */
	R_RANDOM_STRUCT randomstr;	/* random structure */
	int rval;			/* return value */
	u_char encoded_key[MAXKEYLEN];	/* encoded PEM string buffer */
	u_int modulus;			/* modulus length */
	u_int len;
#endif /* PUBKEY */
	struct timeval tv;		/* initialization vector */
	u_long ntptime;			/* NTP timestamp */
	u_char hostname[256];		/* DNS host name */
	u_char filename[256];		/* public key file name */
	u_char md5key[17];		/* generated MD5 key */ 
	FILE *str;			/* file handle */
	u_int temp;
	int i, j;
	mode_t std_mask;	/* Standard mask */
	mode_t sec_mask = 077;	/* Secure mask */
	char pathbuf[PATH_MAX];

	gethostname(hostname, sizeof(hostname));
	gettimeofday(&tv, 0);
	ntptime = tv.tv_sec + JAN_1970;

	/* Initialize config_file */
	getconfig(argc, argv);	/* ntpd/ntp_config.c */

	if (!path_keys) {
		/* Shouldn't happen... */
		path_keys = "PATH_KEYS";
	}
	if (*path_keys != '/') {
		fprintf(stderr,
			"%s: keys path <%s> doesn't begin with a /\n",
			progname, path_keys);
		exit(1);
	}
	snifflink(path_keys, &link_keys);

	if (!path_keysdir) {
		/* Shouldn't happen... */
		path_keysdir = "PATH_KEYSDIR";
	}
	if (*path_keysdir != '/') {
		fprintf(stderr,
			"%s: keysdir path <%s> doesn't begin with a /\n",
			progname, path_keysdir);
		exit(1);
	}
	snifflink(path_keysdir, &link_keysdir);

	if (!link_publickey) {
		snprintf(pathbuf, sizeof pathbuf, "ntpkey_%s.%lu",
			 hostname, ntptime);
		link_publickey = strdup(pathbuf);
	}
	if (!path_publickey) {
		snprintf(pathbuf, sizeof pathbuf, "%s/ntpkey_%s",
			 path_keysdir, hostname);
		path_publickey = strdup(pathbuf);
	}
	if (*path_publickey != '/') {
		fprintf(stderr,
			"%s: publickey path <%s> doesn't begin with a /\n",
			progname, path_publickey);
		exit(1);
	}
	snifflink(path_publickey, &link_publickey);

	if (!link_privatekey) {
		snprintf(pathbuf, sizeof pathbuf, "ntpkey.%lu",
			 ntptime);
		link_privatekey = strdup(pathbuf);
	}
	if (!path_privatekey) {
		snprintf(pathbuf, sizeof pathbuf, "%s/ntpkey",
			 path_keysdir);
		path_privatekey = strdup(pathbuf);
	}
	if (*path_privatekey != '/') {
		fprintf(stderr,
			"%s: privatekey path <%s> doesn't begin with a /\n",
			progname, path_privatekey);
		exit(1);
	}
	snifflink(path_privatekey, &link_privatekey);

	if (!link_dhparms) {
		snprintf(pathbuf, sizeof pathbuf, "ntpkey_dh.%lu",
			 ntptime);
		link_dhparms = strdup(pathbuf);
	}
	if (!path_dhparms) {
		snprintf(pathbuf, sizeof pathbuf, "%s/ntpkey_dh",
			 path_keysdir);
		path_dhparms = strdup(pathbuf);
	}
	if (*path_dhparms != '/') {
		fprintf(stderr,
			"%s: dhparms path <%s> doesn't begin with a /\n",
			progname, path_dhparms);
		exit(1);
	}
	snifflink(path_dhparms, &link_dhparms);

	printf("After config:\n");
	printf("path_keys       = <%s> -> <%s>\n"
	       , path_keys? path_keys: ""
	       , link_keys? link_keys: ""
		);
	printf("path_keysdir    = <%s> -> <%s>\n"
	       , path_keysdir? path_keysdir: ""
	       , link_keysdir? link_keysdir: ""
		);
	printf("path_publickey  = <%s> -> <%s>\n"
	       , path_publickey? path_publickey: ""
	       , link_publickey? link_publickey: ""
		);
	printf("path_privatekey = <%s> -> <%s>\n"
	       , path_privatekey? path_privatekey: ""
	       , link_privatekey? link_privatekey: ""
		);
	printf("path_dhparms = <%s> -> <%s>\n"
	       , path_dhparms? path_dhparms: ""
	       , link_dhparms? link_dhparms: ""
		);

	/*
	  We:
	  - get each target filename
	  - if it exists, if it's a symlink get the "target"
	  - for each file we're going to install:
	  - - build the new timestamped file
	  - - install it with the timestamp suffix
	  - - If it's OK to make links:
	  - - - remove any old link
	  - - - make any needed directories?
	  - - - make the link
	  - - - remove the old file (if (trash))

	  We'll probably learn about how the link should be installed.
	  We will start by using fully-rooted paths, but we should use
	  whatever information we have from the old link.
	*/

	std_mask = umask(sec_mask); /* Get the standard mask */

	/*
	 * Generate 16 random MD5 keys.
	 */
	printf("Generating MD5 key file...\n");
	sprintf(filename, "ntp.keys.%lu", ntptime);
	str = fopen(filename, "w");
	if (str == NULL) {
		perror("MD5 key file");
		return (-1);
	}
	srandom((u_int)tv.tv_usec);
	fprintf(str, "# MD5 key file %s\n# %s", filename,
	   ctime(&tv.tv_sec));
	for (i = 1; i <= 16; i++) {
		for (j = 0; j < 16; j++) {
			while (1) {
				temp = random() & 0xff;
				if (temp > 0x20 && temp < 0x7f)
					break;
			}
			md5key[j] = (u_char)temp;
		}
		md5key[16] = 0;
		fprintf(str, "%2d M %16s	# MD5 key\n", i,
		    md5key);
	}
	fclose(str);

#ifdef PUBKEY
	/*
	 * Roll the RSA public/private key pair.
	 */
	printf("Generating RSA public/private key pair (%d bits)...\n",
	    MODULUSLEN);
	protokey.bits = MODULUSLEN;
	protokey.useFermat4 = 1;
	R_RandomInit(&randomstr);
	R_GetRandomBytesNeeded(&len, &randomstr);
	for (i = 0; i < len; i++) {
		temp = random();
		R_RandomUpdate(&randomstr, (u_char *)&temp, 1);
	}
	rval = R_GeneratePEMKeys(&rsaref_public, &rsaref_private,
	    &protokey, &randomstr);
	if (rval) {
		printf("R_GeneratePEMKeys error %x\n", rval);
		return (-1);
	}

	/*
	 * Generate the file "ntpkey.*" containing the RSA private key in
	 * printable ASCII format.
	 */
	sprintf(filename, "ntpkey.%lu", ntptime);
	str = fopen(filename, "w");
	if (str == NULL) { 
		perror("RSA private key file");
		return (-1);
	}
	len = sizeof(rsaref_private) - sizeof(rsaref_private.bits);
	modulus = (u_int32)rsaref_private.bits;
	fprintf(str, "# RSA private key file %s\n# %s", filename,
	    ctime(&tv.tv_sec));
	R_EncodePEMBlock(encoded_key, &temp,
	    (u_char *)rsaref_private.modulus, len);
	encoded_key[temp] = '\0';
	fprintf(str, "%d %s\n", modulus, encoded_key);
	fclose(str);

	/*
	 * Generate the file "ntpkey_host.*" containing the RSA public key
	 * in printable ASCII format.
	 */
	sprintf(filename, "ntpkey_%s.%lu", hostname, ntptime);
	str = fopen(filename, "w");
	if (str == NULL) { 
		perror("RSA public key file");
		return (-1);
	}
	len = sizeof(rsaref_public) - sizeof(rsaref_public.bits);
	modulus = (u_int32)rsaref_public.bits;
	fprintf(str, "# RSA public key file %s\n# %s", filename,
	    ctime(&tv.tv_sec));
	R_EncodePEMBlock(encoded_key, &temp,
	    (u_char *)rsaref_public.modulus, len);
	encoded_key[temp] = '\0';
	fprintf(str, "%d %s\n", modulus, encoded_key);
	fclose(str);
#endif /* PUBKEY */

#ifdef PUBKEY
	/*
	 * Roll the prime and generator for the Diffie-Hellman key
	 * agreement algorithm.
	 */
	printf("Generating Diffie-Hellman parameters (%d bits)...\n",
	    PRIMELEN);
	R_RandomInit(&randomstr);
	R_GetRandomBytesNeeded(&len, &randomstr);
	for (i = 0; i < len; i++) {
		temp = random();
		R_RandomUpdate(&randomstr, (u_char *)&temp, 1);
	}

	/*
	 * Generate the file "ntpkey_dh.*" containing the Diffie-Hellman
	 * prime and generator in printable ASCII format.
	 */
	len = DH_PRIME_LEN(PRIMELEN);
	dh_params.prime = (u_char *)malloc(len);
	dh_params.generator = (u_char *)malloc(len);
	rval = R_GenerateDHParams(&dh_params, PRIMELEN, PRIMELEN / 2,
	    &randomstr);
	if (rval) {
		printf("R_GenerateDHParams error %x\n", rval);
		return (-1);
	}
	sprintf(filename, "ntpkey_dh.%lu", ntptime);
	str = fopen(filename, "w");
	if (str == NULL) { 
		perror("Diffie-Hellman parameters file");
		return (-1);
	}
	fprintf(str, "# Diffie-Hellman parameter file %s\n# %s", filename,
	    ctime(&tv.tv_sec));
	R_EncodePEMBlock(encoded_key, &temp,
	    (u_char *)dh_params.prime, dh_params.primeLen);
	encoded_key[temp] = '\0';
	fprintf(str, "%d %s\n", dh_params.primeLen, encoded_key);
	R_EncodePEMBlock(encoded_key, &temp,
	    (u_char *)dh_params.generator, dh_params.generatorLen);
	encoded_key[temp] = '\0';
	fprintf(str, "%d %s\n", dh_params.generatorLen, encoded_key);
	fclose(str);
#endif /* PUBKEY */

	return (0);
}
