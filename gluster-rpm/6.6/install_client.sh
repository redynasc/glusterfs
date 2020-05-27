#!/bin/bash
cd `dirname $0`
pwd
yum install -y attr
yum install -y psmisc
yum install -y libtirpc

rpm -ivh glusterfs-libs-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-client-xlators-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-api-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-fuse-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-cli-6.6-1.el7.x86_64.rpm

fs_cache_so_dir=/usr/lib64/glusterfs/6.6/xlator/performance/
if [[ -f ${fs_cache_so_dir}fs-cache.so ]]; then
    /bin/cp ${fs_cache_so_dir}fs-cache.so  -f ${fs_cache_so_dir}fs-cache-old.so
fi
rm -f ${fs_cache_so_dir}fs-cache.so
/bin/cp fs-cache.so -f ${fs_cache_so_dir}
chmod 755  ${fs_cache_so_dir}fs-cache.so

