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

performance_so_dir=/usr/lib64/glusterfs/6.6/xlator/performance/
/bin/mv ${performance_so_dir}fs-cache.so -f ${performance_so_dir}fs-cache-old.so
/bin/cp fs-cache.so -f ${performance_so_dir}
chmod 755  ${performance_so_dir}fs-cache.so

/bin/mv ${performance_so_dir}md-cache.so -f ${performance_so_dir}md-cache-old.so
/bin/cp md-cache.so -f ${performance_so_dir}
chmod 755  ${performance_so_dir}md-cache.so

/bin/mv ${performance_so_dir}io-threads.so -f ${performance_so_dir}io-threads-old.so
/bin/cp io-threads.so -f ${performance_so_dir}
chmod 755  ${performance_so_dir}io-threads.so

