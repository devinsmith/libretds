/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <assert.h>
#include <ctype.h>

#include <stdlib.h>

#if HAVE_LIMITS_H
#include <limits.h>
#endif 

#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#ifdef _WIN32
#include <process.h>
#endif

#include <freetds/tds.h>
#include "tds_configs.h"
#include <freetds/string.h>
#include "replacements.h"



static void tds_config_login(TDSLOGIN * connection, TDSLOGIN * login);
static int tds_config_env_tdsdump(TDSLOGIN * login);
static int parse_server_name_for_port(TDSLOGIN * connection, TDSLOGIN * login);

#if !defined(_WIN32)
       const char STD_DATETIME_FMT[] = "%b %e %Y %I:%M%p";
static const char pid_config_logpath[] = "/tmp/tdsconfig.log.%d";
static const char freetds_conf[] = "%s/etc/freetds.conf";
static const char location[] = "(from $FREETDS/etc)";
static const char pid_logpath[] = "/tmp/freetds.log.%d";
static const char interfaces_path[] = "/etc/freetds";
#else
       const char STD_DATETIME_FMT[] = "%b %d %Y %I:%M%p"; /* msvcr80.dll does not support %e */
static const char pid_config_logpath[] = "c:\\tdsconfig.log.%d";
static const char freetds_conf [] = "%s\\freetds.conf";
static const char location[] = "(from $FREETDS)";
static const char pid_logpath[] = "c:\\freetds.log.%d";
static const char interfaces_path[] = "c:\\";
#endif

/**
 * \ingroup libtds
 * \defgroup config Configuration
 * Handle reading of configuration
 */

/**
 * \addtogroup config
 * @{ 
 */

/**
 * tds_read_config_info() will fill the tds connection structure based on configuration 
 * information gathered in the following order:
 * 1) Program specified in TDSLOGIN structure
 * 2) The environment variables TDSVER, TDSDUMP, TDSPORT, TDSQUERY, TDSHOST
 * 3) A config file with the following search order:
 *    a) a readable file specified by environment variable FREETDSCONF
 *    b) a readable file in ~/.freetds.conf
 *    c) a readable file in $prefix/etc/freetds.conf
 * 3) ~/.interfaces if exists
 * 4) $SYBASE/interfaces if exists
 * 5) TDS_DEF_* default values
 *
 * .tdsrc and freetds.conf have been added to make the package easier to 
 * integration with various Linux and *BSD distributions.
 */
TDSLOGIN *
tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale)
{
	TDSLOGIN *connection;
	char *s;
	char *path;
	pid_t pid;
	int opened = 0, found;
	struct tds_addrinfo *addrs;

	/* allocate a new structure with hard coded and build-time defaults */
	connection = tds_alloc_login(0);
	if (!connection || !tds_init_login(connection, locale)) {
		tds_free_login(connection);
		return NULL;
	}

	s = getenv("TDSDUMPCONFIG");
	if (s) {
		if (*s) {
			opened = tdsdump_open(s);
		} else {
			pid = getpid();
			if (asprintf(&path, pid_config_logpath, (int)pid) >= 0) {
				if (*path) {
					opened = tdsdump_open(path);
				}
				free(path);
			}
		}
	}

	tdsdump_log(TDS_DBG_INFO1, "Getting connection information for [%s].\n", 
			    tds_dstr_cstr(&login->server_name));	/* (The server name is set in login.c.) */

	/* Read the config files. */
	tdsdump_log(TDS_DBG_INFO1, "Attempting to read conf files.\n");
	found = 0;
	if (!found) {
		if (parse_server_name_for_port(connection, login)) {

			found = 0;
			/* do it again to really override what found in freetds.conf */
			if (TDS_SUCCEED(tds_lookup_host_set(tds_dstr_cstr(&connection->server_name), &connection->ip_addrs))) {
				tds_dstr_dup(&connection->server_host_name, &connection->server_name);
				found = 1;
			}
		}
	}
	if (!found) {
		if (connection->ip_addrs == NULL)
			tdserror(tds_get_ctx(tds), tds, TDSEINTF, 0);
	}

	/* Override config file settings with environment variables. */
	tds_fix_login(connection);

	/* And finally apply anything from the login structure */
	tds_config_login(connection, login);

	if (opened) {
		char tmp[128];

		tdsdump_log(TDS_DBG_INFO1, "Final connection parameters:\n");
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_name", tds_dstr_cstr(&connection->server_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_host_name", tds_dstr_cstr(&connection->server_host_name));

		for (addrs = connection->ip_addrs; addrs != NULL; addrs = addrs->ai_next)
			tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "ip_addr", tds_addrinfo2str(addrs, tmp, sizeof(tmp)));

		if (connection->ip_addrs == NULL)
			tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "ip_addr", "");

		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "instance_name", tds_dstr_cstr(&connection->instance_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "port", connection->port);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "major_version", TDS_MAJOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "minor_version", TDS_MINOR(connection));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "block_size", connection->block_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "language", tds_dstr_cstr(&connection->language));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_charset", tds_dstr_cstr(&connection->server_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "connect_timeout", connection->connect_timeout);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_host_name", tds_dstr_cstr(&connection->client_host_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "client_charset", tds_dstr_cstr(&connection->client_charset));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "use_utf16", connection->use_utf16);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "app_name", tds_dstr_cstr(&connection->app_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "user_name", tds_dstr_cstr(&connection->user_name));
		/* tdsdump_log(TDS_DBG_PASSWD, "\t%20s = %s\n", "password", tds_dstr_cstr(&connection->password)); 
			(no such flag yet) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "library", tds_dstr_cstr(&connection->library));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "bulk_copy", (int)connection->bulk_copy);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "suppress_language", (int)connection->suppress_language);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "encrypt level", (int)connection->encryption_level);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "query_timeout", connection->query_timeout);
		/* tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "capabilities", tds_dstr_cstr(&connection->capabilities)); 
			(not null terminated) */
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "database", tds_dstr_cstr(&connection->database));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "dump_file", tds_dstr_cstr(&connection->dump_file));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %x\n", "debug_flags", connection->debug_flags);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "text_size", connection->text_size);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %d\n", "emul_little_endian", connection->emul_little_endian);
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_realm_name", tds_dstr_cstr(&connection->server_realm_name));
		tdsdump_log(TDS_DBG_INFO1, "\t%20s = %s\n", "server_spn", tds_dstr_cstr(&connection->server_spn));

		tdsdump_close();
	}

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connection->dump_file) && !tdsdump_isopen()) {
		if (connection->debug_flags)
			tds_debug_flags = connection->debug_flags;
		tdsdump_open(tds_dstr_cstr(&connection->dump_file));
	}

	return connection;
}

/**
 * Fix configuration after reading it. 
 * Currently this read some environment variables and replace some options.
 */
void
tds_fix_login(TDSLOGIN * login)
{
	/* Now check the environment variables */
	tds_config_env_tdsdump(login);
}

/**
 * Read a section of configuration file (INI style file)
 * @param in             configuration file
 * @param section        section to read
 * @param tds_conf_parse callback that receive every entry in section
 * @param param          parameter to pass to callback function
 */
int
tds_read_conf_section(FILE * in, const char *section, TDSCONFPARSE tds_conf_parse, void *param)
{
	char line[256], *value;
#define option line
	char *s;
	char p;
	int i;
	int insection = 0;
	int found = 0;

	tdsdump_log(TDS_DBG_INFO1, "Looking for section %s.\n", section);
	while (fgets(line, sizeof(line), in)) {
		s = line;

		/* skip leading whitespace */
		while (*s && isspace(*s))
			s++;

		/* skip it if it's a comment line */
		if (*s == ';' || *s == '#')
			continue;

		/* read up to the = ignoring duplicate spaces */
		p = 0;
		i = 0;
		while (*s && *s != '=') {
			if (!isspace(*s)) {
				if (isspace(p))
					option[i++] = ' ';
				option[i++] = tolower((unsigned char) *s);
			}
			p = *s;
			s++;
		}

		/* skip if empty option */
		if (!i)
			continue;

		/* skip the = */
		if (*s)
			s++;

		/* terminate the option, must be done after skipping = */
		option[i] = '\0';

		/* skip leading whitespace */
		while (*s && isspace(*s))
			s++;

		/* read up to a # ; or null ignoring duplicate spaces */
		value = s;
		p = 0;
		i = 0;
		while (*s && *s != ';' && *s != '#') {
			if (!isspace(*s)) {
				if (isspace(p))
					value[i++] = ' ';
				value[i++] = *s;
			}
			p = *s;
			s++;
		}
		value[i] = '\0';

		if (option[0] == '[') {
			s = strchr(option, ']');
			if (s)
				*s = '\0';
			tdsdump_log(TDS_DBG_INFO1, "\tFound section %s.\n", &option[1]);

			if (!strcasecmp(section, &option[1])) {
				tdsdump_log(TDS_DBG_INFO1, "Got a match.\n");
				insection = 1;
				found = 1;
			} else {
				insection = 0;
			}
		} else if (insection) {
			tds_conf_parse(option, value, param);
		}

	}
	tdsdump_log(TDS_DBG_INFO1, "\tReached EOF\n");
	return found;
#undef option
}

static void
tds_config_login(TDSLOGIN * connection, TDSLOGIN * login)
{
	DSTR *res = &login->server_name;

	if (!tds_dstr_isempty(&login->server_name)) {
		if (1 || tds_dstr_isempty(&connection->server_name)) 
			res = tds_dstr_dup(&connection->server_name, &login->server_name);
	}
	if (login->tds_version)
		connection->tds_version = login->tds_version;
	if (res && !tds_dstr_isempty(&login->language)) {
		res = tds_dstr_dup(&connection->language, &login->language);
	}
	if (res && !tds_dstr_isempty(&login->server_charset)) {
		res = tds_dstr_dup(&connection->server_charset, &login->server_charset);
	}
	if (res && !tds_dstr_isempty(&login->client_charset)) {
		res = tds_dstr_dup(&connection->client_charset, &login->client_charset);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "client_charset",
			    tds_dstr_cstr(&connection->client_charset));
	}
	if (res && !tds_dstr_isempty(&login->database)) {
		res = tds_dstr_dup(&connection->database, &login->database);
		tdsdump_log(TDS_DBG_INFO1, "tds_config_login: %s is %s.\n", "database_name",
			    tds_dstr_cstr(&connection->database));
	}
	if (res && !tds_dstr_isempty(&login->client_host_name)) {
		res = tds_dstr_dup(&connection->client_host_name, &login->client_host_name);
	}
	if (res && !tds_dstr_isempty(&login->app_name)) {
		res = tds_dstr_dup(&connection->app_name, &login->app_name);
	}
	if (res && !tds_dstr_isempty(&login->user_name)) {
		res = tds_dstr_dup(&connection->user_name, &login->user_name);
	}
	if (res && !tds_dstr_isempty(&login->password)) {
		/* for security reason clear memory */
		tds_dstr_zero(&connection->password);
		res = tds_dstr_dup(&connection->password, &login->password);
	}
	if (res && !tds_dstr_isempty(&login->library)) {
		res = tds_dstr_dup(&connection->library, &login->library);
	}
	if (login->encryption_level) {
		connection->encryption_level = login->encryption_level;
	}
	if (login->suppress_language) {
		connection->suppress_language = 1;
	}
	if (login->bulk_copy) {
		connection->bulk_copy = 1;
	}
	if (login->block_size) {
		connection->block_size = login->block_size;
	}
	if (login->port)
		connection->port = login->port;
	if (login->connect_timeout)
		connection->connect_timeout = login->connect_timeout;

	if (login->query_timeout)
		connection->query_timeout = login->query_timeout;

	/* copy other info not present in configuration file */
	connection->capabilities = login->capabilities;
}

static int
tds_config_env_tdsdump(TDSLOGIN * login)
{
	char *s;
	char *path;
	pid_t pid = 0;

	if ((s = getenv("TDSDUMP"))) {
		if (!strlen(s)) {
			pid = getpid();
			if (asprintf(&path, pid_logpath, (int)pid) < 0)
				return 0;
			if (!tds_dstr_set(&login->dump_file, path)) {
				free(path);
				return 0;
			}
		} else {
			if (!tds_dstr_copy(&login->dump_file, s))
				return 0;
		}
		tdsdump_log(TDS_DBG_INFO1, "Setting 'dump_file' to '%s' from $TDSDUMP.\n", tds_dstr_cstr(&login->dump_file));
	}
	return 1;
}


/**
 * Get the IP address for a hostname. Store server's IP address 
 * in the string 'ip' in dotted-decimal notation.  (The "hostname" might itself
 * be a dotted-decimal address.  
 *
 * If we can't determine the IP address then 'ip' will be set to empty
 * string.
 */
/* TODO callers seem to set always connection info... change it */
struct tds_addrinfo *
tds_lookup_host(const char *servername)	/* (I) name of the server                  */
{
	struct tds_addrinfo hints, *addr = NULL;
	assert(servername != NULL);

	memset(&hints, '\0', sizeof(hints));
	hints.ai_family = AF_UNSPEC;

#ifdef AI_ADDRCONFIG
	hints.ai_flags |= AI_ADDRCONFIG;
#endif

#ifdef AI_NUMERICSERV
	hints.ai_flags |= AI_NUMERICSERV;
#endif

	if (tds_getaddrinfo(servername, "1433", &hints, &addr))
		return NULL;
	return addr;
}

TDSRET
tds_lookup_host_set(const char *servername, struct tds_addrinfo **addr)
{
	struct tds_addrinfo *newaddr;
	assert(servername != NULL && addr != NULL);

	if ((newaddr = tds_lookup_host(servername)) != NULL) {
		if (*addr != NULL)
			tds_freeaddrinfo(*addr);
		*addr = newaddr;
		return TDS_SUCCESS;
	}
	return TDS_FAIL;
}

/**
 * Check the server name to find port info first
 * Warning: connection-> & login-> are all modified when needed
 * \return 1 when found, else 0
 */
static int
parse_server_name_for_port(TDSLOGIN * connection, TDSLOGIN * login)
{
	const char *pSep;
	const char *server;

	/* seek the ':' in login server_name */
	server = tds_dstr_cstr(&login->server_name);

	/* IPv6 address can be quoted */
	if (server[0] == '[') {
		pSep = strstr(server, "]:");
		if (pSep)
			++pSep;
	} else {
		pSep = strrchr(server, ':');
	}

	if (pSep && pSep != server) {	/* yes, i found it! */
		/* modify connection-> && login->server_name & ->port */
		login->port = connection->port = atoi(pSep + 1);
		tds_dstr_empty(&connection->instance_name);
	} else {
		/* handle instance name */
		pSep = strrchr(server, '\\');
		if (!pSep || pSep == server)
			return 0;

		if (!tds_dstr_copy(&connection->instance_name, pSep + 1))
			return 0;
		connection->port = 0;
	}

	if (!tds_dstr_copyn(&connection->server_name, server, pSep - server))
		return 0;

	return 1;
}

/** @} */
