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

/* libglusterfs/src/defaults.h:
       This file contains definition of default fops and mops functions.
*/

#ifndef _DEFAULTS_H
#define _DEFAULTS_H

#ifndef _CONFIG_H
#define _CONFIG_H
#include "config.h"
#endif

#include "xlator.h"

/* Management Operations */

int32_t default_getspec (call_frame_t *frame,
			 xlator_t *this,
			 const char *key,
			 int32_t flag);

int32_t default_checksum (call_frame_t *frame,
			  xlator_t *this,
			  loc_t *loc,
			  int32_t flag);

int32_t default_rchecksum (call_frame_t *frame,
                           xlator_t *this,
                           fd_t *fd, off_t offset,
                           int32_t len);

/* FileSystem operations */
int32_t default_lookup (call_frame_t *frame,
			xlator_t *this,
			loc_t *loc,
			dict_t *xattr_req);

int32_t default_stat (call_frame_t *frame,
		      xlator_t *this,
		      loc_t *loc);

int32_t default_fstat (call_frame_t *frame,
		       xlator_t *this,
		       fd_t *fd);

int32_t default_truncate (call_frame_t *frame,
			  xlator_t *this,
			  loc_t *loc,
			  off_t offset);

int32_t default_ftruncate (call_frame_t *frame,
			   xlator_t *this,
			   fd_t *fd,
			   off_t offset);

int32_t default_access (call_frame_t *frame,
			xlator_t *this,
			loc_t *loc,
			int32_t mask);

int32_t default_readlink (call_frame_t *frame,
			  xlator_t *this,
			  loc_t *loc,
			  size_t size);

int32_t default_mknod (call_frame_t *frame,
		       xlator_t *this,
		       loc_t *loc,
		       mode_t mode,
		       dev_t rdev);

int32_t default_mkdir (call_frame_t *frame,
		       xlator_t *this,
		       loc_t *loc,
		       mode_t mode);

int32_t default_unlink (call_frame_t *frame,
			xlator_t *this,
			loc_t *loc);

int32_t default_rmdir (call_frame_t *frame,
		       xlator_t *this,
		       loc_t *loc);

int32_t default_symlink (call_frame_t *frame,
			 xlator_t *this,
			 const char *linkpath,
			 loc_t *loc);

int32_t default_rename (call_frame_t *frame,
			xlator_t *this,
			loc_t *oldloc,
			loc_t *newloc);

int32_t default_link (call_frame_t *frame,
		      xlator_t *this,
		      loc_t *oldloc,
		      loc_t *newloc);

int32_t default_create (call_frame_t *frame,
			xlator_t *this,
			loc_t *loc,
			int32_t flags,
			mode_t mode, fd_t *fd);

int32_t default_open (call_frame_t *frame,
		      xlator_t *this,
		      loc_t *loc,
		      int32_t flags, fd_t *fd,
                      int32_t wbflags);

int32_t default_readv (call_frame_t *frame,
		       xlator_t *this,
		       fd_t *fd,
		       size_t size,
		       off_t offset);

int32_t default_writev (call_frame_t *frame,
			xlator_t *this,
			fd_t *fd,
			struct iovec *vector,
			int32_t count,
			off_t offset,
                        struct iobref *iobref);

int32_t default_flush (call_frame_t *frame,
		       xlator_t *this,
		       fd_t *fd);

int32_t default_fsync (call_frame_t *frame,
		       xlator_t *this,
		       fd_t *fd,
		       int32_t datasync);

int32_t default_opendir (call_frame_t *frame,
			 xlator_t *this,
			 loc_t *loc, fd_t *fd);

int32_t default_fsyncdir (call_frame_t *frame,
			  xlator_t *this,
			  fd_t *fd,
			  int32_t datasync);

int32_t default_statfs (call_frame_t *frame,
			xlator_t *this,
			loc_t *loc);

int32_t default_setxattr (call_frame_t *frame,
			  xlator_t *this,
			  loc_t *loc,
			  dict_t *dict,
			  int32_t flags);

int32_t default_getxattr (call_frame_t *frame,
			  xlator_t *this,
			  loc_t *loc,
			  const char *name);

int32_t default_fsetxattr (call_frame_t *frame,
                           xlator_t *this,
                           fd_t *fd,
                           dict_t *dict,
                           int32_t flags);

int32_t default_fgetxattr (call_frame_t *frame,
                           xlator_t *this,
                           fd_t *fd,
                           const char *name);

int32_t default_removexattr (call_frame_t *frame,
			     xlator_t *this,
			     loc_t *loc,
			     const char *name);

int32_t default_lk (call_frame_t *frame,
		    xlator_t *this,
		    fd_t *fd,
		    int32_t cmd,
		    struct flock *flock);

int32_t default_inodelk (call_frame_t *frame, xlator_t *this,
			 const char *volume, loc_t *loc, int32_t cmd, 
                         struct flock *flock);

int32_t default_finodelk (call_frame_t *frame, xlator_t *this,
			  const char *volume, fd_t *fd, int32_t cmd, 
                          struct flock *flock);

int32_t default_entrylk (call_frame_t *frame, xlator_t *this,
			 const char *volume, loc_t *loc, const char *basename,
			 entrylk_cmd cmd, entrylk_type type);

int32_t default_fentrylk (call_frame_t *frame, xlator_t *this,
			  const char *volume, fd_t *fd, const char *basename,
			  entrylk_cmd cmd, entrylk_type type);

int32_t default_readdir (call_frame_t *frame,
			  xlator_t *this,
			  fd_t *fd,
			  size_t size, off_t off);

int32_t default_readdirp (call_frame_t *frame,
			  xlator_t *this,
			  fd_t *fd,
			  size_t size, off_t off);

int32_t default_xattrop (call_frame_t *frame,
			 xlator_t *this,
			 loc_t *loc,
			 gf_xattrop_flags_t flags,
			 dict_t *dict);

int32_t default_fxattrop (call_frame_t *frame,
			  xlator_t *this,
			  fd_t *fd,
			  gf_xattrop_flags_t flags,
			  dict_t *dict);

int32_t default_notify (xlator_t *this,
			int32_t event,
			void *data,
			...);

int32_t default_forget (xlator_t *this,
			inode_t *inode);

int32_t default_release (xlator_t *this,
			 fd_t *fd);

int32_t default_releasedir (xlator_t *this,
			    fd_t *fd);

int32_t default_setattr (call_frame_t *frame,
                         xlator_t *this,
                         loc_t *loc,
                         struct iatt *stbuf,
                         int32_t valid);

int32_t default_fsetattr (call_frame_t *frame,
                          xlator_t *this,
                          fd_t *fd,
                          struct iatt *stbuf,
                          int32_t valid);

int32_t
default_truncate_cbk (call_frame_t *frame,
		      void *cookie,
		      xlator_t *this,
		      int32_t op_ret,
		      int32_t op_errno,
		      struct iatt *prebuf,
                      struct iatt *postbuf);

int32_t
default_access_cbk (call_frame_t *frame,
		    void *cookie,
		    xlator_t *this,
		    int32_t op_ret,
		    int32_t op_errno);

int32_t
default_readlink_cbk (call_frame_t *frame,
		      void *cookie,
		      xlator_t *this,
		      int32_t op_ret,
		      int32_t op_errno,
		      const char *path,
                      struct iatt *buf);

int32_t
default_mknod_cbk (call_frame_t *frame,
		   void *cookie,
		   xlator_t *this,
		   int32_t op_ret,
		   int32_t op_errno,
		   inode_t *inode,
                   struct iatt *buf,
                   struct iatt *preparent,
                   struct iatt *postparent);

int32_t
default_mkdir_cbk (call_frame_t *frame,
		   void *cookie,
		   xlator_t *this,
		   int32_t op_ret,
		   int32_t op_errno,
		   inode_t *inode,
                   struct iatt *buf,
                   struct iatt *preparent,
                   struct iatt *postparent);

int32_t
default_unlink_cbk (call_frame_t *frame,
		    void *cookie,
		    xlator_t *this,
		    int32_t op_ret,
		    int32_t op_errno,
                    struct iatt *preparent,
                    struct iatt *postparent);

int32_t
default_rmdir_cbk (call_frame_t *frame,
		   void *cookie,
		   xlator_t *this,
		   int32_t op_ret,
		   int32_t op_errno,
                   struct iatt *preparent,
                   struct iatt *postparent);

int32_t
default_symlink_cbk (call_frame_t *frame,
		     void *cookie,
		     xlator_t *this,
		     int32_t op_ret,
		     int32_t op_errno,
		     inode_t *inode,
                     struct iatt *buf,
                     struct iatt *preparent,
                     struct iatt *postparent);

int32_t
default_rename_cbk (call_frame_t *frame,
		    void *cookie,
		    xlator_t *this,
		    int32_t op_ret,
		    int32_t op_errno,
		    struct iatt *buf,
                    struct iatt *preoldparent,
                    struct iatt *postoldparent,
                    struct iatt *prenewparent,
                    struct iatt *postnewparent);

int32_t
default_link_cbk (call_frame_t *frame,
		  void *cookie,
		  xlator_t *this,
		  int32_t op_ret,
		  int32_t op_errno,
		  inode_t *inode,
                  struct iatt *buf,
                  struct iatt *preparent,
                  struct iatt *postparent);


int32_t
default_create_cbk (call_frame_t *frame,
		    void *cookie,
		    xlator_t *this,
		    int32_t op_ret,
		    int32_t op_errno,
		    fd_t *fd,
		    inode_t *inode,
		    struct iatt *buf,
                    struct iatt *preparent,
                    struct iatt *postparent);

int32_t
default_open_cbk (call_frame_t *frame,
		  void *cookie,
		  xlator_t *this,
		  int32_t op_ret,
		  int32_t op_errno,
		  fd_t *fd);

int32_t
default_readv_cbk (call_frame_t *frame,
		   void *cookie,
		   xlator_t *this,
		   int32_t op_ret,
		   int32_t op_errno,
		   struct iovec *vector,
		   int32_t count,
		   struct iatt *stbuf,
                   struct iobref *iobref);

int32_t
default_opendir_cbk (call_frame_t *frame,
		     void *cookie,
		     xlator_t *this,
		     int32_t op_ret,
		     int32_t op_errno,
		     fd_t *fd);

int32_t
default_setattr_cbk (call_frame_t *frame,
                     void *cookie,
                     xlator_t *this,
                     int32_t op_ret,
                     int32_t op_errno,
                     struct iatt *statpre,
                     struct iatt *statpost);

int32_t
default_fsetattr_cbk (call_frame_t *frame,
                      void *cookie,
                      xlator_t *this,
                      int32_t op_ret,
                      int32_t op_errno,
                      struct iatt *statpre,
                      struct iatt *statpost);

int32_t
default_writev_cbk (call_frame_t *frame,
		    void *cookie,
		    xlator_t *this,
		    int32_t op_ret,
		    int32_t op_errno,
                    struct iatt *prebuf,
		    struct iatt *postbuf);

int32_t
default_mem_acct_init (xlator_t *this);
#endif /* _DEFAULTS_H */
