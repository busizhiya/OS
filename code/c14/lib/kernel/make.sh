for i in $(ls -l *.asm | awk -F ' ' '{print $9}' | awk -F '.' '{print $1}')
do
/opt/nasm-2.15.05/nasm -f elf32 $i.asm -o $i.o
done
