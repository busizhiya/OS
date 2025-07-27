#!/bin/zsh
cd ../kernel
for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o ../build/$i\_asm.o
done
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done

cd ../device
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done

cd ../lib/kernel
for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o ../../build/$i\_asm.o
done
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done
cd ../user
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done



cd ../../lib
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done

cd ../thread
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done
for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o ../build/$i\_asm.o
done
cd ../userprog
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done
cd ../fs
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done
cd ../shell
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
done

#  command
cd ../command
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
        x86_64-elf-gcc -o ../build/command/$i.o $i.c -c -m32 -ffreestanding -nostdlib -fno-builtin
	cd ../build
	/usr/local/Cellar/lld/20.1.7/bin/ld.lld -Ttext 0x8050000 --image-base=0xc0000000 -e main -o ../command/$i command/$i.o stdio.o debug.o syscall.o string.o print_asm.o
done
