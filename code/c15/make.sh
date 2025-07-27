#!/bin/zsh
cd boot
./make.sh
echo "Boot recompiled"
cd ..

#cd kernel
#./make.sh
#echo "kernel recompiled"
#cd ..
#cd lib/kernel
#./make.sh
#echo "lib recompiled"
#cd ../..
cd build
./make.sh
echo "build all files"
cd ..
cd build
/usr/local/Cellar/lld/20.1.7/bin/ld.lld -Ttext 0xc0001500 --image-base=0x00070000 -e main -o ../kernel.bin  main.o $(ls *.o | grep -v '^main.o$')
echo "kernel.bin linked"
cd ..
dd if=kernel.bin of=hd60M.img bs=512 count=200 seek=9 conv=notrunc
cp hd60M.img /usr/local/Cellar/bochs/3.0/
echo "New hd60M.img copyed"
