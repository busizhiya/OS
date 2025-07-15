#!/bin/zsh
/opt/nasm-2.15.05/nasm mbr.asm -I ../include -o mbr.bin && /opt/nasm-2.15.05/nasm loader.asm -I ../include -o loader.bin && dd if=mbr.bin of=../hd60M.img bs=512 count=1 conv=notrunc  && dd if=loader.bin of=../hd60M.img bs=512 seek=2 count=3 conv=notrunc
cp ../hd60M.img /usr/local/Cellar/bochs/3.0/
