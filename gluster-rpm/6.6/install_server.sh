#!/bin/bash
./install_client.sh

cd `dirname $0`
pwd

yum install -y python2-prettytable rpcbind lvm2 libaio

rpm -ivh userspace-rcu-0.10.0-3.el7.x86_64.rpm
rpm -ivh glusterfs-server-6.6-1.el7.x86_64.rpm
rpm -ivh python2-gluster-6.6-1.el7.x86_64.rpm
rpm -ivh glusterfs-geo-replication-6.6-1.el7.x86_64.rpm


glusterd_so_dir=/usr/lib64/glusterfs/6.6/xlator/mgmt/

if [[ -f ${glusterd_so_dir}glusterd.so ]]; then
    /bin/cp ${glusterd_so_dir}glusterd.so  -f ${glusterd_so_dir}glusterd-old.so
fi

rm -f ${glusterd_so_dir}glusterd.so
/bin/cp glusterd.so -f ${glusterd_so_dir}
chmod 755  ${glusterd_so_dir}glusterd.so
systemctl  enable glusterd
systemctl  restart glusterd
