/*
  Copyright (c) 2010 Gluster, Inc. <http://www.gluster.com>
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

#include "rpcsvc.h"
#include "rpc-transport.h"
#include "dict.h"
#include "logging.h"
#include "byte-order.h"
#include "common-utils.h"
#include "compat-errno.h"
#include "list.h"
#include "xdr-rpc.h"
#include "iobuf.h"
#include "globals.h"
#include "xdr-common.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <arpa/inet.h>
#include <rpc/xdr.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>

struct rpcsvc_program gluster_dump_prog;


#define rpcsvc_alloc_request(con, request)                              \
        do {                                                            \
                request = (rpcsvc_request_t *) mem_get ((con)->rxpool); \
                memset (request, 0, sizeof (rpcsvc_request_t));         \
        } while (0)


int
rpcsvc_conn_peer_check_search (dict_t *options, char *pattern, char *clstr)
{
        int                     ret = -1;
        char                    *addrtok = NULL;
        char                    *addrstr = NULL;
        char                    *svptr = NULL;

        if ((!options) || (!clstr))
                return -1;

        if (!dict_get (options, pattern))
                return -1;

        ret = dict_get_str (options, pattern, &addrstr);
        if (ret < 0) {
                ret = -1;
                goto err;
        }

        if (!addrstr) {
                ret = -1;
                goto err;
        }

        addrtok = strtok_r (addrstr, ",", &svptr);
        while (addrtok) {

#ifdef FNM_CASEFOLD
                ret = fnmatch (addrtok, clstr, FNM_CASEFOLD);
#else
                ret = fnmatch (addrtok, clstr, 0);
#endif
                if (ret == 0)
                        goto err;

                addrtok = strtok_r (NULL, ",", &svptr);
        }

        ret = -1;
err:

        return ret;
}


int
rpcsvc_conn_peer_check_allow (dict_t *options, char *volname, char *clstr)
{
        int     ret = RPCSVC_AUTH_DONTCARE;
        char    *srchstr = NULL;
        char    globalrule[] = "rpc-auth.addr.allow";

        if ((!options) || (!clstr))
                return ret;

        /* If volname is NULL, then we're searching for the general rule to
         * determine the current address in clstr is allowed or not for all
         * subvolumes.
         */
        if (volname) {
                ret = gf_asprintf (&srchstr, "rpc-auth.addr.%s.allow", volname);
                if (ret == -1) {
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "asprintf failed");
                        ret = RPCSVC_AUTH_DONTCARE;
                        goto out;
                }
        } else
                srchstr = globalrule;

        ret = rpcsvc_conn_peer_check_search (options, srchstr, clstr);
        if (volname)
                GF_FREE (srchstr);

        if (ret == 0)
                ret = RPCSVC_AUTH_ACCEPT;
        else
                ret = RPCSVC_AUTH_DONTCARE;
out:
        return ret;
}

int
rpcsvc_conn_peer_check_reject (dict_t *options, char *volname, char *clstr)
{
        int     ret = RPCSVC_AUTH_DONTCARE;
        char    *srchstr = NULL;
        char    generalrule[] = "rpc-auth.addr.reject";

        if ((!options) || (!clstr))
                return ret;

        if (volname) {
                ret = gf_asprintf (&srchstr, "rpc-auth.addr.%s.reject", volname);
                if (ret == -1) {
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "asprintf failed");
                        ret = RPCSVC_AUTH_REJECT;
                        goto out;
                }
        } else
                srchstr = generalrule;

        ret = rpcsvc_conn_peer_check_search (options, srchstr, clstr);
        if (volname)
                GF_FREE (srchstr);

        if (ret == 0)
                ret = RPCSVC_AUTH_REJECT;
        else
                ret = RPCSVC_AUTH_DONTCARE;
out:
        return ret;
}


/* This function tests the results of the allow rule and the reject rule to
 * combine them into a single result that can be used to determine if the
 * connection should be allowed to proceed.
 * Heres the test matrix we need to follow in this function.
 *
 * A -  Allow, the result of the allow test. Never returns R.
 * R - Reject, result of the reject test. Never returns A.
 * Both can return D or dont care if no rule was given.
 *
 * | @allow | @reject | Result |
 * |    A   |   R     | R      |
 * |    D   |   D     | D      |
 * |    A   |   D     | A      |
 * |    D   |   R     | R      |
 */
int
rpcsvc_combine_allow_reject_volume_check (int allow, int reject)
{
        int     final = RPCSVC_AUTH_REJECT;

        /* If allowed rule allows but reject rule rejects, we stay cautious
         * and reject. */
        if ((allow == RPCSVC_AUTH_ACCEPT) && (reject == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;
        /* if both are dont care, that is user did not specify for either allow
         * or reject, we leave it up to the general rule to apply, in the hope
         * that there is one.
         */
        else if ((allow == RPCSVC_AUTH_DONTCARE) &&
                 (reject == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_DONTCARE;
        /* If one is dont care, the other one applies. */
        else if ((allow == RPCSVC_AUTH_ACCEPT) &&
                 (reject == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((allow == RPCSVC_AUTH_DONTCARE) &&
                 (reject == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;

        return final;
}


/* Combines the result of the general rule test against, the specific rule
 * to determine final permission for the client's address.
 *
 * | @gen   | @spec   | Result |
 * |    A   |   A     | A      |
 * |    A   |   R     | R      |
 * |    A   |   D     | A      |
 * |    D   |   A     | A      |
 * |    D   |   R     | R      |
 * |    D   |   D     | D      |
 * |    R   |   A     | A      |
 * |    R   |   D     | R      |
 * |    R   |   R     | R      |
 */
int
rpcsvc_combine_gen_spec_addr_checks (int gen, int spec)
{
        int     final = RPCSVC_AUTH_REJECT;

        if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec== RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_DONTCARE;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;

        return final;
}



/* Combines the result of the general rule test against, the specific rule
 * to determine final test for the connection coming in for a given volume.
 *
 * | @gen   | @spec   | Result |
 * |    A   |   A     | A      |
 * |    A   |   R     | R      |
 * |    A   |   D     | A      |
 * |    D   |   A     | A      |
 * |    D   |   R     | R      |
 * |    D   |   D     | R      |, special case, we intentionally disallow this.
 * |    R   |   A     | A      |
 * |    R   |   D     | R      |
 * |    R   |   R     | R      |
 */
int
rpcsvc_combine_gen_spec_volume_checks (int gen, int spec)
{
        int     final = RPCSVC_AUTH_REJECT;

        if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_ACCEPT) && (spec == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;
        /* On no rule, we reject. */
        else if ((gen == RPCSVC_AUTH_DONTCARE) && (spec== RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_ACCEPT))
                final = RPCSVC_AUTH_ACCEPT;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_DONTCARE))
                final = RPCSVC_AUTH_REJECT;
        else if ((gen == RPCSVC_AUTH_REJECT) && (spec == RPCSVC_AUTH_REJECT))
                final = RPCSVC_AUTH_REJECT;

        return final;
}


int
rpcsvc_conn_peer_check_name (dict_t *options, char *volname,
                             rpcsvc_conn_t *conn)
{
        int     ret = RPCSVC_AUTH_REJECT;
        int     aret = RPCSVC_AUTH_REJECT;
        int     rjret = RPCSVC_AUTH_REJECT;
        char    clstr[RPCSVC_PEER_STRLEN];

        if (!conn)
                return ret;

        ret = rpcsvc_conn_peername (conn, clstr, RPCSVC_PEER_STRLEN);
        if (ret != 0) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to get remote addr: "
                        "%s", gai_strerror (ret));
                ret = RPCSVC_AUTH_REJECT;
                goto err;
        }

        aret = rpcsvc_conn_peer_check_allow (options, volname, clstr);
        rjret = rpcsvc_conn_peer_check_reject (options, volname, clstr);

        ret = rpcsvc_combine_allow_reject_volume_check (aret, rjret);

err:
        return ret;
}


int
rpcsvc_conn_peer_check_addr (dict_t *options, char *volname,rpcsvc_conn_t *conn)
{
        int     ret = RPCSVC_AUTH_REJECT;
        int     aret = RPCSVC_AUTH_DONTCARE;
        int     rjret = RPCSVC_AUTH_REJECT;
        char    clstr[RPCSVC_PEER_STRLEN];

        if (!conn)
                return ret;

        ret = rpcsvc_conn_peeraddr (conn, clstr, RPCSVC_PEER_STRLEN, NULL, 0);
        if (ret != 0) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to get remote addr: "
                        "%s", gai_strerror (ret));
                ret = RPCSVC_AUTH_REJECT;
                goto err;
        }

        aret = rpcsvc_conn_peer_check_allow (options, volname, clstr);
        rjret = rpcsvc_conn_peer_check_reject (options, volname, clstr);

        ret = rpcsvc_combine_allow_reject_volume_check (aret, rjret);
err:
        return ret;
}


int
rpcsvc_conn_check_volume_specific (dict_t *options, char *volname,
                                   rpcsvc_conn_t *conn)
{
        int             namechk = RPCSVC_AUTH_REJECT;
        int             addrchk = RPCSVC_AUTH_REJECT;
        gf_boolean_t    namelookup = _gf_true;
        char            *namestr = NULL;
        int             ret = 0;

        if ((!options) || (!volname) || (!conn))
                return RPCSVC_AUTH_REJECT;

        /* Enabled by default */
        if ((dict_get (options, "rpc-auth.addr.namelookup"))) {
                ret = dict_get_str (options, "rpc-auth.addr.namelookup"
                                    , &namestr);
                if (ret == 0) {
                        ret = gf_string2boolean (namestr, &namelookup);
                        if (ret)
                                gf_log ("rpcsvc", GF_LOG_DEBUG,
                                        "wrong option %s given for "
                                        "'namelookup'", namestr);
                }
        }

        /* We need two separate checks because the rules with addresses in them
         * can be network addresses which can be general and names can be
         * specific which will over-ride the network address rules.
         */
        if (namelookup)
                namechk = rpcsvc_conn_peer_check_name (options, volname, conn);
        addrchk = rpcsvc_conn_peer_check_addr (options, volname, conn);

        if (namelookup)
                ret = rpcsvc_combine_gen_spec_addr_checks (addrchk, namechk);
        else
                ret = addrchk;

        return ret;
}


int
rpcsvc_conn_check_volume_general (dict_t *options, rpcsvc_conn_t *conn)
{
        int             addrchk = RPCSVC_AUTH_REJECT;
        int             namechk = RPCSVC_AUTH_REJECT;
        gf_boolean_t    namelookup = _gf_true;
        char            *namestr = NULL;
        int             ret = 0;

        if ((!options) || (!conn))
                return RPCSVC_AUTH_REJECT;

        /* Enabled by default */
        if ((dict_get (options, "rpc-auth.addr.namelookup"))) {
                ret = dict_get_str (options, "rpc-auth.addr.namelookup"
                                    , &namestr);
                if (!ret) {
                        ret = gf_string2boolean (namestr, &namelookup);
                        if (ret)
                                gf_log ("rpcsvc", GF_LOG_DEBUG,
                                        "wrong option %s given for "
                                        "'namelookup'", namestr);
                }
        }

        /* We need two separate checks because the rules with addresses in them
         * can be network addresses which can be general and names can be
         * specific which will over-ride the network address rules.
         */
        if (namelookup)
                namechk = rpcsvc_conn_peer_check_name (options, NULL, conn);
        addrchk = rpcsvc_conn_peer_check_addr (options, NULL, conn);

        if (namelookup)
                ret = rpcsvc_combine_gen_spec_addr_checks (addrchk, namechk);
        else
                ret = addrchk;

        return ret;
}

int
rpcsvc_conn_peer_check (dict_t *options, char *volname, rpcsvc_conn_t *conn)
{
        int     general_chk = RPCSVC_AUTH_REJECT;
        int     specific_chk = RPCSVC_AUTH_REJECT;

        if ((!options) || (!volname) || (!conn))
                return RPCSVC_AUTH_REJECT;

        general_chk = rpcsvc_conn_check_volume_general (options, conn);
        specific_chk = rpcsvc_conn_check_volume_specific (options, volname,
                                                          conn);

        return rpcsvc_combine_gen_spec_volume_checks (general_chk,specific_chk);
}


char *
rpcsvc_volume_allowed (dict_t *options, char *volname)
{
        char    globalrule[] = "rpc-auth.addr.allow";
        char    *srchstr = NULL;
        char    *addrstr = NULL;
        int     ret = -1;

        if ((!options) || (!volname))
                return NULL;

        ret = gf_asprintf (&srchstr, "rpc-auth.addr.%s.allow", volname);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "asprintf failed");
                goto out;
        }

        if (!dict_get (options, srchstr)) {
                GF_FREE (srchstr);
                srchstr = globalrule;
                ret = dict_get_str (options, srchstr, &addrstr);
                if (ret)
                        gf_log ("rpcsvc", GF_LOG_DEBUG,
                                "failed to get the string %s", srchstr);
        } else {
                ret = dict_get_str (options, srchstr, &addrstr);
                if (ret)
                        gf_log ("rpcsvc", GF_LOG_DEBUG,
                                "failed to get the string %s", srchstr);
        }
out:
        return addrstr;
}



/* Initialize the core of a connection */
rpcsvc_conn_t *
rpcsvc_conn_alloc (rpcsvc_t *svc, rpc_transport_t *trans)
{
        rpcsvc_conn_t  *conn = NULL;
        int             ret = -1;
        unsigned int    poolcount = 0;

        conn = GF_CALLOC (1, sizeof(*conn), gf_common_mt_rpcsvc_conn_t);
        if (!conn) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "memory allocation failed");
                return NULL;
        }

        conn->trans = trans;
        conn->svc   = svc;
        poolcount   = RPCSVC_POOLCOUNT_MULT * svc->memfactor;

        gf_log (GF_RPCSVC, GF_LOG_TRACE, "rx pool: %d", poolcount);
        conn->rxpool = mem_pool_new (rpcsvc_request_t, poolcount);
        /* TODO: leak */
        if (!conn->rxpool) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "mem pool allocation failed");
                goto free_conn;
        }

        /* Cannot consider a connection connected unless the user of this
         * connection decides it is ready to use. It is possible that we have
         * to free this connection soon after. That free will not happpen
         * unless the state is disconnected.
         */
        conn->connstate = RPCSVC_CONNSTATE_DISCONNECTED;
        pthread_mutex_init (&conn->connlock, NULL);
        conn->connref = 0;

        ret = 0;

free_conn:
        if (ret == -1) {
                GF_FREE (conn);
                conn = NULL;
        }

        return conn;
}

int
rpcsvc_notify (rpc_transport_t *trans, void *mydata,
               rpc_transport_event_t event, void *data, ...);

void
rpcsvc_conn_state_init (rpcsvc_conn_t *conn)
{
        if (!conn)
                return;

        ++conn->connref;
        conn->connstate = RPCSVC_CONNSTATE_CONNECTED;
}


rpcsvc_notify_wrapper_t *
rpcsvc_notify_wrapper_alloc (void)
{
        rpcsvc_notify_wrapper_t *wrapper = NULL;

        wrapper = GF_CALLOC (1, sizeof (*wrapper), gf_common_mt_rpcsvc_wrapper_t);
        if (!wrapper) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "memory allocation failed");
                goto out;
        }

        INIT_LIST_HEAD (&wrapper->list);
out:
        return wrapper;
}


void
rpcsvc_listener_destroy (rpcsvc_listener_t *listener)
{
        rpcsvc_t *svc = NULL;

        if (!listener) {
                goto out;
        }

        if (!listener->conn) {
                goto listener_free;
        }

        svc = listener->conn->svc;
        if (!svc) {
                goto listener_free;
        }

        pthread_mutex_lock (&svc->rpclock);
        {
                list_del_init (&listener->list);
        }
        pthread_mutex_unlock (&svc->rpclock);

listener_free:
        GF_FREE (listener);
out:
        return;
}


void
rpcsvc_conn_destroy (rpcsvc_conn_t *conn)
{
        rpcsvc_listener_t       *listener = NULL;

        if (!conn || !conn->rxpool || !conn->listener)
                goto out;

        if (conn->trans)
                rpc_transport_destroy (conn->trans);

        mem_pool_destroy (conn->rxpool);

        listener = conn->listener;
        if (listener->conn == conn) {
                rpcsvc_listener_destroy (listener);
        }

        /* Need to destory record state, txlists etc. */
        GF_FREE (conn);
out:
        gf_log (GF_RPCSVC, GF_LOG_DEBUG, "Connection destroyed");
}


rpcsvc_conn_t *
rpcsvc_conn_init (rpcsvc_t *svc, rpc_transport_t *trans)
{
        int            ret  = -1;
        rpcsvc_conn_t *conn = NULL;

        conn = rpcsvc_conn_alloc (svc, trans);
        if (!conn) {
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "cannot init a connection");
                goto out;
        }

        ret = rpc_transport_register_notify (trans, rpcsvc_notify, conn);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "registering notify failed");
                rpcsvc_conn_destroy (conn);
                conn = NULL;
                goto out;
        }

        rpcsvc_conn_state_init (conn);

out:
        return conn;
}


int
__rpcsvc_conn_unref (rpcsvc_conn_t *conn)
{
        --conn->connref;
        return conn->connref;
}


void
__rpcsvc_conn_deinit (rpcsvc_conn_t *conn)
{
        if (!conn)
                return;

        if (rpcsvc_conn_check_active (conn)) {
                conn->connstate = RPCSVC_CONNSTATE_DISCONNECTED;
        }

        if (conn->trans) {
                rpc_transport_disconnect (conn->trans);
                conn->trans = NULL;
        }
}


void
rpcsvc_conn_deinit (rpcsvc_conn_t *conn)
{
        int ref = 0;

        if (!conn)
                return;

        pthread_mutex_lock (&conn->connlock);
        {
                __rpcsvc_conn_deinit (conn);
                ref = __rpcsvc_conn_unref (conn);
        }
        pthread_mutex_unlock (&conn->connlock);

        if (ref == 0)
                rpcsvc_conn_destroy (conn);

        return;
}


void
rpcsvc_conn_unref (rpcsvc_conn_t *conn)
{
        int ref = 0;
        if (!conn)
                return;

        pthread_mutex_lock (&conn->connlock);
        {
                ref = __rpcsvc_conn_unref (conn);
        }
        pthread_mutex_unlock (&conn->connlock);

        if (ref == 0) {
                rpcsvc_conn_destroy (conn);
        }
}


int
rpcsvc_conn_active (rpcsvc_conn_t *conn)
{
        int     status = 0;

        if (!conn)
                return 0;

        pthread_mutex_lock (&conn->connlock);
        {
                status = rpcsvc_conn_check_active (conn);
        }
        pthread_mutex_unlock (&conn->connlock);

        return status;
}


void
rpcsvc_conn_ref (rpcsvc_conn_t *conn)
{
        if (!conn)
                return;

        pthread_mutex_lock (&conn->connlock);
        {
                ++conn->connref;
        }
        pthread_mutex_unlock (&conn->connlock);

        return;
}


int
rpcsvc_conn_privport_check (rpcsvc_t *svc, char *volname, rpcsvc_conn_t *conn)
{
        struct sockaddr_in      sa;
        int                     ret = RPCSVC_AUTH_REJECT;
        socklen_t               sasize = sizeof (sa);
        char                    *srchstr = NULL;
        char                    *valstr = NULL;
        int                     globalinsecure = RPCSVC_AUTH_REJECT;
        int                     exportinsecure = RPCSVC_AUTH_DONTCARE;
        uint16_t                port = 0;
        gf_boolean_t            insecure = _gf_false;

        if ((!svc) || (!volname) || (!conn))
                return ret;

        ret = rpcsvc_conn_peeraddr (conn, NULL, 0, (struct sockaddr *)&sa,
                                    sasize);
        if (ret != 0) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to get peer addr: %s",
                        gai_strerror (ret));
                ret = RPCSVC_AUTH_REJECT;
                goto err;
        }

        port = ntohs (sa.sin_port);
        gf_log (GF_RPCSVC, GF_LOG_TRACE, "Client port: %d", (int)port);
        /* If the port is already a privileged one, dont bother with checking
         * options.
         */
        if (port <= 1024) {
                ret = RPCSVC_AUTH_ACCEPT;
                goto err;
        }

        /* Disabled by default */
        if ((dict_get (svc->options, "rpc-auth.ports.insecure"))) {
                ret = dict_get_str (svc->options, "rpc-auth.ports.insecure"
                                    , &srchstr);
                if (ret == 0) {
                        ret = gf_string2boolean (srchstr, &insecure);
                        if (ret == 0) {
                                if (insecure == _gf_true)
                                        globalinsecure = RPCSVC_AUTH_ACCEPT;
                        } else
                                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to"
                                        " read rpc-auth.ports.insecure value");
                } else
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to"
                                " read rpc-auth.ports.insecure value");
        }

        /* Disabled by default */
        ret = gf_asprintf (&srchstr, "rpc-auth.ports.%s.insecure", volname);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "asprintf failed");
                ret = RPCSVC_AUTH_REJECT;
                goto err;
        }

        if (dict_get (svc->options, srchstr)) {
                ret = dict_get_str (svc->options, srchstr, &valstr);
                if (ret == 0) {
                        ret = gf_string2boolean (srchstr, &insecure);
                        if (ret == 0) {
                                if (insecure == _gf_true)
                                        exportinsecure = RPCSVC_AUTH_ACCEPT;
                                else
                                        exportinsecure = RPCSVC_AUTH_REJECT;
                        } else
                                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to"
                                        " read rpc-auth.ports.insecure value");
                } else
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to"
                                " read rpc-auth.ports.insecure value");
        }

        ret = rpcsvc_combine_gen_spec_volume_checks (globalinsecure,
                                                     exportinsecure);
        if (ret == RPCSVC_AUTH_ACCEPT)
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "Unprivileged port allowed");
        else
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "Unprivileged port not"
                        " allowed");

err:
        return ret;
}


/* This needs to change to returning errors, since
 * we need to return RPC specific error messages when some
 * of the pointers below are NULL.
 */
rpcsvc_actor_t *
rpcsvc_program_actor (rpcsvc_conn_t *conn, rpcsvc_request_t *req)
{
        rpcsvc_program_t        *program = NULL;
        int                     err      = SYSTEM_ERR;
        rpcsvc_actor_t          *actor   = NULL;
        rpcsvc_t                *svc     = NULL;
        char                    found    = 0;

        if ((!conn) || (!req))
                goto err;

        svc = conn->svc;
        pthread_mutex_lock (&svc->rpclock);
        {
                list_for_each_entry (program, &svc->programs, program) {
                        if (program->prognum == req->prognum) {
                                err = PROG_MISMATCH;
                        }

                        if ((program->prognum == req->prognum)
                            && (program->progver == req->progver)) {
                                found = 1;
                                break;
                        }
                }
        }
        pthread_mutex_unlock (&svc->rpclock);

        if (!found) {
                if (err != PROG_MISMATCH) {
                        gf_log (GF_RPCSVC, GF_LOG_ERROR,
                                "RPC program not available");
                        err = PROG_UNAVAIL;
                        goto err;
                }

                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC program version not"
                        " available");
                goto err;
        }
        req->prog = program;
        if (!program->actors) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC System error");
                err = SYSTEM_ERR;
                goto err;
        }

        if ((req->procnum < 0) || (req->procnum >= program->numactors)) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC Program procedure not"
                        " available");
                err = PROC_UNAVAIL;
                goto err;
        }

        actor = &program->actors[req->procnum];
        if (!actor->actor) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC Program procedure not"
                        " available");
                err = PROC_UNAVAIL;
                actor = NULL;
                goto err;
        }

        err = SUCCESS;
        gf_log (GF_RPCSVC, GF_LOG_TRACE, "Actor found: %s - %s",
                program->progname, actor->procname);
err:
        if (req)
                req->rpc_err = err;

        return actor;
}


/* this procedure can only pass 4 arguments to registered notifyfn. To send more
 * arguements call wrapper->notify directly.
 */
inline void
rpcsvc_program_notify (rpcsvc_listener_t *listener, rpcsvc_event_t event,
                       void *data)
{
        rpcsvc_notify_wrapper_t *wrapper = NULL;

        if (!listener) {
                goto out;
        }

        list_for_each_entry (wrapper, &listener->list, list) {
                if (wrapper->notify) {
                        wrapper->notify (listener->conn->svc,
                                         wrapper->data,
                                         event, data);
                }
        }

out:
        return;
}


int
rpcsvc_accept (rpcsvc_conn_t *listen_conn, rpc_transport_t *new_trans)
{
        rpcsvc_listener_t *listener = NULL;
        rpcsvc_conn_t     *conn     = NULL;
        char               clstr[RPCSVC_PEER_STRLEN];

        listener = listen_conn->listener;
        conn = rpcsvc_conn_init (listen_conn->svc, new_trans);
        if (!conn) {
                rpc_transport_disconnect (new_trans);
                memset (clstr, 0, RPCSVC_PEER_STRLEN);
                rpc_transport_get_peername (new_trans, clstr,
                                            RPCSVC_PEER_STRLEN);
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "allocating connection for "
                        "new transport (%s) failed", clstr);
                goto out;
        }

        conn->listener = listener;

        //rpcsvc_program_notify (listener, RPCSVC_EVENT_ACCEPT, conn);
out:
        return 0;
}


void
rpcsvc_request_destroy (rpcsvc_conn_t *conn, rpcsvc_request_t *req)
{
        if (!conn || !req) {
                goto out;
        }

        if (req->iobref) {
                iobref_unref (req->iobref);
        }

        mem_put (conn->rxpool, req);
out:
        return;
}


rpcsvc_request_t *
rpcsvc_request_init (rpcsvc_conn_t *conn, struct rpc_msg *callmsg,
                     struct iovec progmsg, rpc_transport_pollin_t *msg,
                     rpcsvc_request_t *req)
{
        if ((!conn) || (!callmsg)|| (!req) || (!msg))
                return NULL;

        /* We start a RPC request as always denied. */
        req->rpc_status = MSG_DENIED;
        req->xid = rpc_call_xid (callmsg);
        req->prognum = rpc_call_program (callmsg);
        req->progver = rpc_call_progver (callmsg);
        req->procnum = rpc_call_progproc (callmsg);
        req->conn = conn;
        req->count = msg->count;
        req->msg[0] = progmsg;
        req->iobref = iobref_ref (msg->iobref);
        if (msg->vectored) {
                req->msg[1] = msg->vector[1];
        }

        req->trans_private = msg->private;

        INIT_LIST_HEAD (&req->txlist);
        req->payloadsize = 0;

        /* By this time, the data bytes for the auth scheme would have already
         * been copied into the required sections of the req structure,
         * we just need to fill in the meta-data about it now.
         */
        req->cred.flavour = rpc_call_cred_flavour (callmsg);
        req->cred.datalen = rpc_call_cred_len (callmsg);
        req->verf.flavour = rpc_call_verf_flavour (callmsg);
        req->verf.datalen = rpc_call_verf_len (callmsg);

        /* AUTH */
        rpcsvc_auth_request_init (req);
        return req;
}


rpcsvc_request_t *
rpcsvc_request_create (rpcsvc_conn_t *conn, rpc_transport_pollin_t *msg)
{
        char                    *msgbuf = NULL;
        struct rpc_msg          rpcmsg;
        struct iovec            progmsg;        /* RPC Program payload */
        rpcsvc_request_t        *req    = NULL;
        size_t                  msglen  = 0;
        int                     ret     = -1;

        if (!conn)
                return NULL;

        /* We need to allocate the request before actually calling
         * rpcsvc_request_init on the request so that we, can fill the auth
         * data directly into the request structure from the message iobuf.
         * This avoids a need to keep a temp buffer into which the auth data
         * would've been copied otherwise.
         */
        rpcsvc_alloc_request (conn, req);
        if (!req) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to alloc request");
                goto err;
        }

        msgbuf = msg->vector[0].iov_base;
        msglen = msg->vector[0].iov_len;

        ret = xdr_to_rpc_call (msgbuf, msglen, &rpcmsg, &progmsg,
                               req->cred.authdata,req->verf.authdata);

        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC call decoding failed");
                rpcsvc_request_seterr (req, GARBAGE_ARGS);
                goto err;
        }

        ret = -1;
        rpcsvc_request_init (conn, &rpcmsg, progmsg, msg, req);

        gf_log (GF_RPCSVC, GF_LOG_TRACE, "RPC XID: %lx, Ver: %ld, Program: %ld,"
                " ProgVers: %ld, Proc: %ld", rpc_call_xid (&rpcmsg),
                rpc_call_rpcvers (&rpcmsg), rpc_call_program (&rpcmsg),
                rpc_call_progver (&rpcmsg), rpc_call_progproc (&rpcmsg));

        if (rpc_call_rpcvers (&rpcmsg) != 2) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "RPC version not supported");
                rpcsvc_request_seterr (req, RPC_MISMATCH);
                goto err;
        }

        ret = rpcsvc_authenticate (req);
        if (ret == RPCSVC_AUTH_REJECT) {
                /* No need to set auth_err, that is the responsibility of
                 * the authentication handler since only that know what exact
                 * error happened.
                 */
                rpcsvc_request_seterr (req, AUTH_ERROR);
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed authentication");
                ret = -1;
                goto err;
        }


        /* If the error is not RPC_MISMATCH, we consider the call as accepted
         * since we are not handling authentication failures for now.
         */
        req->rpc_status = MSG_ACCEPTED;
        ret = 0;
err:
        if (ret == -1) {
                ret = rpcsvc_error_reply (req);
                if (ret)
                        gf_log ("rpcsvc", GF_LOG_DEBUG,
                                "failed to queue error reply");
                req = NULL;
        }

        return req;
}


int
rpcsvc_handle_rpc_call (rpcsvc_conn_t *conn, rpc_transport_pollin_t *msg)
{
        rpcsvc_actor_t          *actor = NULL;
        rpcsvc_request_t        *req = NULL;
        int                     ret = -1;

        if (!conn)
                return -1;

        req = rpcsvc_request_create (conn, msg);
        if (!req)
                goto err;

        if (!rpcsvc_request_accepted (req))
                goto err_reply;

        actor = rpcsvc_program_actor (conn, req);
        if (!actor)
                goto err_reply;

        if (actor && (req->rpc_err == SUCCESS)) {
                if (req->count == 2) {
                        if (actor->vector_actor) {
                                rpcsvc_conn_ref (conn);
                                ret = actor->vector_actor (req, &req->msg[1], 1,
                                                           req->iobref);
                        } else {
                                rpcsvc_request_seterr (req, PROC_UNAVAIL);
                                gf_log (GF_RPCSVC, GF_LOG_ERROR,
                                        "No vectored handler present");
                                ret = RPCSVC_ACTOR_ERROR;
                        }
                } else if (actor->actor) {
                        rpcsvc_conn_ref (req->conn);
                        /* Before going to xlator code, set the THIS properly */
                        THIS = conn->svc->mydata;
                        ret = actor->actor (req);
                }
        }

err_reply:
        if ((ret == RPCSVC_ACTOR_ERROR) || (req->rpc_err != SUCCESS))
                ret = rpcsvc_error_reply (req);

        if (ret)
                gf_log ("rpcsvc", GF_LOG_DEBUG, "failed to queue error reply");

        /* No need to propagate error beyond this function since the reply
         * has now been queued. */
        ret = 0;
err:
        return ret;
}


int
rpcsvc_notify (rpc_transport_t *trans, void *mydata,
               rpc_transport_event_t event, void *data, ...)
{
        rpcsvc_conn_t          *conn      = NULL;
        int                     ret       = -1;
        rpc_transport_pollin_t *msg       = NULL;
        rpc_transport_t        *new_trans = NULL;

        conn = mydata;
        if (conn == NULL) {
                goto out;
        }

        switch (event) {
        case RPC_TRANSPORT_ACCEPT:
                new_trans = data;
                ret = rpcsvc_accept (conn, new_trans);
                break;

        case RPC_TRANSPORT_DISCONNECT:
                rpcsvc_conn_deinit (conn);
                ret = 0;
                break;

        case RPC_TRANSPORT_MSG_RECEIVED:
                msg = data;
                ret = rpcsvc_handle_rpc_call (conn, msg);
                break;

        case RPC_TRANSPORT_MSG_SENT:
                ret = 0;
                break;

        case RPC_TRANSPORT_CONNECT:
                /* do nothing, no need for rpcsvc to handle this, client should
                 * handle this event
                 */
                gf_log ("rpcsvc", GF_LOG_CRITICAL,
                        "got CONNECT event, which should have not come");
                ret = 0;
                break;

        case RPC_TRANSPORT_CLEANUP:
                /* FIXME: think about this later */
                ret = 0;
                break;

        case RPC_TRANSPORT_MAP_XID_REQUEST:
                /* FIXME: think about this later */
                gf_log ("rpcsvc", GF_LOG_CRITICAL,
                        "got MAP_XID event, which should have not come");
                ret = 0;
                break;
        }

out:
        return ret;
}


void
rpcsvc_set_lastfrag (uint32_t *fragsize) {
        (*fragsize) |= 0x80000000U;
}

void
rpcsvc_set_frag_header_size (uint32_t size, char *haddr)
{
        size = htonl (size);
        memcpy (haddr, &size, sizeof (size));
}

void
rpcsvc_set_last_frag_header_size (uint32_t size, char *haddr)
{
        rpcsvc_set_lastfrag (&size);
        rpcsvc_set_frag_header_size (size, haddr);
}


/* Given the RPC reply structure and the payload handed by the RPC program,
 * encode the RPC record header into the buffer pointed by recordstart.
 */
struct iovec
rpcsvc_record_build_header (char *recordstart, size_t rlen,
                            struct rpc_msg reply, size_t payload)
{
        struct iovec    replyhdr;
        struct iovec    txrecord = {0, 0};
        size_t          fraglen = 0;
        int             ret = -1;

        /* After leaving aside the 4 bytes for the fragment header, lets
         * encode the RPC reply structure into the buffer given to us.
         */
        ret = rpc_reply_to_xdr (&reply,(recordstart + RPCSVC_FRAGHDR_SIZE),
                                rlen, &replyhdr);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to create RPC reply");
                goto err;
        }

        fraglen = payload + replyhdr.iov_len;
        gf_log (GF_RPCSVC, GF_LOG_TRACE, "Reply fraglen %zu, payload: %zu, "
                "rpc hdr: %zu", fraglen, payload, replyhdr.iov_len);

        /* Since we're not spreading RPC records over mutiple fragments
         * we just set this fragment as the first and last fragment for this
         * record.
         */
        rpcsvc_set_last_frag_header_size (fraglen, recordstart);

        /* Even though the RPC record starts at recordstart+RPCSVC_FRAGHDR_SIZE
         * we need to transmit the record with the fragment header, which starts
         * at recordstart.
         */
        txrecord.iov_base = recordstart;

        /* Remember, this is only the vec for the RPC header and does not
         * include the payload above. We needed the payload only to calculate
         * the size of the full fragment. This size is sent in the fragment
         * header.
         */
        txrecord.iov_len = RPCSVC_FRAGHDR_SIZE + replyhdr.iov_len;
err:
        return txrecord;
}


int
rpcsvc_conn_submit (rpcsvc_conn_t *conn, struct iovec *hdrvec,
                    int hdrcount, struct iovec *proghdr, int proghdrcount,
                    struct iovec *progpayload, int progpayloadcount,
                    struct iobref *iobref, void *priv)
{
        int                   ret   = -1;
        rpc_transport_reply_t reply = {{0, }};

        if ((!conn) || (!hdrvec) || (!hdrvec->iov_base) || (!conn->trans)) {
                goto out;
        }

        reply.msg.rpchdr = hdrvec;
        reply.msg.rpchdrcount = hdrcount;
        reply.msg.proghdr = proghdr;
        reply.msg.proghdrcount = proghdrcount;
        reply.msg.progpayload = progpayload;
        reply.msg.progpayloadcount = progpayloadcount;
        reply.msg.iobref = iobref;
        reply.private = priv;

        /* Now that we have both the RPC and Program buffers in xdr format
         * lets hand it to the transmission layer.
         */
        if (!rpcsvc_conn_check_active (conn)) {
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "Connection inactive");
                goto out;
        }

        ret = rpc_transport_submit_reply (conn->trans, &reply);

out:
        return ret;
}


int
rpcsvc_fill_reply (rpcsvc_request_t *req, struct rpc_msg *reply)
{
        rpcsvc_program_t        *prog = NULL;
        if ((!req) || (!reply))
                return -1;

        prog = rpcsvc_request_program (req);
        rpc_fill_empty_reply (reply, req->xid);

        if (req->rpc_status == MSG_DENIED)
                rpc_fill_denied_reply (reply, req->rpc_err, req->auth_err);
        else if (req->rpc_status == MSG_ACCEPTED)
                rpc_fill_accepted_reply (reply, req->rpc_err, prog->proglowvers,
                                         prog->proghighvers, req->verf.flavour,
                                         req->verf.datalen,
                                         req->verf.authdata);
        else
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Invalid rpc_status value");

        return 0;
}


/* Given a request and the reply payload, build a reply and encodes the reply
 * into a record header. This record header is encoded into the vector pointed
 * to be recbuf.
 * msgvec is the buffer that points to the payload of the RPC program.
 * This buffer can be NULL, if an RPC error reply is being constructed.
 * The only reason it is needed here is that in case the buffer is provided,
 * we should account for the length of that buffer in the RPC fragment header.
 */
struct iobuf *
rpcsvc_record_build_record (rpcsvc_request_t *req, size_t payload,
                            struct iovec *recbuf)
{
        struct rpc_msg          reply;
        struct iobuf            *replyiob = NULL;
        char                    *record = NULL;
        struct iovec            recordhdr = {0, };
        size_t                  pagesize = 0;
        rpcsvc_conn_t           *conn = NULL;
        rpcsvc_t                *svc = NULL;

        if ((!req) || (!req->conn) || (!recbuf))
                return NULL;

        /* First, try to get a pointer into the buffer which the RPC
         * layer can use.
         */
        conn = req->conn;
        svc = rpcsvc_conn_rpcsvc (conn);
        replyiob = iobuf_get (svc->ctx->iobuf_pool);
        pagesize = iobpool_pagesize ((struct iobuf_pool *)svc->ctx->iobuf_pool);
        if (!replyiob) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to get iobuf");
                goto err_exit;
        }

        record = iobuf_ptr (replyiob);  /* Now we have it. */

        /* Fill the rpc structure and XDR it into the buffer got above. */
        rpcsvc_fill_reply (req, &reply);
        recordhdr = rpcsvc_record_build_header (record, pagesize, reply,
                                                payload);
        if (!recordhdr.iov_base) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to build record "
                        " header");
                iobuf_unref (replyiob);
                replyiob = NULL;
                recbuf->iov_base = NULL;
                goto err_exit;
        }

        recbuf->iov_base = recordhdr.iov_base;
        recbuf->iov_len = recordhdr.iov_len;
err_exit:
        return replyiob;
}


/*
 * The function to submit a program message to the RPC service.
 * This message is added to the transmission queue of the
 * conn.
 *
 * Program callers are not expected to use the msgvec->iov_base
 * address for anything else.
 * Nor are they expected to free it once this function returns.
 * Once the transmission of the buffer is completed by the RPC service,
 * the memory area as referenced through @msg will be unrefed.
 * If a higher layer does not want anything to do with this iobuf
 * after this function returns, it should call unref on it. For keeping
 * it around till the transmission is actually complete, rpcsvc also refs it.
 *  *
 * If this function returns an error by returning -1, the
 * higher layer programs should assume that a disconnection happened
 * and should know that the conn memory area as well as the req structure
 * has been freed internally.
 *
 * For now, this function assumes that a submit is always called
 * to send a new record. Later, if there is a situation where different
 * buffers for the same record come from different sources, then we'll
 * need to change this code to account for multiple submit calls adding
 * the buffers into a single record.
 */

int
rpcsvc_submit_generic (rpcsvc_request_t *req, struct iovec *proghdr,
                       int hdrcount, struct iovec *payload, int payloadcount,
                       struct iobref *iobref)
{
        int                     ret         = -1, i = 0;
        struct iobuf            *replyiob   = NULL;
        struct iovec            recordhdr   = {0, };
        rpcsvc_conn_t           *conn       = NULL;
        size_t                   msglen     = 0;
        char                     new_iobref = 0;

        if ((!req) || (!req->conn))
                return -1;

        conn = req->conn;

        for (i = 0; i < hdrcount; i++) {
                msglen += proghdr[i].iov_len;
        }

        for (i = 0; i < payloadcount; i++) {
                msglen += payload[i].iov_len;
        }

        gf_log (GF_RPCSVC, GF_LOG_TRACE, "Tx message: %zu", msglen);

        /* Build the buffer containing the encoded RPC reply. */
        replyiob = rpcsvc_record_build_record (req, msglen, &recordhdr);
        if (!replyiob) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR,"Reply record creation failed");
                goto disconnect_exit;
        }

        if (!iobref) {
                iobref = iobref_new ();
                if (!iobref) {
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "memory allocation "
                                "failed");
                        goto disconnect_exit;
                }

                new_iobref = 1;
        }

        iobref_add (iobref, replyiob);

        ret = rpcsvc_conn_submit (conn, &recordhdr, 1, proghdr, hdrcount,
                                  payload, payloadcount, iobref,
                                  req->trans_private);

        rpcsvc_request_destroy (conn, req);

        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to submit message");
        }

disconnect_exit:
        if (replyiob) {
                iobuf_unref (replyiob);
        }

        if (new_iobref) {
                iobref_unref (iobref);
        }

        /* Note that a unref is called everytime a reply is sent. This is in
         * response to the ref that is performed on the conn when a request is
         * handed to the RPC program.
         *
         * The catch, however, is that if the reply is an rpc error, we must
         * not unref. This is because the ref only contains
         * references for the actors to which the request was handed plus one
         * reference maintained by the RPC layer. By unrefing for a case where
         * no actor was called, we will be losing the ref held for the RPC
         * layer.
         */
        if ((rpcsvc_request_accepted (req)) &&
            (rpcsvc_request_accepted_success (req)))
                rpcsvc_conn_unref (conn);

        return ret;
}


int
rpcsvc_error_reply (rpcsvc_request_t *req)
{
        struct iovec    dummyvec = {0, };

        if (!req)
                return -1;

        /* At this point the req should already have been filled with the
         * appropriate RPC error numbers.
         */
        return rpcsvc_submit_generic (req, &dummyvec, 0, NULL, 0, NULL);
}


/* Register the program with the local portmapper service. */
int
rpcsvc_program_register_portmap (rpcsvc_program_t *newprog, rpcsvc_conn_t *conn)
{
        int                ret   = 0;
        struct sockaddr_in sa    = {0, };

        if (!newprog || !conn->trans) {
                goto out;
        }

        if (!(pmap_set (newprog->prognum, newprog->progver, IPPROTO_TCP,
                        sa.sin_port))) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Could not register with"
                        " portmap");
                goto out;
        }

        ret = 0;
out:
        return ret;
}


int
rpcsvc_program_unregister_portmap (rpcsvc_program_t *prog)
{
        if (!prog)
                return -1;

        if (!(pmap_unset(prog->prognum, prog->progver))) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Could not unregister with"
                        " portmap");
                return -1;
        }

        return 0;
}


rpcsvc_listener_t *
rpcsvc_get_listener (rpcsvc_t *svc, uint16_t port)
{
        rpcsvc_listener_t  *listener = NULL;
        char                found    = 0;

        if (!svc) {
                goto out;
        }

        pthread_mutex_lock (&svc->rpclock);
        {
                list_for_each_entry (listener, &svc->listeners, list) {
                        if (((struct sockaddr_in *)&listener->sa)->sin_port
                            == port) {
                                found = 1;
                                break;
                        }
                }
        }
        pthread_mutex_unlock (&svc->rpclock);

        if (!found) {
                listener = NULL;
        }

out:
        return listener;
}


/* The only difference between the generic submit and this one is that the
 * generic submit is also used for submitting RPC error replies in where there
 * are no payloads so the msgvec and msgbuf can be NULL.
 * Since RPC programs should be using this function along with their payloads
 * we must perform NULL checks before calling the generic submit.
 */
int
rpcsvc_submit_message (rpcsvc_request_t *req, struct iovec *proghdr,
                       int hdrcount, struct iovec *payload, int payloadcount,
                       struct iobref *iobref)
{
        if ((!req) || (!req->conn) || (!proghdr) || (!proghdr->iov_base))
                return -1;

        return rpcsvc_submit_generic (req, proghdr, hdrcount, payload,
                                      payloadcount, iobref);
}


int
rpcsvc_program_unregister (rpcsvc_t *svc, rpcsvc_program_t prog)
{
        int                     ret = -1;

        if (!svc)
                return -1;

        /* TODO: De-init the listening connection for this program. */
        ret = rpcsvc_program_unregister_portmap (&prog);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "portmap unregistration of"
                        " program failed");
                goto err;
        }

        ret = 0;
        gf_log (GF_RPCSVC, GF_LOG_DEBUG, "Program unregistered: %s, Num: %d,"
                " Ver: %d, Port: %d", prog.progname, prog.prognum,
                prog.progver, prog.progport);

err:
        if (ret == -1)
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Program unregistration failed"
                        ": %s, Num: %d, Ver: %d, Port: %d", prog.progname,
                        prog.prognum, prog.progver, prog.progport);

        return ret;
}


int
rpcsvc_conn_peername (rpcsvc_conn_t *conn, char *hostname, int hostlen)
{
        if (!conn || !conn->trans)
                return -1;

        return rpc_transport_get_peername (conn->trans, hostname, hostlen);
}


int
rpcsvc_conn_peeraddr (rpcsvc_conn_t *conn, char *addrstr, int addrlen,
                      struct sockaddr *sa, socklen_t sasize)
{
        if (!conn || !conn->trans)
                return -1;

        return rpc_transport_get_peeraddr(conn->trans, addrstr, addrlen, sa,
                                          sasize);
}


rpcsvc_conn_t *
rpcsvc_conn_create (rpcsvc_t *svc, dict_t *options, char *name)
{
        int            ret   = -1;
        rpc_transport_t   *trans = NULL;
        rpcsvc_conn_t *conn  = NULL;

        trans = rpc_transport_load (svc->ctx, options, name);
        if (!trans) {
                gf_log (GF_RPCSVC, GF_LOG_DEBUG, "cannot create listener, "
                        "initing the transport failed");
                goto out;
        }

        ret = rpc_transport_listen (trans);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_DEBUG,
                        "listening on transport failed");
                goto out;
        }

        conn = rpcsvc_conn_init (svc, trans);
        if (!conn) {
                ret = -1;
                gf_log (GF_RPCSVC, GF_LOG_DEBUG,
                        "initializing connection for transport failed");
                goto out;
        }

        ret = 0;
out:
        if ((ret == -1) && (trans)) {
                rpc_transport_disconnect (trans);
        }

        return conn;
}

rpcsvc_listener_t *
rpcsvc_listener_alloc (rpcsvc_t *svc, rpcsvc_conn_t *conn)
{
        rpcsvc_listener_t *listener = NULL;

        listener = GF_CALLOC (1, sizeof (*listener),
                              gf_common_mt_rpcsvc_listener_t);
        if (!listener) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "memory allocation failed");
                goto out;
        }

        listener->conn = conn;

        INIT_LIST_HEAD (&listener->list);

        pthread_mutex_lock (&svc->rpclock);
        {
                list_add_tail (&listener->list, &svc->listeners);
        }
        pthread_mutex_unlock (&svc->rpclock);
out:
        return listener;
}


rpcsvc_listener_t *
rpcsvc_create_listener (rpcsvc_t *svc, dict_t *options, char *name)
{
        rpcsvc_conn_t     *conn     = NULL;
        rpcsvc_listener_t *listener = NULL;

        if (!svc || !options) {
                goto out;
        }

        conn = rpcsvc_conn_create (svc, options, name);
        if (!conn) {
                goto out;
        }

        listener = rpcsvc_listener_alloc (svc, conn);
        if (listener == NULL) {
                goto out;
        }

        conn->listener = listener;
out:
        if (!listener && conn) {
                rpcsvc_conn_deinit (conn);
        }

        return listener;
}


int
rpcsvc_unregister_notify (rpcsvc_t *svc, rpcsvc_notify_t notify, void *mydata)
{
        rpcsvc_notify_wrapper_t *wrapper = NULL, *tmp = NULL;
        int                      ret     = 0;

        if (!svc || !notify) {
                goto out;
        }

        pthread_mutex_lock (&svc->rpclock);
        {
                list_for_each_entry_safe (wrapper, tmp, &svc->notify, list) {
                        if ((wrapper->notify == notify)
                            && (mydata == wrapper->data)) {
                                list_del_init (&wrapper->list);
                                GF_FREE (wrapper);
                                ret++;
                        }
                }
        }
        pthread_mutex_unlock (&svc->rpclock);

out:
        return ret;
}

int
rpcsvc_register_notify (rpcsvc_t *svc, rpcsvc_notify_t notify, void *mydata)
{
        rpcsvc_notify_wrapper_t *wrapper = NULL;
        int                      ret     = -1;

        wrapper = rpcsvc_notify_wrapper_alloc ();
        if (!wrapper) {
                goto out;
        }
        svc->mydata   = mydata;  /* this_xlator */
        wrapper->data = mydata;
        wrapper->notify = notify;

        pthread_mutex_lock (&svc->rpclock);
        {
                list_add_tail (&wrapper->list, &svc->notify);
                svc->notify_count++;
        }
        pthread_mutex_unlock (&svc->rpclock);

        ret = 0;
out:
        return ret;
}



int
rpcsvc_program_register (rpcsvc_t *svc, rpcsvc_program_t program)
{
        rpcsvc_program_t        *newprog       = NULL;
        int                      ret           = -1;
        rpcsvc_listener_t       *listener      = NULL;

        if (!svc)
                return -1;

        newprog = GF_CALLOC (1, sizeof(*newprog), gf_common_mt_rpcsvc_program_t);
        if (!newprog)
                return -1;

        if (!program.actors)
                goto free_prog;

        memcpy (newprog, &program, sizeof (program));

        listener = svc->listener;

        ret = rpcsvc_program_register_portmap (newprog, listener->conn);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "portmap registration of"
                        " program failed");
                goto free_prog;
        }

        pthread_mutex_lock (&svc->rpclock);
        {
                list_add_tail (&newprog->program, &svc->programs);
        }
        pthread_mutex_unlock (&svc->rpclock);

        ret = 0;
        gf_log (GF_RPCSVC, GF_LOG_DEBUG, "New program registered: %s, Num: %d,"
                " Ver: %d, Port: %d", newprog->progname, newprog->prognum,
                newprog->progver, newprog->progport);

free_prog:
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Program registration failed:"
                        " %s, Num: %d, Ver: %d, Port: %d", newprog->progname,
                        newprog->prognum, newprog->progver, newprog->progport);
                GF_FREE (newprog);
        }

        return ret;
}

static void
free_prog_details (gf_dump_rsp *rsp)
{
        gf_prog_detail *prev = NULL;
        gf_prog_detail *trav = NULL;

        trav = rsp->prog;
        while (trav) {
                prev = trav;
                trav = trav->next;
                GF_FREE (prev);
        }
}

static int
build_prog_details (rpcsvc_request_t *req, gf_dump_rsp *rsp)
{
        int               ret     = -1;
        rpcsvc_program_t *program = NULL;
        gf_prog_detail   *prog    = NULL;
        gf_prog_detail   *prev    = NULL;

        if (!req || !req->conn || !req->conn->svc)
                goto out;

        list_for_each_entry (program, &req->conn->svc->programs, program) {
                prog = GF_CALLOC (1, sizeof (*prog), 0);
                if (!prog)
                        goto out;
                prog->progname = program->progname;
                prog->prognum  = program->prognum;
                prog->progver  = program->progver;
                if (!rsp->prog)
                        rsp->prog = prog;
                if (prev)
                        prev->next = prog;
                prev = prog;
        }
        if (prev)
                ret = 0;
out:
        return ret;
}

static int
rpcsvc_dump (rpcsvc_request_t *req)
{
        char         rsp_buf[8 * 1024] = {0,};
        gf_dump_rsp  rsp      = {0,};
        struct iovec iov      = {0,};
        int          op_errno = EINVAL;
        int          ret      = -1;

        if (!req)
                goto fail;

        ret = build_prog_details (req, &rsp);
        if (ret < 0) {
                op_errno = -ret;
                goto fail;
        }

fail:
        rsp.op_errno = gf_errno_to_error (op_errno);
        rsp.op_ret   = ret;

        iov.iov_base = rsp_buf;
        iov.iov_len  = (8 * 1024);

        ret = xdr_serialize_dump_rsp (iov, &rsp);
        if (ret < 0) {
                if (req)
                        req->rpc_err = GARBAGE_ARGS;
                op_errno = EINVAL;
                goto fail;
        }

        ret = rpcsvc_submit_generic (req, &iov, 1, NULL, 0,
                                     NULL);

        free_prog_details (&rsp);

        return ret;
}

int
rpcsvc_init_options (rpcsvc_t *svc, dict_t *options)
{
        svc->memfactor = RPCSVC_DEFAULT_MEMFACTOR;
        return 0;
}


/* The global RPC service initializer.
 */
rpcsvc_t *
rpcsvc_init (glusterfs_ctx_t *ctx, dict_t *options)
{
        rpcsvc_t          *svc      = NULL;
        int                ret      = -1;
        rpcsvc_listener_t *listener = NULL;

        if ((!ctx) || (!options))
                return NULL;

        svc = GF_CALLOC (1, sizeof (*svc), gf_common_mt_rpcsvc_t);
        if (!svc)
                return NULL;

        pthread_mutex_init (&svc->rpclock, NULL);
        INIT_LIST_HEAD (&svc->authschemes);
        INIT_LIST_HEAD (&svc->notify);
        INIT_LIST_HEAD (&svc->listeners);
        INIT_LIST_HEAD (&svc->programs);

        ret = rpcsvc_init_options (svc, options);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to init options");
                goto free_svc;
        }

        ret = rpcsvc_auth_init (svc, options);
        if (ret == -1) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "Failed to init "
                        "authentication");
                goto free_svc;
        }

        ret = -1;
        svc->options = options;
        svc->ctx = ctx;
        gf_log (GF_RPCSVC, GF_LOG_DEBUG, "RPC service inited.");

        /* One listen port per RPC */
        listener = rpcsvc_get_listener (svc, 0);
        if (!listener) {
                /* FIXME: listener is given the name of first program that
                 * creates it. This is not always correct. For eg., multiple
                 * programs can be listening on the same listener
                 * (glusterfs 3.1.0, 3.1.2, 3.1.3 etc).
                 */
                listener = rpcsvc_create_listener (svc, options, "RPC");
                if (!listener) {
                        gf_log (GF_RPCSVC, GF_LOG_ERROR, "creation of listener"
                                " for program failed");
                        goto free_svc;
                }
        }

        if (!listener->conn) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR, "listener with no connection "
                        "found");
                goto free_svc;
        }

        svc->listener = listener;

        ret = rpcsvc_program_register (svc, gluster_dump_prog);
        if (ret) {
                gf_log (GF_RPCSVC, GF_LOG_ERROR,
                        "failed to register DUMP program");
                goto free_svc;
        }
        ret = 0;
free_svc:
        if (ret == -1) {
                GF_FREE (svc);
                svc = NULL;
        }

        return svc;
}


rpcsvc_actor_t gluster_dump_actors[] = {
        [GF_DUMP_NULL] = {"NULL", GF_DUMP_NULL, NULL, NULL, NULL },
        [GF_DUMP_DUMP] = {"DUMP", GF_DUMP_DUMP, rpcsvc_dump, NULL, NULL },
        [GF_DUMP_MAXVALUE] = {"MAXVALUE", GF_DUMP_MAXVALUE, NULL, NULL, NULL },
};


struct rpcsvc_program gluster_dump_prog = {
        .progname  = "GF-DUMP",
        .prognum   = GLUSTER_DUMP_PROGRAM,
        .progver   = GLUSTER_DUMP_VERSION,
        .actors    = gluster_dump_actors,
        .numactors = 2,
};
