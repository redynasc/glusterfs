#! /bin/bash

#echo $@
if [[ $# < 1 ]]; then
    echo "pack.sh: $0 version" 
    exit
fi
version=$1

git_tags=`git describe|awk -F"-" '{print$1}'`
git_index=`git describe|awk -F"-" '{print$2}'`
git_commit=`git describe|awk -F"-" '{print$3}'`

debuginfo_rpm=glusterfs-debuginfo-${git_tags:1}-0.${git_index}.git${git_commit:1}.el7.x86_64.rpm
base_rpm=glusterfs-${git_tags:1}-0.${git_index}.git${git_commit:1}.el7.x86_64.rpm
client_rpm=glusterfs-client-xlators-${git_tags:1}-0.${git_index}.git${git_commit:1}.el7.x86_64.rpm

pack_dir=glusterfs-update-${version}
pack_dir_tar=glusterfs-update-${version}.tar.gz

so_array=()
so_array+=("performance/fs-cache")
so_array+=("performance/io-threads")
so_array+=("performance/md-cache")
so_array+=("performance/open-behind")
so_array+=("cluster/dht")

rm /data/glusterfs/extras/LinuxRPM/run /data/glusterfs/extras/LinuxRPM/usr/ /data/glusterfs/extras/LinuxRPM/var/ -rf
rm -fr ${pack_dir}
rm -f ${pack_dir_tar}
mkdir ${pack_dir}

patch_sh=${pack_dir}/patch_so.sh
install_debug=${pack_dir}/install_debug.sh
build_id_list=${pack_dir}/build-id.list
md5_list=${pack_dir}/md5.list
echo "#! /bin/bash

script_dir=\$(dirname \$0)
cd \$script_dir
echo \"into \`pwd\`\"
glusterfs_dir=/usr/lib64/glusterfs/6.6
install_so_dir=/usr/lib64/glusterfs/6.6/xlator
if [ -d \"\${glusterfs_dir}\" ]
then" > $patch_sh


ret=`rpm2cpio $debuginfo_rpm | cpio -div  > /dev/null 2>&1`
ret=`rpm2cpio $base_rpm | cpio -div  > /dev/null 2>&1`
ret=`rpm2cpio $client_rpm | cpio -div  > /dev/null 2>&1`

echo "packing ..."
for so in ${so_array[@]}
do
    src_so=usr/lib64/glusterfs/6.6/xlator/${so}.so
    /bin/cp ${src_so} -f ${pack_dir}/
    echo "    /bin/mv \${install_so_dir}/${so}.so -f \${install_so_dir}/${so}-old.so" >> $patch_sh
    echo "    /bin/cp ${so##*/}.so -f \${install_so_dir}/${so%/*}/" >> $patch_sh 
    echo "    chmod 755  \${install_so_dir}/${so}.so" >> $patch_sh
    echo "" >> $patch_sh

    build_id=`file ${src_so} -b |awk -F[,] '{print$5}'|awk -F[=] '{print$2}'`
    echo "${build_id} ${so##*/}.so" >> ${build_id_list}

    md5_content=`md5sum -b ${src_so} |awk '{print$1}'`
    echo "${md5_content} ${so##*/}.so" >> ${md5_list}    
    #echo $build_id
    if [ -n "${build_id}" ] ; then
        src_so_debug=usr/lib/debug/.build-id/${build_id:0:2}/${build_id:2}.debug
        if [ -f "${src_so_debug}" ]; then
            /bin/cp ${src_so_debug} ${pack_dir}/${so##*/}.so.debug

        else
            echo "not find debug info ${src_so_debug}"
        fi
    else
        echo "not find build_id for ${src_so}"
    fi

done

echo "    echo \"patch so update success\"" >> $patch_sh
echo "else
    echo \"no need\"
fi" >> $patch_sh

echo "script_dir=\$(dirname \$0)
cd \$script_dir
echo \"into \`pwd\`\"

for so in \`ls ./|less|grep -E \"\.so$\"\`; do
    build_id=\`grep \${so} build-id.list |awk '{print\$1}'\`
    if [ -z \"\${build_id}\" ]; then
        echo \"not find build_id for \${so}\"
    else
        prefix_d=\${build_id:0:2}
        name=\${build_id:2}
        debug_dir=/usr/lib/debug/.build-id/\${prefix_d}
        mkdir -p \${debug_dir}
        /bin/cp \${so}.debug -f \${debug_dir}/

        bak=\`pwd\`

        cd \${debug_dir}
        so_dir=\`find  ../../../../lib64/glusterfs/6.6/xlator|grep \${so}\$|xargs dirname\`
        /bin/mv \${so}.debug -f \${so_dir}
        echo \"ln -sf  \${so_dir}/\${so}   \${debug_dir}/\${name}\"
        ln -sf  \${so_dir}/\${so}  \${debug_dir}/\${name}
        echo \"ln -sf  \${so_dir}/\${so}.debug   \${debug_dir}/\${name}.debug\"
        ln -sf  \${so_dir}/\${so}.debug   \${debug_dir}/\${name}.debug

        cd \${bak}
    fi
done" > ${install_debug}
tar -zcvf ${pack_dir_tar} ${pack_dir}

echo "packing end"



