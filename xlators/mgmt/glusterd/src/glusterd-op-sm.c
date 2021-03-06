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

#include "fnmatch.h"
#include "xlator.h"
#include "protocol-common.h"
#include "glusterd.h"
#include "call-stub.h"
#include "defaults.h"
#include "list.h"
#include "dict.h"
#include "compat.h"
#include "compat-errno.h"
#include "statedump.h"
#include "glusterd-sm.h"
#include "glusterd-op-sm.h"
#include "glusterd-utils.h"
#include "glusterd-store.h"
#include "cli1.h"

static struct list_head gd_op_sm_queue;
glusterd_op_info_t    opinfo;
static int glusterfs_port = GLUSTERD_DEFAULT_PORT;

static void
glusterd_set_volume_status (glusterd_volinfo_t  *volinfo,
                            glusterd_volume_status status)
{
        GF_ASSERT (volinfo);
        volinfo->status = status;
}

static int
glusterd_is_volume_started (glusterd_volinfo_t  *volinfo)
{
        GF_ASSERT (volinfo);
        return (!(volinfo->status == GLUSTERD_STATUS_STARTED));
}

static int
glusterd_op_get_len (glusterd_op_t op)
{
        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);
        int             ret = -1;

        switch (op) {
                case GD_OP_CREATE_VOLUME:
                        {
                                dict_t *dict = glusterd_op_get_ctx (op);
                                ret = dict_serialized_length (dict);
                                return ret;
                        }
                        break;

                case GD_OP_START_BRICK:
                        break;

                case GD_OP_REPLACE_BRICK:
                case GD_OP_ADD_BRICK:
                        {
                                dict_t *dict = glusterd_op_get_ctx (op);
                                ret = dict_serialized_length (dict);
                                return ret;
                        }

                 case GD_OP_REMOVE_BRICK:
                        {
                                dict_t *dict = glusterd_op_get_ctx (op);
                                ret = dict_serialized_length (dict);
                                return ret;
                        }
                        break;
                       break;

                default:
                        GF_ASSERT (op);

        }

        return 0;
}

static int
glusterd_op_sm_inject_all_acc ()
{
        int32_t                 ret = -1;
        ret = glusterd_op_sm_inject_event (GD_OP_EVENT_ALL_ACC, NULL);
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);
        return ret;
}

int
glusterd_op_build_payload (glusterd_op_t op, gd1_mgmt_stage_op_req **req)
{
        int                     len = 0;
        int                     ret = -1;
        gd1_mgmt_stage_op_req   *stage_req = NULL;

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);
        GF_ASSERT (req);

        len = glusterd_op_get_len (op);

        stage_req = GF_CALLOC (1, sizeof (*stage_req),
                               gf_gld_mt_mop_stage_req_t);

        if (!stage_req) {
                gf_log ("", GF_LOG_ERROR, "Out of Memory");
                goto out;
        }


        glusterd_get_uuid (&stage_req->uuid);
        stage_req->op = op;
        //stage_req->buf.buf_len = len;

        switch (op) {
                case GD_OP_CREATE_VOLUME:
                        {
                                dict_t  *dict = NULL;
                                dict = glusterd_op_get_ctx (op);
                                GF_ASSERT (dict);
                                ++glusterfs_port;
                                ret = dict_set_int32 (dict, "port", glusterfs_port);
                                ret = dict_allocate_and_serialize (dict,
                                                &stage_req->buf.buf_val,
                                        (size_t *)&stage_req->buf.buf_len);
                                if (ret) {
                                        goto out;
                                }
                        }
                        break;

                case GD_OP_START_VOLUME:
                        {
                                glusterd_op_start_volume_ctx_t *ctx = NULL;
                                ctx = glusterd_op_get_ctx (op);
                                GF_ASSERT (ctx);
                                stage_req->buf.buf_len  =
                                        strlen (ctx->volume_name);
                                stage_req->buf.buf_val =
                                        gf_strdup (ctx->volume_name);
                        }
                        break;

                case GD_OP_STOP_VOLUME:
                        {
                                glusterd_op_stop_volume_ctx_t *ctx = NULL;
                                ctx = glusterd_op_get_ctx (op);
                                GF_ASSERT (ctx);
                                stage_req->buf.buf_len  =
                                        strlen (ctx->volume_name);
                                stage_req->buf.buf_val =
                                        gf_strdup (ctx->volume_name);
                        }
                        break;

                case GD_OP_DELETE_VOLUME:
                        {
                                glusterd_op_delete_volume_ctx_t *ctx = NULL;
                                ctx = glusterd_op_get_ctx (op);
                                GF_ASSERT (ctx);
                                stage_req->buf.buf_len  =
                                        strlen (ctx->volume_name);
                                stage_req->buf.buf_val =
                                        gf_strdup (ctx->volume_name);
                        }
                        break;

                case GD_OP_ADD_BRICK:
                        {
                                dict_t  *dict = NULL;
                                dict = glusterd_op_get_ctx (op);
                                GF_ASSERT (dict);
                                ret = dict_allocate_and_serialize (dict,
                                                &stage_req->buf.buf_val,
                                        (size_t *)&stage_req->buf.buf_len);
                                if (ret) {
                                        goto out;
                                }
                        }
                        break;

                case GD_OP_REPLACE_BRICK:
                        {
                                dict_t  *dict = NULL;
                                dict = glusterd_op_get_ctx (op);
                                GF_ASSERT (dict);
                                ret = dict_allocate_and_serialize (dict,
                                                &stage_req->buf.buf_val,
                                        (size_t *)&stage_req->buf.buf_len);
                                if (ret) {
                                        goto out;
                                }
                        }
                        break;

                case GD_OP_REMOVE_BRICK:
                        {
                                dict_t  *dict = NULL;
                                dict = glusterd_op_get_ctx (op);
                                GF_ASSERT (dict);
                                ret = dict_allocate_and_serialize (dict,
                                                &stage_req->buf.buf_val,
                                        (size_t *)&stage_req->buf.buf_len);
                                if (ret) {
                                        goto out;
                                }
                        }
                        break;

                default:
                        break;
        }

        *req = stage_req;
        ret = 0;

out:
        return ret;
}

static int
glusterd_volume_create_generate_volfiles (glusterd_volinfo_t *volinfo)
{
        int32_t         ret = -1;
        char            cmd_str[8192] = {0,};
        char            path[PATH_MAX] = {0,};
        glusterd_conf_t *priv = NULL;
        xlator_t        *this = NULL;
        char            bricks[8192] = {0,};
        glusterd_brickinfo_t    *brickinfo = NULL;
        int32_t         len = 0;

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;

        GF_ASSERT (priv);
        GF_ASSERT (volinfo);

        GLUSTERD_GET_VOLUME_DIR(path, volinfo, priv);
        if (!volinfo->port) {
                //volinfo->port = ++glusterfs_port;
        }

        list_for_each_entry (brickinfo, &volinfo->bricks, brick_list) {
                snprintf (bricks + len, 8192 - len, "%s:%s ",
                          brickinfo->hostname, brickinfo->path);
                len = strlen (bricks);
        }

        gf_log ("", GF_LOG_DEBUG, "Brick string: %s", bricks);

        switch (volinfo->type) {

                case GF_CLUSTER_TYPE_REPLICATE:
                {
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c %s -r 1 %s -p %d "
                                  "--num-replica %d",
                                   volinfo->volname, path, bricks,
                                   volinfo->port, volinfo->sub_count);
                        ret = system (cmd_str);
                        break;
                }

                case GF_CLUSTER_TYPE_STRIPE:
                {
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c %s -r 0 %s -p %d "
                                  "--num-stripe %d",
                                  volinfo->volname, path, bricks,
                                  volinfo->port, volinfo->sub_count);
                        ret = system (cmd_str);
                        break;
                }

                case GF_CLUSTER_TYPE_NONE:
                {
                        snprintf (cmd_str, 8192,
                                  "glusterfs-volgen -n %s -c %s %s -p %d",
                                  volinfo->volname, path, bricks,
                                  volinfo->port);
                        ret = system (cmd_str);
                        break;
                }

                default:
                        gf_log ("", GF_LOG_ERROR, "Unkown type: %d",
                                volinfo->type);
                        ret = -1;
        }
//out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);
        return ret;
}



static int
glusterd_op_stage_create_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        gf_boolean_t                            exists = _gf_false;

        GF_ASSERT (req);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        exists = glusterd_check_volume_exists (volname);

        if (exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name: %s exists",
                        volname);
                ret = -1;
        } else {
                ret = 0;
        }

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_start_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname [1024] = {0,};
        gf_boolean_t                            exists = _gf_false;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;

        GF_ASSERT (req);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);
        //volname = req->buf.buf_val;

        exists = glusterd_check_volume_exists (volname);

        if (!exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name %s does not exist",
                        volname);
                ret = -1;
        } else {
                ret = 0;
        }

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        list_for_each_entry (brickinfo, &volinfo->bricks, brick_list) {
                ret = glusterd_resolve_brick (brickinfo);
                if (ret) {
                        gf_log ("", GF_LOG_ERROR, "Unable to resolve brick"
                                " with hostname: %s, export: %s",
                                brickinfo->hostname,brickinfo->path);
                        goto out;
                }
        }

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_stop_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname[1024] = {0,};
        gf_boolean_t                            exists = _gf_false;
        glusterd_volinfo_t                      *volinfo = NULL;

        GF_ASSERT (req);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);

        exists = glusterd_check_volume_exists (volname);

        if (!exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name %s does not exist",
                        volname);
                ret = -1;
        } else {
                ret = 0;
        }

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        ret = glusterd_is_volume_started (volinfo);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Volume %s has not been started",
                        volname);
                goto out;
        }


out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_delete_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname [1024] = {0,};
        gf_boolean_t                            exists = _gf_false;
        glusterd_volinfo_t                      *volinfo = NULL;

        GF_ASSERT (req);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);

        exists = glusterd_check_volume_exists (volname);

        if (!exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name %s does not exist",
                        volname);
                ret = -1;
                goto out;
        } else {
                ret = 0;
        }

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        ret = glusterd_is_volume_started (volinfo);

        if (!ret) {
                gf_log ("", GF_LOG_ERROR, "Volume %s has been started."
                        "Volume needs to be stopped before deletion.",
                        volname);
                ret = -1;
                goto out;
        }

        ret = 0;

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_add_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        gf_boolean_t                            exists = _gf_false;

        GF_ASSERT (req);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        exists = glusterd_check_volume_exists (volname);

        if (!exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name: %s exists",
                        volname);
                ret = -1;
        } else {
                ret = 0;
        }

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_replace_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *src_brick = NULL;
        char                                    *dst_brick = NULL;

        GF_ASSERT (req);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }
        /* Need to do a little more error checking and validation */
        ret = dict_get_str (dict, "src-brick", &src_brick);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get src brick");
                goto out;
        }

        gf_log ("", GF_LOG_DEBUG,
                "src brick=%s", src_brick);

        ret = dict_get_str (dict, "dst-brick", &dst_brick);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get dest brick");
                goto out;
        }

        gf_log ("", GF_LOG_DEBUG,
                "dest brick=%s", dst_brick);

        ret = 0;

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_stage_remove_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        gf_boolean_t                            exists = _gf_false;

        GF_ASSERT (req);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        exists = glusterd_check_volume_exists (volname);

        if (!exists) {
                gf_log ("", GF_LOG_ERROR, "Volume with name: %s exists",
                        volname);
                ret = -1;
                goto out;
        } else {
                ret = 0;
        }

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}


static int
glusterd_op_create_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;
        xlator_t                                *this = NULL;
        char                                    *brick = NULL;
        int32_t                                 count = 0;
        int32_t                                 i = 1;
        char                                    *bricks    = NULL;
        char                                    *brick_list = NULL;
        char                                    *saveptr = NULL;
        int32_t                                 sub_count = 0;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = glusterd_volinfo_new (&volinfo);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        strncpy (volinfo->volname, volname, 1024);
        GF_ASSERT (volinfo->volname);

        ret = dict_get_int32 (dict, "type", &volinfo->type);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get type");
                goto out;
        }

        ret = dict_get_int32 (dict, "count", &volinfo->brick_count);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get count");
                goto out;
        }

        ret = dict_get_int32 (dict, "port", &volinfo->port);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get port");
                goto out;
        }

        count = volinfo->brick_count;

        ret = dict_get_str (dict, "bricks", &bricks);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get bricks");
                goto out;
        }

        if (GF_CLUSTER_TYPE_REPLICATE == volinfo->type) {
                ret = dict_get_int32 (dict, "replica-count",
                                      &sub_count);
                if (ret)
                        goto out;
        } else if (GF_CLUSTER_TYPE_STRIPE == volinfo->type) {
                ret = dict_get_int32 (dict, "stripe-count",
                                      &sub_count);
                if (ret)
                        goto out;
        }

        volinfo->sub_count = sub_count;


        if (bricks)
                brick_list = gf_strdup (bricks);

        if (count)
                brick = strtok_r (brick_list+1, " \n", &saveptr);

        while ( i <= count) {
                ret = glusterd_brickinfo_from_brick (brick, &brickinfo);
                if (ret)
                        goto out;

                list_add_tail (&brickinfo->brick_list, &volinfo->bricks);
                brick = strtok_r (NULL, " \n", &saveptr);
                i++;
        }
        list_add_tail (&volinfo->vol_list, &priv->volumes);

        ret = glusterd_store_create_volume (volinfo);

        if (ret)
                goto out;

        ret = glusterd_volume_create_generate_volfiles (volinfo);
        if (ret)
                goto out;


out:
        return ret;
}

static int
glusterd_op_add_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;
        xlator_t                                *this = NULL;
        char                                    *brick = NULL;
        int32_t                                 count = 0;
        int32_t                                 i = 1;
        char                                    *bricks    = NULL;
        char                                    *brick_list = NULL;
        char                                    *saveptr = NULL;
        gf_boolean_t                            glfs_started = _gf_false;
        int32_t                                 mybrick = 0;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        ret = glusterd_volinfo_find (volname, &volinfo);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                goto out;
        }


        ret = dict_get_int32 (dict, "count", &count);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get count");
                goto out;
        }

        volinfo->brick_count += count;

        ret = dict_get_str (dict, "bricks", &bricks);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get bricks");
                goto out;
        }

        if (bricks)
                brick_list = gf_strdup (bricks);

        if (count)
                brick = strtok_r (brick_list+1, " \n", &saveptr);

        while ( i <= count) {
                ret = glusterd_brickinfo_from_brick (brick, &brickinfo);
                if (ret)
                        goto out;

                list_add_tail (&brickinfo->brick_list, &volinfo->bricks);
                ret = glusterd_resolve_brick (brickinfo);

                if ((!uuid_compare (brickinfo->uuid, priv->uuid)) &&
                                (GLUSTERD_STATUS_STARTED == volinfo->status)) {
                        ret =
                          glusterd_volume_create_generate_volfiles (volinfo);
                        if (ret)
                                goto out;

                        gf_log ("", GF_LOG_NORMAL, "About to start glusterfs"
                                " for brick %s:%s", brickinfo->hostname,
                                brickinfo->path);
                        ret = glusterd_volume_start_glusterfs
                                                (volinfo, brickinfo, mybrick);
                        if (ret) {
                                gf_log ("", GF_LOG_ERROR, "Unable to start "
                                        "glusterfs, ret: %d", ret);
                                goto out;
                        }
                        glfs_started = _gf_true;
                        mybrick++;
                }

                brick = strtok_r (NULL, " \n", &saveptr);
                i++;
        }

        if (!glfs_started) {
                ret = glusterd_volume_create_generate_volfiles (volinfo);
                if (ret)
                        goto out;
        }

        ret = glusterd_store_update_volume (volinfo);

        if (ret)
                goto out;



out:
        return ret;
}

static int
replace_brick_start_dst_brick (glusterd_volinfo_t *volinfo, glusterd_brickinfo_t *dst_brickinfo)
{        glusterd_conf_t    *priv = NULL;
        char  cmd_str[8192] = {0,};
        char filename[PATH_MAX];
        FILE *file = NULL;
        int ret;

        priv = THIS->private;

        snprintf (filename, PATH_MAX, "%s/vols/%s/replace_dst_brick.vol",
                  priv->workdir, volinfo->volname);


        file = fopen (filename, "a+");
        if (!file) {
                gf_log ("", GF_LOG_DEBUG,
                        "Open of volfile failed");
                return -1;
        }

        ret = truncate (filename, 0);

        fprintf (file, "volume src-posix\n");
        fprintf (file, "type storage/posix\n");
        fprintf (file, "option directory %s\n",
                 dst_brickinfo->path);
        fprintf (file, "end-volume\n");
        fprintf (file, "volume %s\n",
                 dst_brickinfo->path);
        fprintf (file, "type features/locks\n");
        fprintf (file, "subvolumes src-posix\n");
        fprintf (file, "end-volume\n");
        fprintf (file, "volume src-server\n");
        fprintf (file, "type protocol/server\n");
        fprintf (file, "option auth.addr.%s.allow *\n",
                 dst_brickinfo->path);
        fprintf (file, "option listen-port 34034\n");
        fprintf (file, "subvolumes %s\n",
                 dst_brickinfo->path);
        fprintf (file, "end-volume\n");

        gf_log ("", GF_LOG_DEBUG,
                "starting dst brick");

        snprintf (cmd_str, 4096, "glusterfs -f %s/vols/%s/replace_dst_brick.vol",
                  priv->workdir, volinfo->volname);

        ret = system (cmd_str);


        fclose (file);

        return 0;
}

static int
replace_brick_spawn_brick (glusterd_volinfo_t *volinfo, dict_t *dict,
                           glusterd_brickinfo_t *dst_brickinfo)

{

        replace_brick_start_dst_brick (volinfo, dst_brickinfo);

        return 0;
}

static int
replace_brick_generate_volfile (glusterd_volinfo_t *volinfo,
                                glusterd_brickinfo_t *src_brickinfo)
{
        glusterd_conf_t    *priv = NULL;
        FILE *file = NULL;
        char filename[PATH_MAX];
        int ret;

        priv = THIS->private;

        gf_log ("", GF_LOG_DEBUG,
                "Creating volfile");

        snprintf (filename, PATH_MAX, "%s/vols/%s/replace_brick.vol",
                  priv->workdir, volinfo->volname);

        file = fopen (filename, "a+");
        if (!file) {
                gf_log ("", GF_LOG_DEBUG,
                        "Open of volfile failed");
                return -1;
        }

        ret = truncate (filename, 0);
        ret = unlink ("/tmp/replace_brick.vol");

        fprintf (file, "volume client/protocol\n");
        fprintf (file, "type protocol/client\n");
        fprintf (file, "option remote-host %s\n",
                 src_brickinfo->hostname);
        fprintf (file, "option remote-subvolume %s\n",
                 src_brickinfo->path);
        fprintf (file, "option remote-port 34034\n");
        fprintf (file, "echo end-volume\n");

        ret = symlink(filename, "/tmp/replace_brick.vol");
        if (!ret) {
                gf_log ("", GF_LOG_DEBUG,
                        "symlink call failed");
                return -1;
        }

        fclose (file);

        return 0;
}

static int
replace_brick_start_source_brick (glusterd_volinfo_t *volinfo, glusterd_brickinfo_t *src_brickinfo,
                                  glusterd_brickinfo_t *dst_brickinfo)
{        glusterd_conf_t    *priv = NULL;
        char filename[PATH_MAX];
        FILE *file = NULL;
        char  cmd_str[8192] = {0,};
        int ret;

        priv = THIS->private;

        gf_log ("", GF_LOG_DEBUG,
                "Creating volfile");

        snprintf (filename, PATH_MAX, "%s/vols/%s/replace_source_brick.vol",
                  priv->workdir, volinfo->volname);

        file = fopen (filename, "a+");
        if (!file) {
                gf_log ("", GF_LOG_DEBUG,
                        "Open of volfile failed");
                return -1;
        }

        ret = truncate (filename, 0);

        fprintf (file, "volume src-posix\n");
        fprintf (file, "type storage/posix\n");
        fprintf (file, "option directory %s\n",
                 src_brickinfo->path);
        fprintf (file, "end-volume\n");
        fprintf (file, "volume locks\n");
        fprintf (file, "type features/locks\n");
        fprintf (file, "subvolumes src-posix\n");
        fprintf (file, "end-volume\n");
        fprintf (file, "volume remote-client\n");
        fprintf (file, "type protocol/client\n");
        fprintf (file, "option remote-host %s\n",
                 dst_brickinfo->hostname);
        fprintf (file, "option remote-port 34034\n");
        fprintf (file, "option remote-subvolume %s\n",
                 dst_brickinfo->path);
        fprintf (file, "end-volume\n");
        fprintf (file, "volume %s\n",
                 src_brickinfo->path);
        fprintf (file, "type cluster/pump\n");
        fprintf (file, "subvolumes locks remote-client\n");
        fprintf (file, "end-volume\n");
        fprintf (file, "volume src-server\n");
        fprintf (file, "type protocol/server\n");
        fprintf (file, "option auth.addr.%s.allow *\n",
                 src_brickinfo->path);
        fprintf (file, "option listen-port 34034\n");
        fprintf (file, "subvolumes %s\n",
                 src_brickinfo->path);
        fprintf (file, "end-volume\n");


        gf_log ("", GF_LOG_DEBUG,
                "starting source brick");

        snprintf (cmd_str, 4096, "glusterfs -f %s/vols/%s/replace_source_brick.vol -l /tmp/b_log -LTRACE",
                  priv->workdir, volinfo->volname);

        ret = system (cmd_str);

        fclose (file);

        return 0;
}

static int
replace_brick_mount (glusterd_volinfo_t *volinfo, glusterd_brickinfo_t *src_brickinfo,
                     glusterd_brickinfo_t *dst_brickinfo, gf1_cli_replace_op op)
{

        gf_log ("", GF_LOG_DEBUG,
                "starting source brick");

        replace_brick_start_source_brick (volinfo, src_brickinfo,
                                          dst_brickinfo);

        return 0;
}

static int
glusterd_op_replace_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        char                                    *volname = NULL;
        xlator_t                                *this = NULL;
        glusterd_conf_t                         *priv = NULL;
        char                                    *src_brick = NULL;
        char                                    *dst_brick = NULL;
        char                                    *str = NULL;

        glusterd_brickinfo_t                    *src_brickinfo = NULL;
        glusterd_brickinfo_t                    *dst_brickinfo = NULL;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        /* Need to do a little more error checking and validation */
        ret = dict_get_str (dict, "src-brick", &src_brick);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get src brick");
                goto out;
        }

        gf_log (this->name, GF_LOG_DEBUG,
                "src brick=%s", src_brick);

        ret = dict_get_str (dict, "dst-brick", &dst_brick);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get dest brick");
                goto out;
        }

        gf_log (this->name, GF_LOG_DEBUG,
                "dst brick=%s", dst_brick);

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        ret = glusterd_volinfo_find (volname, &volinfo);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                goto out;
        }

        str = strdup (src_brick);

        ret = glusterd_brickinfo_get (str, volinfo,
                                      &src_brickinfo);
        if (ret) {
                gf_log ("", GF_LOG_DEBUG, "Unable to get src-brickinfo");
                goto out;
        }

        ret = glusterd_brickinfo_from_brick (dst_brick, &dst_brickinfo);
        if (ret) {
                gf_log ("", GF_LOG_DEBUG, "Unable to get dst-brickinfo");
                goto out;
        }

        replace_brick_generate_volfile (volinfo, src_brickinfo);

        if (!glusterd_is_local_addr (src_brickinfo->hostname)) {
                gf_log ("", GF_LOG_NORMAL,
                        "I AM THE SOURCE HOST");
                replace_brick_mount (volinfo, src_brickinfo, dst_brickinfo,
                                     req->op);
        } else if (!glusterd_is_local_addr (dst_brickinfo->hostname)) {
                gf_log ("", GF_LOG_NORMAL,
                        "I AM THE DESTINATION HOST");
                replace_brick_spawn_brick (volinfo, dict, dst_brickinfo);
        }

        ret = 0;

out:
        return ret;
}

static int
glusterd_op_remove_brick (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        dict_t                                  *dict = NULL;
        char                                    *volname = NULL;
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;
        xlator_t                                *this = NULL;
        char                                    *brick = NULL;
        int32_t                                 count = 0;
        int32_t                                 i = 1;
        gf_boolean_t                            glfs_stopped = _gf_false;
        int32_t                                 mybrick = 0;
        char                                    key[256] = {0,};
        char                                    *dup_brick = NULL;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        dict = dict_new ();
        if (!dict)
                goto out;

        ret = dict_unserialize (req->buf.buf_val, req->buf.buf_len, &dict);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to unserialize dict");
                goto out;
        }

        ret = dict_get_str (dict, "volname", &volname);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get volume name");
                goto out;
        }

        ret = glusterd_volinfo_find (volname, &volinfo);

        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to allocate memory");
                goto out;
        }


        ret = dict_get_int32 (dict, "count", &count);
        if (ret) {
                gf_log ("", GF_LOG_ERROR, "Unable to get count");
                goto out;
        }


        while ( i <= count) {
                snprintf (key, 256, "brick%d", i);
                ret = dict_get_str (dict, key, &brick);
                if (ret) {
                        gf_log ("", GF_LOG_ERROR, "Unable to get %s", key);
                        goto out;
                }
                dup_brick = gf_strdup (brick);

                ret = glusterd_brickinfo_get (dup_brick, volinfo,  &brickinfo);
                if (ret)
                        goto out;


                ret = glusterd_resolve_brick (brickinfo);

                if (ret)
                        goto out;

                if ((!uuid_compare (brickinfo->uuid, priv->uuid)) &&
                    (GLUSTERD_STATUS_STARTED == volinfo->status)) {
                        ret =
                          glusterd_volume_create_generate_volfiles (volinfo);
                        if (ret)
                                goto out;

                        gf_log ("", GF_LOG_NORMAL, "About to stop glusterfs"
                                " for brick %s:%s", brickinfo->hostname,
                                brickinfo->path);
                        ret = glusterd_volume_stop_glusterfs
                                                (volinfo, brickinfo, mybrick);
                        if (ret) {
                                gf_log ("", GF_LOG_ERROR, "Unable to start "
                                        "glusterfs, ret: %d", ret);
                                goto out;
                        }
                        glfs_stopped = _gf_true;
                        mybrick++;
                }

                glusterd_brickinfo_delete (brickinfo);
                volinfo->brick_count--;

                i++;
        }

        if (!glfs_stopped) {
                ret = glusterd_volume_create_generate_volfiles (volinfo);
                if (ret)
                        goto out;
        }

        ret = glusterd_store_update_volume (volinfo);

        if (ret)
                goto out;



out:
        return ret;
}


static int
glusterd_op_delete_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname[1024] = {0,};
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        xlator_t                                *this = NULL;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        ret = glusterd_store_delete_volume (volinfo);

        if (ret)
                goto out;

        ret = glusterd_volinfo_delete (volinfo);

        if (ret)
                goto out;

out:
        return ret;
}

static int
glusterd_op_start_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname[1024] = {0,};
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;
        xlator_t                                *this = NULL;
        int32_t                                 mybrick = 0;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        list_for_each_entry (brickinfo, &volinfo->bricks, brick_list) {
                if (!uuid_compare (brickinfo->uuid, priv->uuid)) {
                        gf_log ("", GF_LOG_NORMAL, "About to start glusterfs"
                                " for brick %s:%s", brickinfo->hostname,
                                brickinfo->path);
                        ret = glusterd_volume_start_glusterfs
                                                (volinfo, brickinfo, mybrick);
                        if (ret) {
                                gf_log ("", GF_LOG_ERROR, "Unable to start "
                                        "glusterfs, ret: %d", ret);
                                goto out;
                        }
                        mybrick++;
                }
        }

        glusterd_set_volume_status (volinfo, GLUSTERD_STATUS_STARTED);

out:
        return ret;
}

static int
glusterd_op_stop_volume (gd1_mgmt_stage_op_req *req)
{
        int                                     ret = 0;
        char                                    volname[1024] = {0,};
        glusterd_conf_t                         *priv = NULL;
        glusterd_volinfo_t                      *volinfo = NULL;
        glusterd_brickinfo_t                    *brickinfo = NULL;
        xlator_t                                *this = NULL;
        int32_t                                 mybrick = 0;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);

        strncpy (volname, req->buf.buf_val, req->buf.buf_len);

        ret  = glusterd_volinfo_find (volname, &volinfo);

        if (ret)
                goto out;

        list_for_each_entry (brickinfo, &volinfo->bricks, brick_list) {
                if (!uuid_compare (brickinfo->uuid, priv->uuid)) {
                        gf_log ("", GF_LOG_NORMAL, "About to stop glusterfs"
                                " for brick %s:%s", brickinfo->hostname,
                                brickinfo->path);
                        ret = glusterd_volume_stop_glusterfs
                                (volinfo, brickinfo, mybrick);
                        if (ret) {
                                gf_log ("", GF_LOG_ERROR, "Unable to stop "
                                        "glusterfs, ret: %d", ret);
                                goto out;
                        }
                        mybrick++;
                }
        }

        glusterd_set_volume_status (volinfo, GLUSTERD_STATUS_STOPPED);

out:
        return ret;
}

static int
glusterd_op_ac_none (glusterd_op_sm_event_t *event, void *ctx)
{
        int ret = 0;

        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}

static int
glusterd_op_ac_send_lock (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;
        rpc_clnt_procedure_t    *proc = NULL;
        glusterd_conf_t         *priv = NULL;
        xlator_t                *this = NULL;

        this = THIS;
        priv = this->private;

        proc = &priv->mgmt->proctable[GD_MGMT_CLUSTER_LOCK];
        if (proc->fn) {
                ret = proc->fn (NULL, this, NULL);
                if (ret)
                        goto out;
        }

        if (!opinfo.pending_count)
                ret = glusterd_op_sm_inject_all_acc ();

out:
        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}

static int
glusterd_op_ac_send_unlock (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;
        rpc_clnt_procedure_t    *proc = NULL;
        glusterd_conf_t         *priv = NULL;
        xlator_t                *this = NULL;

        this = THIS;
        priv = this->private;

        /*ret = glusterd_unlock (priv->uuid);

        if (ret)
                goto out;
        */

        proc = &priv->mgmt->proctable[GD_MGMT_CLUSTER_UNLOCK];
        if (proc->fn) {
                ret = proc->fn (NULL, this, NULL);
                if (ret)
                        goto out;
        }

        if (!opinfo.pending_count)
                ret = glusterd_op_sm_inject_all_acc ();

out:
        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;

}

static int
glusterd_op_ac_lock (glusterd_op_sm_event_t *event, void *ctx)
{
        int                      ret = 0;
        glusterd_op_lock_ctx_t   *lock_ctx = NULL;
        int32_t                  status = 0;


        GF_ASSERT (event);
        GF_ASSERT (ctx);

        lock_ctx = (glusterd_op_lock_ctx_t *)ctx;

        status = glusterd_lock (lock_ctx->uuid);

        gf_log ("", GF_LOG_DEBUG, "Lock Returned %d", status);

        ret = glusterd_op_lock_send_resp (lock_ctx->req, status);

        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}

static int
glusterd_op_ac_unlock (glusterd_op_sm_event_t *event, void *ctx)
{
        int ret = 0;
        glusterd_op_lock_ctx_t   *lock_ctx = NULL;

        GF_ASSERT (event);
        GF_ASSERT (ctx);

        lock_ctx = (glusterd_op_lock_ctx_t *)ctx;

        ret = glusterd_unlock (lock_ctx->uuid);

        gf_log ("", GF_LOG_DEBUG, "Unlock Returned %d", ret);

        ret = glusterd_op_unlock_send_resp (lock_ctx->req, ret);

        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}

static int
glusterd_op_ac_rcvd_lock_acc (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        GF_ASSERT (event);

        opinfo.pending_count--;

        if (opinfo.pending_count)
                goto out;

        ret = glusterd_op_sm_inject_event (GD_OP_EVENT_ALL_ACC, NULL);

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

out:
        return ret;
}

static int
glusterd_op_ac_send_stage_op (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;
        rpc_clnt_procedure_t    *proc = NULL;
        glusterd_conf_t         *priv = NULL;
        xlator_t                *this = NULL;

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);
        GF_ASSERT (priv->mgmt);

        proc = &priv->mgmt->proctable[GD_MGMT_STAGE_OP];
        GF_ASSERT (proc);
        if (proc->fn) {
                ret = proc->fn (NULL, this, NULL);
                if (ret)
                        goto out;
        }

        if (!opinfo.pending_count)
                ret = glusterd_op_sm_inject_all_acc ();

out:
        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;

}

static int
glusterd_op_ac_send_commit_op (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;
        rpc_clnt_procedure_t    *proc = NULL;
        glusterd_conf_t         *priv = NULL;
        xlator_t                *this = NULL;

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);
        GF_ASSERT (priv->mgmt);

        proc = &priv->mgmt->proctable[GD_MGMT_COMMIT_OP];
        GF_ASSERT (proc);
        if (proc->fn) {
                ret = proc->fn (NULL, this, NULL);
                if (!ret)
                        goto out;
        }

        if (!opinfo.pending_count)
                ret = glusterd_op_sm_inject_all_acc ();

out:
        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;

}

static int
glusterd_op_ac_rcvd_stage_op_acc (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        GF_ASSERT (event);

        opinfo.pending_count--;

        if (opinfo.pending_count)
                goto out;

        ret = glusterd_op_sm_inject_event (GD_OP_EVENT_STAGE_ACC, NULL);

out:
        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

static int
glusterd_op_ac_rcvd_commit_op_acc (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        GF_ASSERT (event);

        opinfo.pending_count--;

        if (opinfo.pending_count)
                goto out;

        ret = glusterd_op_sm_inject_event (GD_OP_EVENT_COMMIT_ACC, NULL);

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

out:
        return ret;
}

static int
glusterd_op_ac_rcvd_unlock_acc (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        GF_ASSERT (event);

        opinfo.pending_count--;

        if (opinfo.pending_count)
                goto out;

        ret = glusterd_op_sm_inject_event (GD_OP_EVENT_ALL_ACC, NULL);

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

out:
        return ret;
}


int32_t
glusterd_op_send_cli_response (int32_t op, int32_t op_ret,
                               int32_t op_errno, rpcsvc_request_t *req)
{
        int32_t         ret = -1;
        gd_serialize_t  sfunc = NULL;
        void            *cli_rsp = NULL;

        switch (op) {
                case GD_MGMT_CLI_CREATE_VOLUME:
                        {
                                gf1_cli_create_vol_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_create_vol_rsp;
                                break;
                        }

                case GD_MGMT_CLI_START_VOLUME:
                        {
                                gf1_cli_start_vol_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_start_vol_rsp;
                                break;
                        }

                case GD_MGMT_CLI_STOP_VOLUME:
                        {
                                gf1_cli_stop_vol_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_stop_vol_rsp;
                                break;
                        }

                case GD_MGMT_CLI_DELETE_VOLUME:
                        {
                                gf1_cli_delete_vol_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_delete_vol_rsp;
                                break;
                        }

                case GD_MGMT_CLI_DEFRAG_VOLUME:
                        {
                                gf1_cli_defrag_vol_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                //rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_defrag_vol_rsp;
                                break;
                        }

                case GD_MGMT_CLI_ADD_BRICK:
                        {
                                gf1_cli_add_brick_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_add_brick_rsp;
                                break;
                        }

                case GD_MGMT_CLI_REMOVE_BRICK:
                        {
                                gf1_cli_remove_brick_rsp rsp = {0,};
                                rsp.op_ret = op_ret;
                                rsp.op_errno = op_errno;
                                rsp.volname = "";
                                cli_rsp = &rsp;
                                sfunc = gf_xdr_serialize_cli_remove_brick_rsp;
                                break;
                        }
        }


        ret = glusterd_submit_reply (req, cli_rsp, NULL, 0, NULL,
                                     sfunc);

        if (ret)
                goto out;

out:
        pthread_mutex_unlock (&opinfo.lock);
        gf_log ("", GF_LOG_NORMAL, "Returning %d", ret);
        return ret;
}


int32_t
glusterd_op_txn_complete ()
{
        int32_t                 ret = -1;
        glusterd_conf_t         *priv = NULL;
        int32_t                 op = -1;

        priv = THIS->private;
        GF_ASSERT (priv);

        ret = glusterd_unlock (priv->uuid);

        if (ret) {
                gf_log ("glusterd", GF_LOG_CRITICAL,
                        "Unable to clear local lock, ret: %d", ret);
                goto out;
        }

        gf_log ("glusterd", GF_LOG_NORMAL, "Cleared local lock");

        ret = glusterd_op_send_cli_response (opinfo.cli_op, opinfo.op_ret,
                                             opinfo.op_errno, opinfo.req);

        opinfo.op_ret = 0;
        opinfo.op_errno = 0;

        op = glusterd_op_get_op ();

        if (op != -1) {
                glusterd_op_clear_pending_op (op);
                glusterd_op_clear_commit_op (op);
                glusterd_op_clear_op (op);
                glusterd_op_clear_ctx (op);
        }

out:
        pthread_mutex_unlock (&opinfo.lock);
        gf_log ("glusterd", GF_LOG_NORMAL, "Returning %d", ret);
        return ret;
}

static int
glusterd_op_ac_unlocked_all (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        GF_ASSERT (event);

        ret = glusterd_op_txn_complete ();

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}


static int
glusterd_op_ac_commit_error (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = 0;

        //Log here with who failed the commit
        //
        return ret;
}

static int
glusterd_op_ac_stage_op (glusterd_op_sm_event_t *event, void *ctx)
{
        int                     ret = -1;
        gd1_mgmt_stage_op_req   *req = NULL;
        glusterd_op_stage_ctx_t *stage_ctx = NULL;
        int32_t                 status = 0;

        GF_ASSERT (ctx);

        stage_ctx = ctx;

        req = &stage_ctx->stage_req;

        status = glusterd_op_stage_validate (req);

        if (status) {
                gf_log ("", GF_LOG_ERROR, "Validate failed: %d", status);
        }

        ret = glusterd_op_stage_send_resp (stage_ctx->req, req->op, status);

        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}

static int
glusterd_op_ac_commit_op (glusterd_op_sm_event_t *event, void *ctx)
{
        int                             ret = 0;
        gd1_mgmt_stage_op_req           *req = NULL;
        glusterd_op_commit_ctx_t        *commit_ctx = NULL;
        int32_t                         status = 0;

        GF_ASSERT (ctx);

        commit_ctx = ctx;

        req = &commit_ctx->stage_req;

        status = glusterd_op_commit_perform (req);

        if (status) {
                gf_log ("", GF_LOG_ERROR, "Commit failed: %d", status);
        }

        ret = glusterd_op_commit_send_resp (commit_ctx->req, req->op, status);

        gf_log ("", GF_LOG_DEBUG, "Returning with %d", ret);

        return ret;
}


static int
glusterd_op_sm_transition_state (glusterd_op_info_t *opinfo,
                                 glusterd_op_sm_t *state,
                                 glusterd_op_sm_event_type_t event_type)
{

        GF_ASSERT (state);
        GF_ASSERT (opinfo);

        gf_log ("", GF_LOG_NORMAL, "Transitioning from %d to %d",
                     opinfo->state.state, state[event_type].next_state);
        opinfo->state.state =
                state[event_type].next_state;
        return 0;
}

int32_t
glusterd_op_stage_validate (gd1_mgmt_stage_op_req *req)
{
        int     ret = -1;

        GF_ASSERT (req);

        switch (req->op) {
                case GD_OP_CREATE_VOLUME:
                        ret = glusterd_op_stage_create_volume (req);
                        break;

                case GD_OP_START_VOLUME:
                        ret = glusterd_op_stage_start_volume (req);
                        break;

                case GD_OP_STOP_VOLUME:
                        ret = glusterd_op_stage_stop_volume (req);
                        break;

                case GD_OP_DELETE_VOLUME:
                        ret = glusterd_op_stage_delete_volume (req);
                        break;

                case GD_OP_ADD_BRICK:
                        ret = glusterd_op_stage_add_brick (req);
                        break;

                case GD_OP_REPLACE_BRICK:
                        ret = glusterd_op_stage_replace_brick (req);
                        break;


                case GD_OP_REMOVE_BRICK:
                        ret = glusterd_op_stage_remove_brick (req);
                        break;

                default:
                        gf_log ("", GF_LOG_ERROR, "Unknown op %d",
                                req->op);
        }

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}


int32_t
glusterd_op_commit_perform (gd1_mgmt_stage_op_req *req)
{
        int     ret = -1;

        GF_ASSERT (req);

        switch (req->op) {
                case GD_OP_CREATE_VOLUME:
                        ret = glusterd_op_create_volume (req);
                        break;

                case GD_OP_START_VOLUME:
                        ret = glusterd_op_start_volume (req);
                        break;

                case GD_OP_STOP_VOLUME:
                        ret = glusterd_op_stop_volume (req);
                        break;

                case GD_OP_DELETE_VOLUME:
                        ret = glusterd_op_delete_volume (req);
                        break;

                case GD_OP_ADD_BRICK:
                        ret = glusterd_op_add_brick (req);
                        break;

                case GD_OP_REPLACE_BRICK:
                        ret = glusterd_op_replace_brick (req);
                        break;

                case GD_OP_REMOVE_BRICK:
                        ret = glusterd_op_remove_brick (req);
                        break;

                default:
                        gf_log ("", GF_LOG_ERROR, "Unknown op %d",
                                req->op);
        }

        gf_log ("", GF_LOG_DEBUG, "Returning %d", ret);

        return ret;
}

glusterd_op_sm_t glusterd_op_state_default [] = {
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_send_lock},//EVENT_START_LOCK
        {GD_OP_STATE_LOCKED, glusterd_op_ac_lock}, //EVENT_LOCK
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_RCVD_ACC
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_ALL_ACC
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_RCVD_RJT
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_lock_sent [] = {
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_rcvd_lock_acc}, //EVENT_RCVD_ACC
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_send_stage_op}, //EVENT_ALL_ACC
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_send_unlock}, //EVENT_RCVD_RJT
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_LOCK_SENT, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_locked [] = {
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_RCVD_ACC
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_ALL_ACC
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_RCVD_RJT
        {GD_OP_STATE_STAGED, glusterd_op_ac_stage_op}, //EVENT_STAGE_OP
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_LOCKED, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_stage_op_sent [] = {
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_rcvd_stage_op_acc}, //EVENT_RCVD_ACC
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_send_stage_op}, //EVENT_ALL_ACC
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_send_commit_op}, //EVENT_STAGE_ACC
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_UNLOCK_SENT,   glusterd_op_ac_send_unlock}, //EVENT_RCVD_RJT
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_STAGE_OP_SENT, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_staged [] = {
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_STAGED, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_RCVD_ACC
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_ALL_ACC
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_RCVD_RJT
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_COMMITED, glusterd_op_ac_commit_op}, //EVENT_COMMIT_OP
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_STAGED, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_commit_op_sent [] = {
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_rcvd_commit_op_acc}, //EVENT_RCVD_ACC
        {GD_OP_STATE_UNLOCK_SENT,    glusterd_op_ac_send_unlock}, //EVENT_ALL_ACC
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_UNLOCK_SENT,    glusterd_op_ac_send_unlock}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_commit_error}, //EVENT_RCVD_RJT
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_COMMIT_OP_SENT, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_commited [] = {
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_RCVD_ACC
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_ALL_ACC
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_RCVD_RJT
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_COMMITED, glusterd_op_ac_none}, //EVENT_MAX
};

glusterd_op_sm_t glusterd_op_state_unlock_sent [] = {
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_NONE
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none},//EVENT_START_LOCK
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_LOCK
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_rcvd_unlock_acc}, //EVENT_RCVD_ACC
        {GD_OP_STATE_DEFAULT, glusterd_op_ac_unlocked_all}, //EVENT_ALL_ACC
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_STAGE_ACC
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_ACC
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_RCVD_RJT
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_STAGE_OP
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_COMMIT_OP
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_unlock}, //EVENT_UNLOCK
        {GD_OP_STATE_UNLOCK_SENT, glusterd_op_ac_none}, //EVENT_MAX
};


glusterd_op_sm_t *glusterd_op_state_table [] = {
        glusterd_op_state_default,
        glusterd_op_state_lock_sent,
        glusterd_op_state_locked,
        glusterd_op_state_stage_op_sent,
        glusterd_op_state_staged,
        glusterd_op_state_commit_op_sent,
        glusterd_op_state_commited,
        glusterd_op_state_unlock_sent
};

int
glusterd_op_sm_new_event (glusterd_op_sm_event_type_t event_type,
                          glusterd_op_sm_event_t **new_event)
{
        glusterd_op_sm_event_t      *event = NULL;

        GF_ASSERT (new_event);
        GF_ASSERT (GD_OP_EVENT_NONE <= event_type &&
                        GD_OP_EVENT_MAX > event_type);

        event = GF_CALLOC (1, sizeof (*event), gf_gld_mt_op_sm_event_t);

        if (!event)
                return -1;

        *new_event = event;
        event->event = event_type;
        INIT_LIST_HEAD (&event->list);

        return 0;
}

int
glusterd_op_sm_inject_event (glusterd_op_sm_event_type_t event_type,
                             void *ctx)
{
        int32_t                 ret = -1;
        glusterd_op_sm_event_t  *event = NULL;

        GF_ASSERT (event_type < GD_OP_EVENT_MAX &&
                        event_type >= GD_OP_EVENT_NONE);

        ret = glusterd_op_sm_new_event (event_type, &event);

        if (ret)
                goto out;

        event->ctx = ctx;

        gf_log ("glusterd", GF_LOG_NORMAL, "Enqueuing event: %d",
                        event->event);
        list_add_tail (&event->list, &gd_op_sm_queue);

out:
        return ret;
}


int
glusterd_op_sm ()
{
        glusterd_op_sm_event_t          *event = NULL;
        glusterd_op_sm_event_t          *tmp = NULL;
        int                             ret = -1;
        glusterd_op_sm_ac_fn            handler = NULL;
        glusterd_op_sm_t                *state = NULL;
        glusterd_op_sm_event_type_t     event_type = GD_OP_EVENT_NONE;


        while (!list_empty (&gd_op_sm_queue)) {

                list_for_each_entry_safe (event, tmp, &gd_op_sm_queue, list) {

                        list_del_init (&event->list);
                        event_type = event->event;

                        state = glusterd_op_state_table[opinfo.state.state];

                        GF_ASSERT (state);

                        handler = state[event_type].handler;
                        GF_ASSERT (handler);

                        ret = handler (event, event->ctx);

                        if (ret) {
                                gf_log ("glusterd", GF_LOG_ERROR,
                                        "handler returned: %d", ret);
                                GF_FREE (event);
                                continue;
                        }

                        ret = glusterd_op_sm_transition_state (&opinfo, state,
                                                                event_type);

                        if (ret) {
                                gf_log ("glusterd", GF_LOG_ERROR,
                                        "Unable to transition"
                                        "state from %d to %d",
                                         opinfo.state.state,
                                         state[event_type].next_state);
                                return ret;
                        }

                        GF_FREE (event);
                }
        }


        ret = 0;

        return ret;
}

int32_t
glusterd_op_set_op (glusterd_op_t op)
{

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        opinfo.op[op] = 1;
        opinfo.pending_op[op] = 1;
        opinfo.commit_op[op] = 1;

        return 0;

}

int32_t
glusterd_op_get_op ()
{

        int     i = 0;
        int32_t ret = 0;

        for ( i = 0; i < GD_OP_MAX; i++) {
                if (opinfo.op[i])
                        break;
        }

        if ( i == GD_OP_MAX)
                ret = -1;
        else
                ret = i;

        return ret;

}


int32_t
glusterd_op_set_cli_op (gf_mgmt_procnum op)
{

        int32_t         ret;

        ret = pthread_mutex_trylock (&opinfo.lock);

        if (ret)
                goto out;

        opinfo.cli_op = op;

out:
        gf_log ("", GF_LOG_NORMAL, "Returning %d", ret);
        return ret;
}

int32_t
glusterd_op_set_req (rpcsvc_request_t *req)
{

        GF_ASSERT (req);
        opinfo.req = req;
        return 0;
}

int32_t
glusterd_op_clear_pending_op (glusterd_op_t op)
{

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        opinfo.pending_op[op] = 0;

        return 0;

}

int32_t
glusterd_op_clear_commit_op (glusterd_op_t op)
{

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        opinfo.commit_op[op] = 0;

        return 0;

}

int32_t
glusterd_op_clear_op (glusterd_op_t op)
{

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        opinfo.op[op] = 0;

        return 0;

}

int32_t
glusterd_op_set_ctx (glusterd_op_t op, void *ctx)
{

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        opinfo.op_ctx[op] = ctx;

        return 0;

}

int32_t
glusterd_op_clear_ctx (glusterd_op_t op)
{

        void    *ctx = NULL;

        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        ctx = opinfo.op_ctx[op];

        opinfo.op_ctx[op] = NULL;

        //Cleanup to be done here
        return 0;

}

void *
glusterd_op_get_ctx (glusterd_op_t op)
{
        GF_ASSERT (op < GD_OP_MAX);
        GF_ASSERT (op > GD_OP_NONE);

        return opinfo.op_ctx[op];

}

int
glusterd_op_sm_init ()
{
        INIT_LIST_HEAD (&gd_op_sm_queue);
        return 0;
}
