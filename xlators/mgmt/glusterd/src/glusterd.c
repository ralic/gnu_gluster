/*
  Copyright (c) 2006-2009 Gluster, Inc. <http://www.gluster.com>
  This file is part of GlusterFS.

  GlusterFS is GF_FREE software; you can redistribute it and/or modify
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
#include <time.h>
#include <sys/uio.h>
#include <sys/resource.h>

#include <libgen.h>
#include "uuid.h"

#include "glusterd.h"
#include "rpcsvc.h"
#include "fnmatch.h"
#include "xlator.h"
#include "call-stub.h"
#include "defaults.h"
#include "list.h"
#include "dict.h"
#include "compat.h"
#include "compat-errno.h"
#include "statedump.h"
#include "glusterd-sm.h"
#include "glusterd-op-sm.h"
#include "glusterd-store.h"


static uuid_t glusterd_uuid;
extern struct rpcsvc_program glusterd1_mop_prog;
extern struct rpcsvc_program gluster_handshake_prog;
extern struct rpc_clnt_program glusterd3_1_mgmt_prog;
extern struct rpcsvc_program gluster_pmap_prog;
extern glusterd_op_info_t opinfo;


static int
glusterd_opinfo_init ()
{
        int32_t ret = -1;

        ret = pthread_mutex_init (&opinfo.lock, NULL);

        return ret;
}

static int
glusterd_uuid_init ()
{
        int             ret = -1;
        char            str[50] = {0,};
        glusterd_conf_t *priv = NULL;

        priv = THIS->private;
        ret = glusterd_retrieve_uuid ();

        if (!ret) {
                uuid_unparse (priv->uuid, str);
                uuid_copy (glusterd_uuid, priv->uuid);
                gf_log ("glusterd", GF_LOG_NORMAL,
                                "retrieved UUID: %s", str);
                return 0;
        }


        uuid_generate (glusterd_uuid);
        uuid_unparse (glusterd_uuid, str);

        gf_log ("glusterd", GF_LOG_NORMAL,
                        "generated UUID: %s",str);
        uuid_copy (priv->uuid, glusterd_uuid);

        ret = glusterd_store_uuid ();

        if (ret) {
                gf_log ("glusterd", GF_LOG_ERROR,
                          "Unable to store generated UUID");
                return ret;
        }

        return 0;
}



int
glusterd_priv (xlator_t *this)
{
        return 0;
}



int32_t
mem_acct_init (xlator_t *this)
{
        int     ret = -1;

        if (!this)
                return ret;

        ret = xlator_mem_acct_init (this, gf_gld_mt_end + 1);

        if (ret != 0) {
                gf_log (this->name, GF_LOG_ERROR, "Memory accounting init"
                        " failed");
                return ret;
        }

        return ret;
}

int
glusterd_rpcsvc_notify (rpcsvc_t *rpc, void *xl, rpcsvc_event_t event,
                     void *data)
{
        xlator_t            *this = NULL;
        rpc_transport_t     *xprt = NULL;

        if (!xl || !data) {
                gf_log ("glusterd", GF_LOG_WARNING,
                        "Calling rpc_notify without initializing");
                goto out;
        }

        this = xl;
        xprt = data;

        switch (event) {
        case RPCSVC_EVENT_ACCEPT:
        {
                xprt->mydata = this;
                break;
        }
        case RPCSVC_EVENT_DISCONNECT:
                break;

        default:
                break;
        }

out:
        return 0;
}

/*
 * init - called during glusterd initialization
 *
 * @this:
 *
 */
int
init (xlator_t *this)
{
        int32_t         ret = -1;
        rpcsvc_t        *rpc = NULL;
        glusterd_conf_t *conf = NULL;
        data_t          *dir_data = NULL;
        char            dirname [PATH_MAX];
        struct stat     buf = {0,};
        char            *port_str = NULL;
        int             port_num = 0;
        char            voldir [PATH_MAX] = {0,};


        dir_data = dict_get (this->options, "working-directory");

        if (!dir_data) {
                //Use default working dir
                strncpy (dirname, GLUSTERD_DEFAULT_WORKDIR, PATH_MAX);
        } else {
                strncpy (dirname, dir_data->data, PATH_MAX);
        }

        ret = stat (dirname, &buf);
        if ((ret != 0) && (ENOENT != errno)) {
                gf_log (this->name, GF_LOG_ERROR,
                        "stat fails on %s, exiting. (errno = %d)",
			dirname, errno);
                exit (1);
        }

        if ((!ret) && (!S_ISDIR(buf.st_mode))) {
                gf_log (this->name, GF_LOG_CRITICAL,
                        "Provided working area %s is not a directory,"
                        "exiting", dirname);
                exit (1);
        }


        if ((-1 == ret) && (ENOENT == errno)) {
                ret = mkdir (dirname, 0777);

                if (-1 == ret) {
                        gf_log (this->name, GF_LOG_CRITICAL,
                                "Unable to create directory %s"
                                " ,errno = %d", dirname, errno);
                }
        }

        gf_log (this->name, GF_LOG_NORMAL, "Using %s as working directory",
                dirname);

        snprintf (voldir, PATH_MAX, "%s/vols", dirname);

        ret = mkdir (voldir, 0777);

        if ((-1 == ret) && (errno != EEXIST)) {
                gf_log (this->name, GF_LOG_CRITICAL,
                        "Unable to create volume directory %s"
                        " ,errno = %d", voldir, errno);
        }

        snprintf (voldir, PATH_MAX, "%s/peers", dirname);

        ret = mkdir (voldir, 0777);

        if ((-1 == ret) && (errno != EEXIST)) {
                gf_log (this->name, GF_LOG_CRITICAL,
                        "Unable to create peers directory %s"
                        " ,errno = %d", voldir, errno);
        }

        rpc = rpcsvc_init (this->ctx, this->options);
        if (rpc == NULL) {
                gf_log (this->name, GF_LOG_ERROR,
                        "failed to init rpc");
                goto out;
        }

        ret = rpcsvc_register_notify (rpc, glusterd_rpcsvc_notify, this);
        if (ret) {
                gf_log (this->name, GF_LOG_ERROR,
                        "rpcsvc_register_notify returned %d", ret);
                goto out;
        }

        glusterd1_mop_prog.options = this->options;
        port_str = getenv ("GLUSTERD_LOCAL_PORT");
        if (port_str) {
                port_num = atoi (port_str);
                glusterd1_mop_prog.progport = port_num;
        }

        ret = rpcsvc_program_register (rpc, glusterd1_mop_prog);
        if (ret) {
                gf_log (this->name, GF_LOG_ERROR,
                        "rpcsvc_program_register returned %d", ret);
                goto out;
        }

        ret = rpcsvc_program_register (rpc, gluster_pmap_prog);
        if (ret) {
                gf_log (this->name, GF_LOG_ERROR,
                        "rpcsvc_program_register returned %d", ret);
                goto out;
        }

        gluster_handshake_prog.options = this->options;
        ret = rpcsvc_program_register (rpc, gluster_handshake_prog);
        if (ret)
                goto out;

        conf = GF_CALLOC (1, sizeof (glusterd_conf_t),
                          gf_gld_mt_glusterd_conf_t);
        GF_VALIDATE_OR_GOTO(this->name, conf, out);
        INIT_LIST_HEAD (&conf->peers);
        INIT_LIST_HEAD (&conf->volumes);
        INIT_LIST_HEAD (&conf->hostnames);
        pthread_mutex_init (&conf->mutex, NULL);
        conf->rpc = rpc;
        conf->mgmt = &glusterd3_1_mgmt_prog;
        strncpy (conf->workdir, dirname, PATH_MAX);

        this->private = conf;
        //this->ctx->top = this;

        ret = glusterd_uuid_init ();

        if (ret < 0)
                goto out;

        ret = glusterd_restore ();
        if (ret < 0)
                goto out;

        glusterd_friend_sm_init ();
        glusterd_op_sm_init ();
        glusterd_opinfo_init ();


        ret = 0;
out:
        return ret;
}




/*
 * fini - finish function for glusterd, called before
 *        unloading gluster.
 *
 * @this:
 *
 */
void
fini (xlator_t *this)
{
        glusterd_conf_t *conf = this->private;

        GF_VALIDATE_OR_GOTO(this->name, conf, out);
        GF_FREE (conf);
        this->private = NULL;
out:
        return;
}

/*
 * notify - notify function for glusterd
 * @this:
 * @trans:
 * @event:
 *
 */
int
notify (xlator_t *this, int32_t event, void *data, ...)
{
        int          ret = 0;
        //transport_t *trans = data;
        //peer_info_t *peerinfo = NULL;
        //peer_info_t *myinfo = NULL;

/*        if (trans != NULL) {
                peerinfo = &(trans->peerinfo);
                myinfo = &(trans->myinfo);
        }
*/
        switch (event) {

                case GF_EVENT_POLLIN:
          //              ret = glusterd_pollin (this, trans);
                        break;


                case GF_EVENT_POLLERR:
                        break;

                case GF_EVENT_TRANSPORT_CLEANUP:
                        break;

                default:
                        default_notify (this, event, data);
                        break;

        }

        return ret;
}



//struct xlator_mops mops = {
//};

struct xlator_fops fops = {
};

struct xlator_cbks cbks = {
};

struct xlator_dumpops dumpops = {
        .priv  = glusterd_priv,
};


struct volume_options options[] = {
        { .key   = {"working-dir"},
          .type  = GF_OPTION_TYPE_PATH,
        },

        { .key   = {NULL} },
};
