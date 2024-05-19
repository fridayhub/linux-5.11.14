gdb \
    -ex "add-auto-load-safe-path /home/liujh-h/mywork/linux-5.11.14/" \
    -ex "file ./vmlinux" \
    -ex "target remote localhost:1234" \
    -ex "b compat_do_replace" \
    -ex "c"
