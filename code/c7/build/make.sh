#!/bin/zsh
cd ../kernel
for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o ../build/$i.o
done
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib
done

cd ../device
for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	x86_64-elf-gcc -o ../build/$i.o $i.c -c -m32 -ffreestanding -nostdlib
done

cd ../lib/kernel
for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
	/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o ../../build/$i.o
done
#for i in $(ls -l *.c | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
#do
#        x86_64-elf-gcc -o ../../build/$i.o $i.c -c -m32
#done


