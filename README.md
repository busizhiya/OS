# README

本仓库记录了操作系统学习过程中的相关教材、环境、源码、复现记录、经验总结和内核结构说明

## 教材

主教材: 《操作系统 真象还原》(郑刚)	(文件较大, 未上传至仓库内)

辅助教材: 《30天自制操作系统》

## 环境说明

所有代码开发与测试运行均于MacOS15.5上完成, 相关经验说明也基于该环境.

### 相关软件版本

nasm (2.15.05)	bochs (3.0)	

x86_64_elf_gcc	binutils	lld

均可使用`brew install <软件名>`进行下载

### 相关软件配置与参数说明

#### bochs

由于书中使用bochs2.6.2, 与现今版本不符, 故特做说明

在笔者使用过程中, bochs无法通过选项[2]选择读取的配置文件, 故将配置文件命名为bochsrc.txt

因版本差异, 配置内容与书中略有不同.

命令行调试`bochs -dbg`

#### nasm

nasm在编译过程中需要注意参数:

-f elf32	在编译为.o文件时, 需指定为32位elf文件, 否则loader中的解析elf头会出错

#### lld.ld

MacOS 下默认的ld命令不支持-Ttext参数, 故安装lld库, 使用lld.ld命令

需要注意的是, 该命令需要用绝对路径调用, 且待链接文件中必须把main.o放在第一位, 否则会出错!

`/usr/local/Cellar/lld/20.1.7/bin/ld.lld -Ttext 0xc0001500 --image-base=0x00070000 -e main -o ../kernel.bin  main.o $(ls *.o | grep -v '^main.o$')`

#### x86_64_elf_gcc

需要注意参数: -m32, 从而编译为elf32格式

`x86_64-elf-gcc -m32 -c main.c -o main.o`

#### readelf

需要绝对路径调用. 为方便使用, 可以在~/.zshrc中添加alias信息

`alias readelf="/usr/local/Cellar/binutils/2.44/bin/readelf"`

而后刷新`source ~/.zshrc`

## 源码说明

每一章节单独实现一次源码, 命名为c`N`, 若单章节内有较大改动, 则以`_N`作后缀

书中有介绍makefile, 但本仓库不使用makefile, 相关目录内存在make.sh, 基于shell脚本编写, 便于一键完成编译、连接、写入硬盘、拷贝至bochs目录下等操作, 具体脚本可见code/c8目录内(前面章节的make.sh编写的不完善)

## 经验总结

书中部分内容存在印刷错误&口误, 请批判性看待.

当代码出现bug时, bochs虚拟机会出现闪屏, 即不断重新加载BIOS界面, 这不是虚拟机的问题, 是代码有误, 导致CPU reset

在bochs调试命令行下常用的命令

```
b <addr>	下断点
c	一直执行, 直到断点/死循环
s	单步执行, 会进入call函数内部
n	单步执行, 不会进入call函数内部
u /<num> 展示当前位置的后num条汇编指令
x <addr> [addr2] 查看逻辑地址为addr[到addr2]的数据
xp	同上, 但物理地址
r	展示寄存器
info <gdt/idt/...>	展示描述符表等信息
<enter> 回车, 重复执行上一次输入的命令
```

## 内核结构说明

### 内存使用梳理

注: 均为物理地址, 0xc000忽略

0x900~0xb00 GDT

0xb00 存储可用总内存

0x1500 kernel程序

0x7c00 MBR

0x70000	 ; 加载kernel.bin文件

0xb8000~0xbffff  ; 显存

0x9a000 ;位图起始地址
	...		;四个位图

0x9e000 ;内核主线程pcb
	0x9f000	;内核主线程栈顶

0x100000 ; PDE基址:1M起点
	...	256页 : 1页的页目录表 + 1页(0/768同一个) + 254(3G~4G内核区域的页表, 即769~1022) = 256

程序流程:
	从0x7c00启动, 通过MBR加载loader到我们指定的0x900, 
	loader中有0x300的数据段, 定义了GDT(0x900~0xb00,共64项)、总可用内存(0xb00)、启动信息(loader_msg),经人工对齐,loader代码段于0xc00开始,故MBR直接跳转到0xc00.
	在loader代码段中,先从硬盘中读取kernel.bin文件到内存0x70000,解析elf文件格式后, 将kernel中的代码段加载到0x1500.
	loader代码流程:
		0.数据段中设置GDT
		1.BIOS中断打印loader信息
		2.读取总内存,写入0xb00
		3.开启保护模式
		4.设置PTE、PDE, 开启内存分页
		5.从硬盘中读取kernel.bin, 解析elf头后将kernel加载到0x1500
		6.设置esp 0xc009f000	1M的最后一页
		7.跳转至0xc0001500, 开始执行内核代码
	kernel代码流程:
		init_all()
			idt_init()
				idt_desc_init();	//未显式指明idt表地址
    			exception_init();
    			pic_init();
    		timer_init()



