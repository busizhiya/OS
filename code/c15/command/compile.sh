#!/bin/zsh
if [ -e $1.c ]; then
	x86_64-elf-gcc -o ../build/command/$1.o $1.c -c -m32 -ffreestanding -nostdlib -fno-builtin
	/usr/local/Cellar/lld/20.1.7/bin/ld.lld -Ttext 0x8050000 --image-base=0xc0000000 -o $1 ../build/command/$1.o ../build/simple_crt.a
	dd if=$1 of=../hd60M.img bs=512 count=25 seek=300 conv=notrunc
	cp ../hd60M.img /usr/local/Cellar/bochs/3.0/
else 
	echo "$1.c not exist"
fi
