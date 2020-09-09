/*
  Copyright (c) 2008-2012 Red Hat, Inc. <http://www.redhat.com>
  This file is part of GlusterFS.

  This file is licensed to you under your choice of the GNU Lesser
  General Public License, version 3 or any later version (LGPLv3 or
  later), or the GNU General Public License, version 2 (GPLv2), in all
  cases as published by the Free Software Foundation.
*/
#include <glusterfs/call-stub.h>
#include <glusterfs/defaults.h>
#include <glusterfs/glusterfs.h>
#include <glusterfs/logging.h>
#include <glusterfs/dict.h>
#include <glusterfs/xlator.h>
#include "fs-cache.h"
#include "fsc-mem-types.h"
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

int32_t
fsc_symlink(const char *oldpath, const char *newpath)
{
    int32_t op_ret = 0;
    op_ret = sys_symlink(oldpath, newpath);
    if (!op_ret && errno == EEXIST) {
        op_ret = sys_unlink(newpath) if (op_ret != 0)
        {
            gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
                   "fsc_symlink delete failed path=(%s)", newpath);
            return op_ret;
        }
        op_ret = sys_symlink(oldpath, newpath);
    }

    if (op_ret != 0) {
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "fsc_symlink failed path=(%s)", newpath);
    }
    return op_ret
}
