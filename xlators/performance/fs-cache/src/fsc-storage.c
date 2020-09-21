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
#define LOCAL_GFID_ATTR_NAME "user.gfs.cache.id"

int32_t
fsc_set_timestamp(const char *file, struct iatt *sbuf)
{
    struct stat sb = {
        0,
    };
    iatt_to_stat(sbuf, &sb);
#if defined(HAVE_UTIMENSAT)
    struct timespec new_time[2] = {{
                                       0,
                                   },
                                   {
                                       0,
                                   }};
#else
    struct timeval new_time[2] = {{
                                      0,
                                  },
                                  {
                                      0,
                                  }};
#endif
    int ret = 0;

    /* The granularity is nano seconds if `utimensat()` is available,
     * and micro seconds otherwise.
     */
#if defined(HAVE_UTIMENSAT)
    new_time[0].tv_sec = sb.st_atime;
    new_time[0].tv_nsec = ST_ATIM_NSEC(&sb);

    new_time[1].tv_sec = sb.st_mtime;
    new_time[1].tv_nsec = ST_MTIM_NSEC(&sb);

    /* dirfd = 0 is ignored because `dest` is an absolute path. */
    ret = sys_utimensat(AT_FDCWD, file, new_time, AT_SYMLINK_NOFOLLOW);

#else
    new_time[0].tv_sec = sb.st_atime;
    new_time[0].tv_usec = ST_ATIM_NSEC(&sb) / 1000;

    new_time[1].tv_sec = sb.st_mtime;
    new_time[1].tv_usec = ST_MTIM_NSEC(&sb) / 1000;

    ret = sys_utimes(file, new_time);
#endif
    return ret;
}

int32_t
fsc_set_local_gfid(xlator_t *this, const char *file_full_path, uuid_t in_gfid)
{
    char uuid_str[GF_UUID_BUF_SIZE] = {0};
    gf_uuid_unparse(in_gfid, uuid_str);
    int32_t ret = 0;
    ret = sys_lsetxattr(file_full_path, LOCAL_GFID_ATTR_NAME, uuid_str,
                        GF_UUID_BUF_SIZE - 1, 0);

    if (ret != 0) {
        if (errno == ENOENT) {
            fsc_resovle_dir(this, file_full_path);
            ret = sys_lsetxattr(file_full_path, LOCAL_GFID_ATTR_NAME, uuid_str,
                                GF_UUID_BUF_SIZE - 1, 0);
        }
    }

    gf_msg("fs-cache", GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
           "set_local_gfid %s, ret=%d, uuid=%s", file_full_path, ret, uuid_str);
    return ret;
}

int32_t
fsc_get_local_gfid(const char *file_full_path, uuid_t o_gfid)
{
    char uuid_str[GF_UUID_BUF_SIZE] = {0};
    int32_t ret = 0;
    ret = sys_lgetxattr(file_full_path, LOCAL_GFID_ATTR_NAME, uuid_str,
                        GF_UUID_BUF_SIZE);
    if (ret != -1) {
        if (gf_uuid_parse(uuid_str, o_gfid)) {
            gf_msg("fs-cache", GF_LOG_ERROR, 0, FS_CACHE_MSG_ERROR,
                   "Failed to parse uuid for %s", uuid_str);
            return -1;
        }
    }
    gf_msg("fs-cache", GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
           "get_local_gfid %s, ret=%d, uuid=%s", file_full_path, ret, uuid_str);
    return ret;
}

int32_t
fsc_resovle_dir(xlator_t *this, const char *file_full_path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;
    size_t base_len;
    fsc_conf_t *priv = this->private;

    snprintf(tmp, sizeof(tmp), "%s", file_full_path);
    len = strlen(tmp);
    base_len = strlen(priv->cache_dir);
    if (base_len >= len) {
        return -1;
    }

    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + base_len + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            sys_mkdir(tmp, 0755);
            *p = '/';
        }
    }
    /*mkdir(tmp, 0755);*/
    return 0;
}

int32_t
fsc_symlink(xlator_t *this, const char *oldpath, const char *newpath,
            struct iatt *sbuf)
{
    int32_t op_ret = 0;
    op_ret = sys_symlink(oldpath, newpath);
    if (op_ret != 0) {
        if (errno == EEXIST) {
            op_ret = sys_unlink(newpath);
            if (op_ret != 0) {
                gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
                       "fsc_symlink delete failed path=(%s)", newpath);
                return op_ret;
            }
            op_ret = sys_symlink(oldpath, newpath);
        } else if (errno == ENOENT) {
            fsc_resovle_dir(this, newpath);
            op_ret = sys_symlink(oldpath, newpath);
        }
    }

    if (op_ret == 0) {
        if (sbuf->ia_mtime > 0) {
            op_ret = fsc_set_timestamp(newpath, sbuf);
            if (op_ret != 0) {
                gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
                       "utimes on %s", newpath);
            }
        }
    } else {
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "fsc_symlink failed path=(%s)", newpath);
    }
    return op_ret;
}
