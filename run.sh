export IMAGE=/home/liujh-h/mywork/linux-5.4.230/rootfs/ubuntu-rootfs-build
export KERNEL=/home/liujh-h/mywork/linux-5.11.14

qemu-system-x86_64 \
    -m 2G -smp 2 \
    -kernel $KERNEL/arch/x86/boot/bzImage \
    -append "console=ttyS0 root=/dev/sda rw earlyprintk=serial net.ifnames=0 nokaslr" \
    -drive file=$IMAGE/rootfs.img,format=raw \
    -net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:10021-:22 \
    -net nic,model=e1000 \
    -virtfs local,path=/home/liujh-h/mywork,mount_tag=tag001,security_model=mapped-xattr \
    -nographic \
    -s \
    -S \
    -pidfile vm.pid
    #-pidfile vm.pid 2>&1 | tee vm.log



# 远程ssh到系统
#ssh -i $IMAGE/rootfs.img.id_rsa -p 10021 -o "StrictHostKeyChecking no" root@localhost
