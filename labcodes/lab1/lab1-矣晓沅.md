lab1 实验报告
=======================
计25 矣晓沅 2012011364
-----------------------------------------
# 练习一
*1.*
查看lab1根目录下的makefile文件，可看出编译生成过程大概分为以下几步：
**（1）** define compiler and flags
定义编译的相关信息
如 
HOSTCC		:= gcc
HOSTCFLAGS	:= -g -Wall -O2
CC		:= $(GCCPREFIX)gcc
CFLAGS	:= -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc $(DEFS)
CFLAGS	+= $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
表示编译器为GCC 
-Wall为显示所有警告信息
-o2为优化级别
-ggdb为提供编译信息
-fno-builtin为除非利用"__builtin_"进行引用，否则不识别所有内建函数
-m32为目标代码设定为32位
-fno-stack-protector 为部检测缓冲区溢出
 
 **（2）**将kernel的相关文件包含进去，create kernel target
 **（3）**创建bootblock，生成bootblock.o
          其中$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
          设定了程序的入口地址为0x7c00
 **（4）**create 'sign' tools
 **（5）**创建UCore.img文件
         $(V)dd if=/dev/zero of=$@ count=10000 用空白符初始化
         $(V)dd if=$(bootblock) of=$@ conv=notrunc 以bootblock为输入
         $(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc 输入kernel
         创建img文件
   大概为以上流程，这个makefile有些复杂，没有太看得懂。
   
*2.*
根据视频中老师的提示，在tools目录下的sign.c文件里可以看到，符合规范的主引导扇区有以下特征：
**(1)** 大小为512字节
char buf[512]; 因为主引导扇区也是一个扇区，扇区大小为512字节
**(2)** 最后结尾的两字节分别为0x55和0xAA
 buf[510] = 0x55;
 buf[511] = 0xAA;
** (3)**程序部分不超过510字节
  if (st.st_size > 510) {
        fprintf(stderr, "%lld >> 510!!\n", (long long)st.st_size);
        return -1;
    }
    根据课件里讲到的知识，这510字节应该分为446字节的启动代码和64字节的硬盘区分表

# 练习二
*1.*
启动qemu，加载ucore.img 用另一个终端进入gdb模式并用命令Remote debugging using localhost:1234
连接qemu。在qemu下打印pc，可看到如下结果：
0xfffffff0:  ljmp   $0xf000,$0xe05b
0xfffffff5:  xor    %dh,0x322f
0xfffffff9:  xor    (%bx),%bp
0xfffffffb:  cmp    %di,(%bx,%di)
0xfffffffd:  add    %bh,%ah
0xffffffff:  add    %al,(%bx,%si)
即加电后的第一条指令是一条长跳转指令，该指令跳转至0x000fe05b处，继续单步跟踪，反汇编得到的部分指令如下：
0x000fe05b:  cmpl   $0x0,%cs:0x65a4
0x000fe062:  jne    0xfd2b9
0x000fe066:  xor    %ax,%ax
0x000fe068:  mov    %ax,%ss
0x000fe06a:  mov    $0x7000,%esp
0x000fe070:  mov    $0xf3c4f,%edx
0x000fe076:  jmp    0xfd12a
0x000fd12a:  mov    %eax,%ecx
0x000fd12d:  cli    
0x000fd12e:  cld    
0x000fd12f:  mov    $0x8f,%eax
在地址0x000f0cc5附近出现一个循环，应该是循环加载bootloader到内存中。

*2.*
在lab1-init中设置如下内容：
file bin/kernel
target remote :1234
set architecture i8086
b *0x7c00
continue
x /2i $pc
执行 make lab1-mon
命令行打印：
Breakpoint 1 at 0x7c00

Breakpoint 1, 0x00007c00 in ?? ()
=> 0x7c00:	cli    
   0x7c01:	cld   
 断点正常  
 
*3.* 单步跟踪，在执行call bootmain前反汇编得到的代码如下：
=> 0x7c01:	cld    
=> 0x7c02:	xor    %ax,%ax
=> 0x7c04:	mov    %ax,%ds
=> 0x7c06:	mov    %ax,%es
=> 0x7c08:	mov    %ax,%ss
=> 0x7c0a:	in     $0x64,%al
=> 0x7c0c:	test   $0x2,%al
=> 0x7c0e:	jne    0x7c0a
=> 0x7c10:	mov    $0xd1,%al
=> 0x7c12:	out    %al,$0x64
=> 0x7c14:	in     $0x64,%al
=> 0x7c16:	test   $0x2,%al
=> 0x7c18:	jne    0x7c14
=> 0x7c1a:	mov    $0xdf,%al
=> 0x7c1c:	out    %al,$0x60
=> 0x7c1e:	lgdtw  0x7c6c
=> 0x7c23:	mov    %cr0,%eax
=> 0x7c26:	or     $0x1,%eax
=> 0x7c2a:	mov    %eax,%cr0
=> 0x7c2d:	ljmp   $0x8,$0x7c32
=> 0x7c32:	mov    $0xd88e0010,%eax
=> 0x7c36:	mov    %ax,%ds
=> 0x7c38:	mov    %ax,%es
=> 0x7c3a:	mov    %ax,%fs
=> 0x7c3c:	mov    %ax,%gs
=> 0x7c3e:	mov    %ax,%ss
=> 0x7c40:	mov    $0x0,%bp
=> 0x7c45:	mov    $0x7c00,%sp
=> 0x7c4a:	call   0x7cfe
与bootasm.S中基本一致
进入bootmain后，其反汇编得到的代码与bootblock.asm也基本一致。bootblock.asm位于obj目录下，是bootblock.o反汇编得到的，而bootblock.o是由bootasm.S和bootmain.c编译生成的。因此可以看出，在bootblock.asm里，bootmain.c部分汇编得到的代码及地址与单步跟踪反汇编得到的都一致。

*4.*
如，在readseg函数中设置断点，在0x7c80与0x7c8d处设置断点
=> 0x7c80:	mov    %ax,-0x10(%di)
Breakpoint 2, 0x00007c80 in ?? ()
(gdb) b *0x7c8d
Breakpoint 3 at 0x7c8d
(gdb) continue
Continuing.
=> 0x7c8d:	lea    0x1(%bx,%si),%bx
Breakpoint 3, 0x00007c8d in ?? ()

# 练习三
*1.*
Intel早起的8086有20根地址线，能够寻址的最大空间是2^20。但是8086数据处理时是16位寻址，因此8068增加了段地+offset的寻址方式。具体计算方式是段址左移四位+offset，这样最大的寻址空间约为1088K，超过了1M。超过1M时，会发生回卷。286及之后的CPU地址线增大，寻址空间也大于1M，则不需要回卷。为了兼容，将A20地址线控制和键盘控制器的一个输出进行AND操作，这样来控制A20地址线的打开（使能）和关闭（屏蔽\禁止），称为A20。A20开启时，系统才能真正访问1M以上的内存空间；A20禁用时，系统会进行回绕，使得内存访问不超过1M。在我们UCore以之为基础的80386里，加电后处于实模式，16为寻址，bootloader完成后，开启保护模式，32位寻址。要实际进行32位寻址，使用4G的内存空间，则必须开启A20.
A20的开启，在bootasm.S文件中，有如下代码
seta20.1:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

seta20.2:
    inb $0x64, %al                                  # Wait for not busy(8042 input buffer empty).
    testb $0x2, %al
    jnz seta20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1
 
 通过写8042端口进行开启
 
*2.*
 GDT表的初始化在pmm.c的gdt_init函数中进行，包括了GDT和TSS的初始化
 但是在bootloader程序执行的时候，尚未启用段基址。此时在bootasm.S中初始化了一个bootstrap GDT，该GDT表“象征性”第做了段机制，完成逻辑地址到线性地址的转换。但实际上是把虚地址直接作为物理地址来进行寻址。
 
 *3.*
 使能和进入保护模式依然是在bootasm.S中执行。
     lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0
    
    这段汇编代码将cr0寄存器的第0比特位置位1，如此则使能了保护模式
    ljmp $PROT_MODE_CSEG, $protcseg
    这条长跳转语句跳转后，CS，IP寄存器重新加载，进入保护模式。
    
# 练习四
 *1.* 
一个扇区的读写在bootmain.c中的readsect函数
主要分为四个步骤：
**(1)**等待硬盘准备好 waitdisk();
**(2)**发出读取扇区的命令 其对应的代码是原函数中的多个outb函数调用
    这些oubt主要向磁盘写入相应的命令，设置读，相关的状态寄存器等等
**(3)** 等待硬盘准备好 waitdisk();
**(4)** 把磁盘扇区数据读取到指定内存 insl(0x1F0, dst, SECTSIZE / 4);

*2.* 
读取elf格式的UCore OS在bootmian函数中实现，简单来说有以下几个步骤：
**(1)** 读取UCore OS文件的elf文件头
	readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);
**(2)** 通过读取的elf文件头判断文件是否有效，要求e_magic要等于ELF_MAGIC
**(3)** 获取program header表指针(首地址)；以及表项数目
**(4)** 利用readseg函数，将函数段逐一读入内存
	for (; ph < eph; ph ++) {
		readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
	}
**(5)** 跳转至UCore OS首地址，控制权移交UCore OS
	((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();
    该语句把OS文件的入口地址强转为一个返回值为空的函数的函数指针，通过调用该函数跳转至函数首地址，即OS代码的入口地址，完成从bootloader到OS的跳转。
    
# 练习五
简要实现过程：
用read_ebp和read_eip读取当前的ebp和eip值，然后沿函数调用栈逐层打印相关的信息。在循环的每一层，打印eip和ebp的16进制值。然后输出传入当前函数的参数。ebp指向的内存单元保存的是上一个函数的ebp值，ebp+1是返回地址，则ebp+2是第一个参数的地址。用语句uint32_t* argp = (uint32_t*)ebp + 2;将ebp+2作为一个数组的起始地址，则通过argp[0],argp[1],argp[2],argp[3]可逐次取出四个参数。然后再调用print_debuginfo打印相关的信息。注释中的说明要求popup a calling stackframe，沿着函数栈逆向逐一打印信息时，需要ebp换位上一个函数(调用者)的ebp，然后讲eip替换为上一个函数的返回地址，即指向返回后上一个函数中要执行的那条指令。这里注意，需要先取返回地址再替换ebp。另外要注意，源程序中通过STACKFRAME_DEPTH指定调用深度最大为20，但是实际可能不到20.before jumping
 * to the kernel entry, the value of ebp has been set to zero, that's the boundary.则需判断ebp是否为0.
 
 
# 练习六
 *1.*
 在mmu.h里的struct gatedesc可以看出中断描述符一共64bit，8个字节。0~15位，48~63位是offset的低16位与高16位，16~31位是selector。通过selector可以在IDT里查到服务例程入口基址，加上offset即为入口地址。
 
*2.* 首先需要在trap.c中的函数idt_init完成中断向量表的初始化。中断向量表共有256项，遍历idt，利用mmu.h中的SETGATE宏初始化各个中断描述符。sel参数选择GD_KTEXT，表示kernel text。描述符优先级为0最高级0.注意，针对系统调用T_SYSCALL的中断描述符需要特殊处理，类型为1，dpl为3.
其次需要完成trap.c中trap对时钟中断的处理，在trap_dispatch()函数中，case IRQ_OFFSET + IRQ_TIMER里，每次tricks加1，满 TICK_NUM 后打印print_ticks();
 


