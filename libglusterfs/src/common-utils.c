/*
  Copyright (c) 2006-2009 Gluster, Inc. <http://www.gluster.com>
  This file is part of GlusterFS.

  GlusterFS is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; either version 3 of the License,
  or (at your option) any later version.

  GlusterFS is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see
  <http://www.gnu.org/licenses/>.
*/

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "logging.h"
#include "common-utils.h"
#include "revision.h"
#include "glusterfs.h"
#include "stack.h"

typedef int32_t (*rw_op_t)(int32_t fd, char *buf, int32_t size);
typedef int32_t (*rwv_op_t)(int32_t fd, const struct iovec *buf, int32_t size);


struct dnscache6 {
	struct addrinfo *first;
	struct addrinfo *next;
};


int
log_base2 (unsigned long x)
{
        int val = 0;

        while (x > 1) {
                x /= 2;
                val++;
        }

        return val;
}


int32_t
gf_resolve_ip6 (const char *hostname, 
		uint16_t port, 
		int family, 
		void **dnscache, 
		struct addrinfo **addr_info)
{
	int32_t ret = 0;
	struct addrinfo hints;
	struct dnscache6 *cache = NULL;
	char service[NI_MAXSERV], host[NI_MAXHOST];

	if (!hostname) {
		gf_log ("resolver", GF_LOG_WARNING, "hostname is NULL");
		return -1;
	}

	if (!*dnscache) {
		*dnscache = GF_CALLOC (1, sizeof (struct dnscache6),
                                        gf_common_mt_dnscache6);
                if (!*dnscache)
                        return -1;
	}

	cache = *dnscache;
	if (cache->first && !cache->next) {
		freeaddrinfo(cache->first);
		cache->first = cache->next = NULL;
		gf_log ("resolver", GF_LOG_TRACE,
			"flushing DNS cache");
	}

	if (!cache->first) {
		char *port_str = NULL;
		gf_log ("resolver", GF_LOG_TRACE,
			"DNS cache not present, freshly probing hostname: %s",
			hostname);

		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = family;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags    = AI_ADDRCONFIG;

		ret = gf_asprintf (&port_str, "%d", port);
                if (-1 == ret) {
                        gf_log ("resolver", GF_LOG_ERROR, "asprintf failed");
                        return -1;
                }
		if ((ret = getaddrinfo(hostname, port_str, &hints, &cache->first)) != 0) {
			gf_log ("resolver", GF_LOG_ERROR,
				"getaddrinfo failed (%s)", gai_strerror (ret));

			GF_FREE (*dnscache);
			*dnscache = NULL;
			GF_FREE (port_str);
			return -1;
		}
		GF_FREE (port_str);

		cache->next = cache->first;
	}

	if (cache->next) {
		ret = getnameinfo((struct sockaddr *)cache->next->ai_addr,
				  cache->next->ai_addrlen,
				  host, sizeof (host),
				  service, sizeof (service),
				  NI_NUMERICHOST);
		if (ret != 0) {
			gf_log ("resolver",
				GF_LOG_ERROR,
				"getnameinfo failed (%s)", gai_strerror (ret));
			goto err;
		}

		gf_log ("resolver", GF_LOG_TRACE,
			"returning ip-%s (port-%s) for hostname: %s and port: %d",
			host, service, hostname, port);

		*addr_info = cache->next;
	}

        if (cache->next)
                cache->next = cache->next->ai_next;
	if (cache->next) {
		ret = getnameinfo((struct sockaddr *)cache->next->ai_addr,
				  cache->next->ai_addrlen,
				  host, sizeof (host),
				  service, sizeof (service),
				  NI_NUMERICHOST);
		if (ret != 0) {
			gf_log ("resolver",
				GF_LOG_ERROR,
				"getnameinfo failed (%s)", gai_strerror (ret));
			goto err;
		}

		gf_log ("resolver", GF_LOG_TRACE,
			"next DNS query will return: ip-%s port-%s", host, service);
	}

	return 0;

err:
	freeaddrinfo (cache->first);
	cache->first = cache->next = NULL;
	GF_FREE (cache);
	*dnscache = NULL;
	return -1;
}


void
gf_log_volume_file (FILE *specfp)
{
	extern FILE *gf_log_logfile;
	int          lcount = 0;
	char         data[GF_UNIT_KB];
	
	fseek (specfp, 0L, SEEK_SET);
	
	fprintf (gf_log_logfile, "Given volfile:\n");
	fprintf (gf_log_logfile, 
		 "+---------------------------------------"
		 "---------------------------------------+\n");
	while (fgets (data, GF_UNIT_KB, specfp) != NULL){
		lcount++;
		fprintf (gf_log_logfile, "%3d: %s", lcount, data);
	}
	fprintf (gf_log_logfile, 
		 "\n+---------------------------------------"
		 "---------------------------------------+\n");
	fflush (gf_log_logfile);
	fseek (specfp, 0L, SEEK_SET);
}

static void 
gf_dump_config_flags (int fd)
{
        int ret = 0;
        /* TODO: 'ret' is not checked properly, add this later */

	ret = write (fd, "configuration details:\n", 23);

/* have argp */
#ifdef HAVE_ARGP
	ret = write (fd, "argp 1\n", 7);
#endif

/* ifdef if found backtrace */
#ifdef HAVE_BACKTRACE 
	ret = write (fd, "backtrace 1\n", 12);
#endif

/* Berkeley-DB version has cursor->get() */
#ifdef HAVE_BDB_CURSOR_GET 
	ret = write (fd, "bdb->cursor->get 1\n", 19);
#endif

/* Define to 1 if you have the <db.h> header file. */
#ifdef HAVE_DB_H 
	ret = write (fd, "db.h 1\n", 7);
#endif

/* Define to 1 if you have the <dlfcn.h> header file. */
#ifdef HAVE_DLFCN_H 
	ret = write (fd, "dlfcn 1\n", 8);
#endif

/* define if fdatasync exists */
#ifdef HAVE_FDATASYNC 
	ret = write (fd, "fdatasync 1\n", 12);
#endif

/* Define to 1 if you have the `pthread' library (-lpthread). */
#ifdef HAVE_LIBPTHREAD 
	ret = write (fd, "libpthread 1\n", 13);
#endif

/* define if llistxattr exists */
#ifdef HAVE_LLISTXATTR 
	ret = write (fd, "llistxattr 1\n", 13);
#endif

/* define if found setfsuid setfsgid */
#ifdef HAVE_SET_FSID 
	ret = write (fd, "setfsid 1\n", 10);
#endif

/* define if found spinlock */
#ifdef HAVE_SPINLOCK 
	ret = write (fd, "spinlock 1\n", 11);
#endif

/* Define to 1 if you have the <sys/epoll.h> header file. */
#ifdef HAVE_SYS_EPOLL_H 
	ret = write (fd, "epoll.h 1\n", 10);
#endif

/* Define to 1 if you have the <sys/extattr.h> header file. */
#ifdef HAVE_SYS_EXTATTR_H 
	ret = write (fd, "extattr.h 1\n", 12);
#endif

/* Define to 1 if you have the <sys/xattr.h> header file. */
#ifdef HAVE_SYS_XATTR_H 
	ret = write (fd, "xattr.h 1\n", 10);
#endif

/* define if found st_atim.tv_nsec */
#ifdef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
	ret = write (fd, "st_atim.tv_nsec 1\n", 18);
#endif

/* define if found st_atimespec.tv_nsec */
#ifdef HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC
	ret = write (fd, "st_atimespec.tv_nsec 1\n",23);
#endif

/* Define to the full name and version of this package. */
#ifdef PACKAGE_STRING 
	{
		char msg[128];
		sprintf (msg, "package-string: %s\n", PACKAGE_STRING); 
		ret = write (fd, msg, strlen (msg));
	}
#endif

	return;
}

/* Obtain a backtrace and print it to stdout. */
/* TODO: It looks like backtrace_symbols allocates memory,
   it may be problem because mostly memory allocation/free causes 'sigsegv' */
void
gf_print_trace (int32_t signum)
{
	extern FILE *gf_log_logfile;
        struct tm   *tm = NULL;
        char         msg[1024] = {0,};
        char         timestr[256] = {0,};
        time_t       utime = 0;
        int          ret = 0;
        int          fd = 0;

        fd = fileno (gf_log_logfile);

	/* Pending frames, (if any), list them in order */
	ret = write (fd, "pending frames:\n", 16);
	{
		glusterfs_ctx_t *ctx = glusterfs_ctx_get ();
		struct list_head *trav = ((call_pool_t *)ctx->pool)->all_frames.next;
		while (trav != (&((call_pool_t *)ctx->pool)->all_frames)) {
			call_frame_t *tmp = (call_frame_t *)(&((call_stack_t *)trav)->frames);
			if (tmp->root->type == GF_OP_TYPE_FOP)
				sprintf (msg,"frame : type(%d) op(%s)\n",
					 tmp->root->type,
					 gf_fop_list[tmp->root->op]);
			if (tmp->root->type == GF_OP_TYPE_MGMT)
				sprintf (msg,"frame : type(%d) op(%s)\n",
					 tmp->root->type,
					 gf_mgmt_list[tmp->root->op]);

			ret = write (fd, msg, strlen (msg));
			trav = trav->next;
		}
		ret = write (fd, "\n", 1);
	}

	sprintf (msg, "patchset: %s\n", GLUSTERFS_REPOSITORY_REVISION);
	ret = write (fd, msg, strlen (msg));

	sprintf (msg, "signal received: %d\n", signum);
	ret = write (fd, msg, strlen (msg));

        {
                /* Dump the timestamp of the crash too, so the previous logs
                   can be related */
                utime = time (NULL);
                tm    = localtime (&utime);
                strftime (timestr, 256, "%Y-%m-%d %H:%M:%S\n", tm);
                ret = write (fd, "time of crash: ", 15);
                ret = write (fd, timestr, strlen (timestr));
        }

	gf_dump_config_flags (fd);
#if HAVE_BACKTRACE
	/* Print 'backtrace' */
	{
		void *array[200];
		size_t size;

		size = backtrace (array, 200);
		backtrace_symbols_fd (&array[1], size-1, fd);
		sprintf (msg, "---------\n");
		ret = write (fd, msg, strlen (msg));
	}
#endif /* HAVE_BACKTRACE */
  
	/* Send a signal to terminate the process */
	signal (signum, SIG_DFL);
	raise (signum);
}

void
trap (void)
{

}

char *
gf_trim (char *string)
{
	register char *s, *t;
  
	if (string == NULL)
	{
		return NULL;
	}
  
	for (s = string; isspace (*s); s++)
		;
  
	if (*s == 0)
		return s;
  
	t = s + strlen (s) - 1;
	while (t > s && isspace (*t))
		t--;
	*++t = '\0';
  
	return s;
}

int 
gf_strsplit (const char *str, const char *delim, 
	     char ***tokens, int *token_count)
{
	char *_running = NULL;
	char *running = NULL;
	char *token = NULL;
	char **token_list = NULL;
	int count = 0;
	int i = 0;
	int j = 0;
  
	if (str == NULL || delim == NULL || tokens == NULL || token_count == NULL)
	{
		return -1;
	}
  
        _running = gf_strdup (str);
	if (_running == NULL)
	{
		return -1;
	}
	running = _running;
  
	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] != '\0')
			count++;
	}
	GF_FREE (_running);
  
        _running = gf_strdup (str);
	if (_running == NULL)
	{
		return -1;
	}
	running = _running;
  
	if ((token_list = GF_CALLOC (count, sizeof (char *),
                                        gf_common_mt_char)) == NULL)
	{
		GF_FREE (_running);
		return -1;
	}
  
	while ((token = strsep (&running, delim)) != NULL)
	{
		if (token[0] == '\0')
			continue;
      
                token_list[i] = gf_strdup (token);
		if (token_list[i] == NULL)
			goto free_exit;
                i++;
	}
  
	GF_FREE (_running);
  
	*tokens = token_list;
	*token_count = count;
	return 0;
  
free_exit:
	GF_FREE (_running);
	for (j = 0; j < i; j++)
	{
		GF_FREE (token_list[j]);
	}
	GF_FREE (token_list);
	return -1;
}

int 
gf_strstr (const char *str, const char *delim, const char *match)
{
        char *tmp      = NULL;
        char *save_ptr = NULL;
        char *tmp_str  = NULL;

        int  ret       = 0;

        tmp_str = strdup (str);

        if (str == NULL || delim == NULL || match == NULL || tmp_str == NULL) {
                ret = -1;
		goto out;
	}


        tmp = strtok_r (tmp_str, delim, &save_ptr);

        while (tmp) {
                ret = strcmp (tmp, match);

                if (ret == 0)
                        break;

                tmp = strtok_r (NULL, delim, &save_ptr);
        }

out:
        if (tmp_str)
                free (tmp_str);

        return ret;

}

int
gf_volume_name_validate (const char *volume_name)
{
	const char *vname = NULL;
  
	if (volume_name == NULL)
	{
		return -1;
	}
  
	if (!isalpha (volume_name[0]))
	{
		return 1;
	}
  
	for (vname = &volume_name[1]; *vname != '\0'; vname++)
	{
		if (!(isalnum (*vname) || *vname == '_'))
			return 1;
	}
  
	return 0;
}


int 
gf_string2time (const char *str, uint32_t *n)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, 0);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (!((tail[0] == '\0') || 
	      ((tail[0] == 's') && (tail[1] == '\0')) ||
	      ((tail[0] == 's') && (tail[1] == 'e') && (tail[2] == 'c') && (tail[3] == '\0'))))
	{
		return -1;
	}
  
	*n = value;
  
	return 0;
}


int 
gf_string2percent (const char *str, uint32_t *n)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, 0);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (!((tail[0] == '\0') || 
	      ((tail[0] == '%') && (tail[1] == '\0'))))
	{
		return -1;
	}
  
	*n = value;
  
	return 0;
}


static int 
_gf_string2long (const char *str, long *n, int base)
{
	long value = 0;
	char *tail = NULL;
	int old_errno = 0;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtol (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}

	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_gf_string2ulong (const char *str, unsigned long *n, int base)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoul (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_gf_string2uint (const char *str, unsigned int *n, int base)
{
	unsigned long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoul (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = (unsigned int)value;
  
	return 0;
}

static int 
_gf_string2double (const char *str, double *n)
{
	double value     = 0.0;
	char   *tail     = NULL;
	int    old_errno = 0;
  
	if (str == NULL || n == NULL) {
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtod (str, &tail);
  
	if (errno == ERANGE || errno == EINVAL)	{
		return -1;
	}
  
	if (errno == 0)	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0') {
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_gf_string2longlong (const char *str, long long *n, int base)
{
	long long value = 0;
	char *tail = NULL;
	int old_errno = 0;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoll (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

static int 
_gf_string2ulonglong (const char *str, unsigned long long *n, int base)
{
	unsigned long long value = 0;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoull (str, &tail, base);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		/* bala: invalid integer format */
		return -1;
	}
  
	*n = value;
  
	return 0;
}

int 
gf_string2long (const char *str, long *n)
{
	return _gf_string2long (str, n, 0);
}

int 
gf_string2ulong (const char *str, unsigned long *n)
{
	return _gf_string2ulong (str, n, 0);
}

int 
gf_string2int (const char *str, int *n)
{
	return _gf_string2long (str, (long *) n, 0);
}

int 
gf_string2uint (const char *str, unsigned int *n)
{
	return _gf_string2uint (str, n, 0);
}

int
gf_string2double (const char *str, double *n)
{
	return _gf_string2double (str, n);
}

int 
gf_string2longlong (const char *str, long long *n)
{
	return _gf_string2longlong (str, n, 0);
}

int 
gf_string2ulonglong (const char *str, unsigned long long *n)
{
	return _gf_string2ulonglong (str, n, 0);
}

int 
gf_string2int8 (const char *str, int8_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _gf_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT8_MIN && l <= INT8_MAX)
	{
		*n = (int8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2int16 (const char *str, int16_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _gf_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT16_MIN && l <= INT16_MAX)
	{
		*n = (int16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2int32 (const char *str, int32_t *n)
{
	long l = 0L;
	int rv = 0;
  
	rv = _gf_string2long (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT32_MIN && l <= INT32_MAX)
	{
		*n = (int32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2int64 (const char *str, int64_t *n)
{
	long long l = 0LL;
	int rv = 0;
  
	rv = _gf_string2longlong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= INT64_MIN && l <= INT64_MAX)
	{
		*n = (int64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint8 (const char *str, uint8_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT8_MAX)
	{
		*n = (uint8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint16 (const char *str, uint16_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT16_MAX)
	{
		*n = (uint16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint32 (const char *str, uint32_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT32_MAX)
	{
		*n = (uint32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint64 (const char *str, uint64_t *n)
{
	unsigned long long l = 0ULL;
	int rv = 0;
  
	rv = _gf_string2ulonglong (str, &l, 0);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT64_MAX)
	{
		*n = (uint64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2ulong_base10 (const char *str, unsigned long *n)
{
	return _gf_string2ulong (str, n, 10);
}

int 
gf_string2uint_base10 (const char *str, unsigned int *n)
{
	return _gf_string2uint (str,  n, 10);
}

int 
gf_string2uint8_base10 (const char *str, uint8_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT8_MAX)
	{
		*n = (uint8_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint16_base10 (const char *str, uint16_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT16_MAX)
	{
		*n = (uint16_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint32_base10 (const char *str, uint32_t *n)
{
	unsigned long l = 0L;
	int rv = 0;
  
	rv = _gf_string2ulong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT32_MAX)
	{
		*n = (uint32_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2uint64_base10 (const char *str, uint64_t *n)
{
	unsigned long long l = 0ULL;
	int rv = 0;
  
	rv = _gf_string2ulonglong (str, &l, 10);
	if (rv != 0)
		return rv;
  
	if (l >= 0 && l <= UINT64_MAX)
	{
		*n = (uint64_t) l;
		return 0;
	}
  
	errno = ERANGE;
	return -1;
}

int 
gf_string2bytesize (const char *str, uint64_t *n)
{
	uint64_t value = 0ULL;
	char *tail = NULL;
	int old_errno = 0;
	const char *s = NULL;
  
	if (str == NULL || n == NULL)
	{
		errno = EINVAL;
		return -1;
	}
  
	for (s = str; *s != '\0'; s++)
	{
		if (isspace (*s))
		{
			continue;
		}
		if (*s == '-')
		{
			/* bala: we do not support suffixed (-) sign and 
			   invalid integer format */
			return -1;
		}
		break;
	}
  
	old_errno = errno;
	errno = 0;
	value = strtoull (str, &tail, 10);
  
	if (errno == ERANGE || errno == EINVAL)
	{
		return -1;
	}
  
	if (errno == 0)
	{
		errno = old_errno;
	}
  
	if (tail[0] != '\0')
	{
		if (strcasecmp (tail, GF_UNIT_KB_STRING) == 0)
		{
			value *= GF_UNIT_KB;
		}
		else if (strcasecmp (tail, GF_UNIT_MB_STRING) == 0)
		{
			value *= GF_UNIT_MB;
		}
		else if (strcasecmp (tail, GF_UNIT_GB_STRING) == 0)
		{
			value *= GF_UNIT_GB;
		}
		else if (strcasecmp (tail, GF_UNIT_TB_STRING) == 0)
		{
			value *= GF_UNIT_TB;
		}
		else if (strcasecmp (tail, GF_UNIT_PB_STRING) == 0)
		{
			value *= GF_UNIT_PB;
		}
		else 
		{
			/* bala: invalid integer format */
			return -1;
		}
	}
  
	*n = value;
  
	return 0;
}

int64_t 
gf_str_to_long_long (const char *number)
{
	int64_t unit = 1;
	int64_t ret = 0;
	char *endptr = NULL ;
	if (!number)
		return 0;

	ret = strtoll (number, &endptr, 0);

	if (endptr) {
		switch (*endptr) {
		case 'G':
		case 'g':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024 * 1024 * 1024;
			break;
		case 'M':
		case 'm':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024 * 1024;
			break;
		case 'K':
		case 'k':
			if ((* (endptr + 1) == 'B') ||(* (endptr + 1) == 'b'))
				unit = 1024;
			break;
		case '%':
			unit = 1;
			break;
		default:
			unit = 1;
			break;
		}
	}
	return ret * unit;
}

int 
gf_string2boolean (const char *str, gf_boolean_t *b)
{
	if (str == NULL) {
		return -1;
	}
	
	if ((strcasecmp (str, "1") == 0) || 
	    (strcasecmp (str, "on") == 0) || 
	    (strcasecmp (str, "yes") == 0) || 
	    (strcasecmp (str, "true") == 0) || 
	    (strcasecmp (str, "enable") == 0)) {
		*b = _gf_true;
		return 0;
	}
	
	if ((strcasecmp (str, "0") == 0) || 
	    (strcasecmp (str, "off") == 0) || 
	    (strcasecmp (str, "no") == 0) || 
	    (strcasecmp (str, "false") == 0) || 
	    (strcasecmp (str, "disable") == 0)) {
		*b = _gf_false;
		return 0;
	}
	
	return -1;
}


int 
gf_lockfd (int fd)
{
	struct flock fl;
	
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	
	return fcntl (fd, F_SETLK, &fl);
}


int 
gf_unlockfd (int fd)
{
	struct flock fl;
	
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	
	return fcntl (fd, F_SETLK, &fl);
}
  
static void
compute_checksum (char *buf, size_t size, uint32_t *checksum)
{
        int  ret = -1;
        char *checksum_buf = NULL;

        checksum_buf = (char *)(checksum);

        if (!(*checksum)) {
                checksum_buf [0] = 0xba;
                checksum_buf [1] = 0xbe;
                checksum_buf [2] = 0xb0;
                checksum_buf [3] = 0x0b;                
        }

        for (ret = 0; ret < (size - 4); ret += 4) {
                checksum_buf[0] ^= (buf[ret]);
                checksum_buf[1] ^= (buf[ret + 1] << 1) ;
                checksum_buf[2] ^= (buf[ret + 2] << 2);
                checksum_buf[3] ^= (buf[ret + 3] << 3);
        }

        for (ret = 0; ret <= (size % 4); ret++) {
                checksum_buf[ret] ^= (buf[(size - 4) + ret] << ret);
        }
        
        return;
}

#define GF_CHECKSUM_BUF_SIZE 1024

int
get_checksum_for_file (int fd, uint32_t *checksum) 
{
        int ret = -1;
        char buf[GF_CHECKSUM_BUF_SIZE] = {0,};

        /* goto first place */
        lseek (fd, 0L, SEEK_SET);
        do {
                ret = read (fd, &buf, GF_CHECKSUM_BUF_SIZE);
                if (ret > 0)
                        compute_checksum (buf, GF_CHECKSUM_BUF_SIZE, 
                                          checksum);
        } while (ret > 0);

        /* set it back */
        lseek (fd, 0L, SEEK_SET);

        return ret;
}
