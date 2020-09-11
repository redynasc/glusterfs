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
#include <glusterfs/locking.h>
#include <glusterfs/timespec.h>

#define ALIGN_SIZE 4096

gf_boolean_t
fsc_inode_is_idle(fsc_inode_t *fsc_inode)
{
    gf_boolean_t is_idle = _gf_false;
    int64_t sec_elapsed = 0;
    struct timeval now = {
        0,
    };
    fsc_conf_t *conf = NULL;

    conf = fsc_inode->conf;

    gettimeofday(&now, NULL);

    sec_elapsed = now.tv_sec - fsc_inode->last_op_time.tv_sec;
    if (sec_elapsed >= conf->time_idle_inode)
        is_idle = _gf_true;

    return is_idle;
}

gf_boolean_t
fsc_inode_is_cache_done(fsc_inode_t *fsc_inode)
{
    return fsc_inode->fsc_size > 0;
    // return (fsc_inode->fsc_size > 0) &&
    //        (fsc_inode->fsc_size >= fsc_inode->s_iatt.ia_size);
}

fsc_inode_t *
fsc_inode_create(xlator_t *this, inode_t *inode, char *path)
{
    int32_t local_path_len = 0;
    fsc_conf_t *priv = this->private;
    fsc_inode_t *fsc_inode = NULL;
    char *base_str = NULL;
    int32_t base_len = 0;
    char *base_cur = NULL;
    char *tmp = NULL;
    char tmm_val = 0;
    // fsc_inode = GF_CALLOC(1, sizeof(fsc_inode_t), gf_fsc_mt_fsc_inode_t);
    fsc_inode = mem_get0(priv->fsc_inode_mem_pool);
    if (fsc_inode == NULL) {
        goto out;
    }
    fsc_inode->conf = priv;
    fsc_inode->inode = inode_ref(inode);
    if (strncmp(path, "<gfid:", 6) == 0) {
        /*<gfid:c8fca0b4-f94c-490a-9b9b-0e7f9cb7f443>/file*/
        base_str = alloca(strlen(path) + 1);
        base_cur = base_str;
        tmp = path + 6;
        while ((tmm_val = *tmp++) != 0) {
            if (tmm_val != '<' && tmm_val != '>') {
                *base_cur = tmm_val;
                base_cur++;
                base_len++;
            }
        }
        *base_cur = 0;

        local_path_len = strlen(priv->cache_dir) + base_len + 1;
        fsc_inode->local_path = GF_CALLOC(1, local_path_len + 1,
                                          gf_fsc_mt_fsc_path_t);
        local_path_len = snprintf(fsc_inode->local_path, local_path_len + 1,
                                  "%s/%s", priv->cache_dir, base_str);

    } else {
        local_path_len = strlen(priv->cache_dir) + strlen(path);
        fsc_inode->local_path = GF_CALLOC(1, local_path_len + 1,
                                          gf_fsc_mt_fsc_path_t);
        local_path_len = snprintf(fsc_inode->local_path, local_path_len + 1,
                                  "%s%s", priv->cache_dir, path);
    }

    gettimeofday(&fsc_inode->last_op_time, NULL);

    INIT_LIST_HEAD(&fsc_inode->inode_list);
    pthread_mutex_init(&fsc_inode->inode_lock, NULL);

    fsc_inodes_list_lock(priv);
    {
        priv->inodes_count++;
        list_add(&fsc_inode->inode_list, &priv->inodes);
    }
    fsc_inodes_list_unlock(priv);
    gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
           "adding to fsc=%p inode_list path=(%s),real_path=(%s)", fsc_inode,
           path, fsc_inode->local_path);
out:
    return fsc_inode;
}

void
fsc_inode_destroy(fsc_inode_t *fsc_inode, int32_t tag)
{
    fsc_conf_t *conf = fsc_inode->conf;
    gf_msg(conf->this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
           "xlator=%p, destroy fsc tag=%d,fd=%d,path=%s", conf->this, tag,
           fsc_inode->fsc_fd, fsc_inode->local_path);

    inode_ctx_put(fsc_inode->inode, conf->this, (uint64_t)0);
    inode_unref(fsc_inode->inode);

    fsc_inode_lock(fsc_inode);
    {
        if (fsc_inode->fsc_fd) {
            close(fsc_inode->fsc_fd);
            fsc_inode->fsc_fd = 0;
        }
    }
    fsc_inode_unlock(fsc_inode);

    GF_FREE(fsc_inode->local_path);
    GF_FREE(fsc_inode->link_target);
    GF_FREE(fsc_inode->write_block);
    pthread_mutex_destroy(&fsc_inode->inode_lock);
    // GF_FREE(fsc_inode);
    mem_put(fsc_inode);
}

void
fsc_inode_from_iatt(fsc_inode_t *fsc_inode, struct iatt *iatt)
{
    if (!iatt) {
        return;
    }

    if (iatt->ia_mtime <= 0) {
        gf_msg("fs-cache", GF_LOG_WARNING, 0, FS_CACHE_MSG_WARNING,
               "fsc_inode fsc=%p invalid iatt local_path=(%s) "
               "ia_size=%" PRId64 ",s_mtime=%" PRId64 ",s_mtime_nsec=%d",
               fsc_inode, fsc_inode->local_path, iatt->ia_size, iatt->ia_mtime,
               iatt->ia_mtime_nsec);
        return;
    } else {
        gf_msg("fs-cache", GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
               "fsc_inode fsc=%p from_iatt local_path=(%s) "
               "ia_size=%" PRId64 ",s_mtime=%" PRId64 ",s_mtime_nsec=%d",
               fsc_inode, fsc_inode->local_path, iatt->ia_size, iatt->ia_mtime,
               iatt->ia_mtime_nsec);
    }

    fsc_inode->s_iatt = *iatt;
}

void
fsc_inode_to_iatt(fsc_inode_t *fsc_inode, struct iatt *iatt)
{
    if (!iatt) {
        return;
    }
    *iatt = fsc_inode->s_iatt;
}

int32_t
fsc_inode_update(xlator_t *this, inode_t *inode, char *path, struct iatt *iabuf)
{
    off_t old_ia_size = 0;
    int64_t old_mtime = 0;
    uint64_t tmp_fsc_inode = 0;
    fsc_inode_t *fsc_inode = NULL;
    fsc_conf_t *conf = NULL;

    if (!this || !inode || !iabuf)
        goto out;

    if (!IA_ISREG(iabuf->ia_type) && !IA_ISLNK(iabuf->ia_type)) {
        gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
               "ignore not reg file path=%s,ia_type=%d", path,
               (int)iabuf->ia_type);
        goto out;
    }

    conf = this->private;
    if (iabuf->ia_size < conf->min_file_size) {
        gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
               "ignore small file path=%s, %" PRIu64 "<  %" PRIu64, path,
               iabuf->ia_size, conf->min_file_size);
        goto out;
    }

    if (!path) {
        gf_msg(this->name, GF_LOG_ERROR, 0, FS_CACHE_MSG_ERROR, "invalid path");
        goto out;
    }

    if (!fsc_check_filter(this->private, path)) {
        goto out;
    }

    LOCK(&inode->lock);
    {
        (void)__inode_ctx_get(inode, this, &tmp_fsc_inode);
        fsc_inode = (fsc_inode_t *)(long)tmp_fsc_inode;

        if (!fsc_inode) {
            fsc_inode = fsc_inode_create(this, inode, path);
            (void)__inode_ctx_put(inode, this, (uint64_t)(long)fsc_inode);
        }
    }
    UNLOCK(&inode->lock);

    fsc_inode_lock(fsc_inode);
    {
        old_ia_size = fsc_inode->s_iatt.ia_size;
        old_mtime = fsc_inode->s_iatt.ia_mtime;
        fsc_inode_from_iatt(fsc_inode, iabuf);
        gettimeofday(&fsc_inode->last_op_time, NULL);

        if (IA_ISREG(iabuf->ia_type) && old_ia_size > 0 &&
            old_ia_size != fsc_inode->s_iatt.ia_size) {
            // invalidate page cache in VFS
            inode_invalidate(inode);
            gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
                   "fsc_inode fsc=%p inode_invalidate ia_size from %" PRId64
                   " to %" PRIu64 ",local_path=(%s),gfid=(%s)",
                   fsc_inode, old_ia_size, fsc_inode->s_iatt.ia_size,
                   fsc_inode->local_path, uuid_utoa(inode->gfid));
        }
        if (IA_ISLNK(iabuf->ia_type) && old_mtime != 0 &&
            old_mtime != fsc_inode->s_iatt.ia_mtime) {
            if (fsc_inode->link_target) {
                gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
                       "fsc_inode fsc=%p invalidate link_target mtime from "
                       "%" PRId64 " to %" PRIu64
                       "local_path=(%s),link_target=(%s)",
                       fsc_inode, old_mtime, fsc_inode->s_iatt.ia_mtime,
                       fsc_inode->local_path, fsc_inode->link_target);
                GF_FREE(fsc_inode->link_target);
                fsc_inode->link_target = NULL;
            }
        }
    }
    fsc_inode_unlock(fsc_inode);

    gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
           "fsc_inode fsc=%p update ia_size from %" PRId64 " to %" PRIu64
           ", path=%s, "
           "local_path=(%s),gfid=(%s)",
           fsc_inode, old_ia_size, fsc_inode->s_iatt.ia_size, path,
           fsc_inode->local_path, uuid_utoa(inode->gfid));
out:
    return 0;
}

int32_t
fsc_inode_open_for_read(xlator_t *this, fsc_inode_t *fsc_inode)
{
    int32_t op_ret = -1;
    int32_t flag = O_RDWR;
    fsc_conf_t *conf = this->private;
    struct stat fstatbuf = {
        0,
    };
    gettimeofday(&fsc_inode->last_op_time, NULL);
    if (fsc_inode->fsc_fd > 0) {
        op_ret = 0;
        goto out;
    }

    if (conf->direct_io_read == 1) {
        flag |= O_DIRECT;
    }

    op_ret = sys_stat(fsc_inode->local_path, &fstatbuf);
    if (op_ret == -1) {
        gf_msg(this->name, GF_LOG_WARNING, 0, FS_CACHE_MSG_WARNING,
               "fsc_inode open for read not find path=(%s),gfid=(%s)",
               fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
        goto out;
    }

    fsc_inode->fsc_fd = sys_open(fsc_inode->local_path, flag,
                                 S_IRWXU | S_IRWXG | S_IRWXO);
    if (fsc_inode->fsc_fd == -1) {
        op_ret = -1;
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "open on %s, flags: %d", fsc_inode->local_path,
               O_DIRECT | O_RDWR);
        goto out;
    }
    fsc_block_init(this, fsc_inode);
    /*fsc_inode->fsc_size = fstatbuf.st_size;*/
    gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_INFO,
           "fsc_inode open for read fd=%d,localsize=%" PRId64
           ",serversize=%" PRIu64 ",path=(%s),gfid=(%s)",
           fsc_inode->fsc_fd, fsc_inode->fsc_size, fsc_inode->s_iatt.ia_size,
           fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
out:
    return op_ret;
}

int32_t
fsc_inode_open_for_write(xlator_t *this, fsc_inode_t *fsc_inode)
{
    int32_t op_ret = -1;
    int32_t flag = O_RDWR | O_CREAT;
    fsc_conf_t *conf = this->private;
    struct stat fstatbuf = {
        0,
    };

    if (fsc_inode->fsc_fd > 0) {
        op_ret = 0;
        goto out;
    }
    gettimeofday(&fsc_inode->last_op_time, NULL);
    if (conf->direct_io_write == 1) {
        flag |= O_DIRECT;
    }

    fsc_inode->fsc_fd = sys_open(fsc_inode->local_path, flag,
                                 S_IRWXU | S_IRWXG | S_IRWXO);
    if (fsc_inode->fsc_fd == -1 && errno == ENOENT) {
        fsc_resovle_dir(this, fsc_inode->local_path);
        // try again
        fsc_inode->fsc_fd = sys_open(fsc_inode->local_path, flag,
                                     S_IRWXU | S_IRWXG | S_IRWXO);
    }

    if (fsc_inode->fsc_fd == -1) {
        op_ret = -1;
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "open on %s failed", fsc_inode->local_path);
        goto out;
    }
    op_ret = sys_fstat(fsc_inode->fsc_fd, &fstatbuf);
    if (op_ret == -1) {
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "fsc_inode open for fstat error ath=(%s),gfid=(%s)",
               fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
        goto out;
    }
    fsc_block_init(this, fsc_inode);
    /*fsc_inode->fsc_size = fstatbuf.st_size;*/
    gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
           "fsc_inode open for write fd=%d,localsize=%" PRId64
           ",serversize=%" PRIu64 ",path=(%s),gfid=(%s)",
           fsc_inode->fsc_fd, fsc_inode->fsc_size, fsc_inode->s_iatt.ia_size,
           fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
out:
    return op_ret;
}

int32_t
fsc_inode_read(fsc_inode_t *fsc_inode, call_frame_t *frame, xlator_t *this,
               fd_t *fd, size_t size, off_t offset, uint32_t flags,
               dict_t *xdata)
{
    int32_t op_ret = -1;
    int32_t op_errno = 0;
    gf_boolean_t is_fault = _gf_true;
    struct iobuf *iobuf = NULL;
    struct iobref *iobref = NULL;
    struct iovec vec = {
        0,
    };
    struct iatt stbuf = {
        0,
    };
    dict_t *rsp_xdata = NULL;

    fsc_inode_lock(fsc_inode);
    {
        op_ret = fsc_inode_open_for_read(this, fsc_inode);
        if (op_ret >= 0) {
            op_ret = fsc_block_is_cache(this, fsc_inode, offset, size);
            if (op_ret == 0) {
                is_fault = _gf_false;
            }
        }
    }
    fsc_inode_unlock(fsc_inode);

    if (is_fault) {
        op_ret = -1;
        gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
               "fsc_inode fault=(%s),fd=%d, offset=%" PRId64
               ",req_size=%" GF_PRI_SIZET,
               fsc_inode->local_path, fsc_inode->fsc_fd, offset, size);
        goto out;
    }

    /* read */
    iobuf = iobuf_get_page_aligned(this->ctx->iobuf_pool, size, ALIGN_SIZE);
    if (!iobuf) {
        op_errno = ENOMEM;
        op_ret = -1;
        goto out;
    }

    op_ret = sys_pread(fsc_inode->fsc_fd, iobuf->ptr, size, offset);
    if (op_ret == -1) {
        op_errno = errno;
        gf_msg(this->name, GF_LOG_ERROR, errno, FS_CACHE_MSG_ERROR,
               "read failed on gfid=%s, "
               "fd=%d, offset=%" PRIu64 " size=%" GF_PRI_SIZET
               ", "
               "buf=%p",
               uuid_utoa(fsc_inode->inode->gfid), fsc_inode->fsc_fd, offset,
               size, iobuf->ptr);
        op_ret = -1;
        goto out;
    }

    if ((offset + op_ret) >= fsc_inode->s_iatt.ia_size) {
        /*op_errno = ENOENT;*/
        gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
               "fsc_inode read local finish=(%s),fd=%d, "
               "offset=%" PRId64 ",req_size=%" GF_PRI_SIZET
               ", rsp_size=%d, op_errno=%d",
               fsc_inode->local_path, fsc_inode->fsc_fd, offset, size, op_ret,
               op_errno);
    }
    fsc_inode_to_iatt(fsc_inode, &stbuf);
    vec.iov_base = iobuf->ptr;
    vec.iov_len = op_ret;
    iobref = iobref_new();
    iobref_add(iobref, iobuf);

    gf_msg(this->name, GF_LOG_TRACE, 0, FS_CACHE_MSG_TRACE,
           "fsc_inode read local=(%s),fd=%d, offset=%" PRId64
           ",req_size=%" GF_PRI_SIZET
           ", "
           "rsp_size=%d, op_errno=%d",
           fsc_inode->local_path, fsc_inode->fsc_fd, offset, size, op_ret,
           op_errno);

    STACK_UNWIND_STRICT(readv, frame, op_ret, op_errno, &vec, 1, &stbuf, iobref,
                        rsp_xdata);
out:
    if (iobref)
        iobref_unref(iobref);
    if (iobuf)
        iobuf_unref(iobuf);
    return op_ret;
}

char *
fsc_page_aligned_alloc(size_t size, char **aligned_buf)
{
    char *alloc_buf = NULL;
    char *buf = NULL;

    alloc_buf = GF_CALLOC(1, (size + ALIGN_SIZE),
                          gf_fsc_mt_fsc_posix_page_aligned_t);
    if (!alloc_buf)
        goto out;
    /* page aligned buffer */
    buf = GF_ALIGN_BUF(alloc_buf, ALIGN_SIZE);
    *aligned_buf = buf;
out:
    return alloc_buf;
}

int32_t
fsc_inode_update_symlink(fsc_inode_t *fsc_inode, xlator_t *this,
                         const char *link, struct iatt *sbuf, dict_t *xdata)
{
    int64_t old_mtime = 0;
    if (!link) {
        return -1;
    }
    fsc_inode_lock(fsc_inode);
    if (!fsc_inode->link_target) {
        fsc_inode->link_target = gf_strdup(link);
        fsc_inode_from_iatt(fsc_inode, sbuf);
        fsc_symlink(this, fsc_inode->link_target, fsc_inode->local_path, sbuf);

        gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
               "fsc_inode fsc=%p update1 link_target "
               "local_path=(%s),old_target=(%s),link_target=(%s)",
               fsc_inode, fsc_inode->local_path, fsc_inode->link_target, link);
    } else {
        old_mtime = fsc_inode->s_iatt.ia_mtime;
        fsc_inode_from_iatt(fsc_inode, sbuf);
        if (fsc_inode->s_iatt.ia_mtime != old_mtime) {
            gf_msg(this->name, GF_LOG_INFO, 0, FS_CACHE_MSG_INFO,
                   "fsc_inode fsc=%p update2 link_target mtime from %" PRId64
                   " to %" PRIu64
                   "local_path=(%s),old_target=(%s),link_target=(%s)",
                   fsc_inode, old_mtime, fsc_inode->s_iatt.ia_mtime,
                   fsc_inode->local_path, fsc_inode->link_target, link);
            GF_FREE(fsc_inode->link_target);
            fsc_inode->link_target = gf_strdup(link);
            fsc_symlink(this, fsc_inode->link_target, fsc_inode->local_path,
                        sbuf);
        }
    }
    fsc_inode_unlock(fsc_inode);
    return 0;
}

int32_t
fsc_inode_read_link(fsc_inode_t *fsc_inode, call_frame_t *frame, xlator_t *this,
                    size_t size, dict_t *xdata)
{
    int32_t op_ret = 0;
    char *link = NULL;
    char link_target[256] = {0};
    struct iatt stbuf = {
        0,
    };
    struct stat local_statbuf = {
        0,
    };
    int64_t local_mtime = 0;

    fsc_inode_lock(fsc_inode);
    if (fsc_inode->link_target) {
        link = gf_strdup(fsc_inode->link_target);
    }
    fsc_inode_unlock(fsc_inode);

    if (link) {
        gf_msg(this->name, GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
               "fsc_inode readlink local1 success path=(%s),link_target=(%s)",
               fsc_inode->local_path, link);

        fsc_inode_to_iatt(fsc_inode, &stbuf);
        STACK_UNWIND_STRICT(readlink, frame, strlen(link), 0, link, &stbuf,
                            NULL);
        GF_FREE(link);
        return 0;
    }

    /*try read from local*/
    op_ret = sys_lstat(fsc_inode->local_path, &local_statbuf);
    if (op_ret == -1) {
        gf_msg(this->name, GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
               "fsc_inode sys_readlink  not find path=(%s),gfid=(%s)",
               fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
        return -1;
    }

    // check mtime
    local_mtime = local_statbuf.st_mtime;
    if (fsc_inode->s_iatt.ia_mtime > 0 &&
        local_mtime != fsc_inode->s_iatt.ia_mtime) {
        gf_msg(this->name, GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
               "fsc_inode readlink  old lmtime=%" PRId64 " smtime=%" PRId64
               " path=(%s),gfid=(%s)",
               local_mtime, fsc_inode->s_iatt.ia_mtime, fsc_inode->local_path,
               uuid_utoa(fsc_inode->inode->gfid));
        return -1;
    }

    op_ret = sys_readlink(fsc_inode->local_path, link_target, 255);
    if (op_ret == -1) {
        gf_msg(this->name, GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
               "fsc_inode readlink  not find path=(%s),gfid=(%s)",
               fsc_inode->local_path, uuid_utoa(fsc_inode->inode->gfid));
        return -1;
    }

    fsc_inode_lock(fsc_inode);
    fsc_inode->link_target = gf_strdup(link_target);
    fsc_inode_unlock(fsc_inode);
    gf_msg(this->name, GF_LOG_TRACE, errno, FS_CACHE_MSG_TRACE,
           "fsc_inode readlink local2 success path=(%s),link_target=(%s)",
           fsc_inode->local_path, fsc_inode->link_target);

    fsc_inode_to_iatt(fsc_inode, &stbuf);
    STACK_UNWIND_STRICT(readlink, frame, op_ret, 0, link_target, &stbuf, NULL);
    return 0;
}
