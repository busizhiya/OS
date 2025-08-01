#include "../lib/kernel/stdint.h"
#include "../kernel/global.h"
#include "../kernel/memory.h"
#include "../lib/user/syscall.h"
#include "../lib/string.h"
#include "../lib/stdio.h"

extern void intr_exit(void);

typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

struct Elf32_Ehdr{
    unsigned char e_ident[16];
    Elf32_Half  e_type;
    Elf32_Half  e_machine;
    Elf32_Word  e_version;
    Elf32_Addr  e_entry;
    Elf32_Off   e_phoff;
    Elf32_Off   e_shoff;
    Elf32_Word  e_flags;
    Elf32_Half  e_ehsize;
    Elf32_Half  e_phentsize;
    Elf32_Half  e_phnum;
    Elf32_Half  e_shentsize;
    Elf32_Half  e_shnum;
    Elf32_Half  e_shstrndx;

};

struct Elf32_Phdr{
    Elf32_Word  p_type;
    Elf32_Off   p_offset;
    Elf32_Addr  p_vaddr;
    Elf32_Addr  p_paddr;
    Elf32_Word  p_filesz;
    Elf32_Word  p_memsz;
    Elf32_Word  p_flags;
    Elf32_Word  p_align;
};

enum segment_type{
    PT_NULL,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NOTE,
    PT_SHILB,
    PT_PHDR,
};

/*将文件描述符fd指向的文件中, 偏移为offset, 大小为filesz的段加载到虚拟空间为vaddr的内存*/
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr){
    uint32_t vaddr_frist_page = vaddr & 0xfffff000;
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);
    uint32_t occupy_pages = 0;
    if(filesz > size_in_first_page){
        uint32_t left_size = filesz - size_in_first_page;
        occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
    } else {
        occupy_pages = 1;
    }

    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_frist_page;
    while(page_idx < occupy_pages){
        uint32_t* pde = pde_ptr(vaddr_page);
        uint32_t* pte = pte_ptr(vaddr_page);

        if(!(*pde & 0x00000001) || !(*pte & 0x00000001)){
            
            if(get_a_page(PF_USER, vaddr_page) == NULL){
                return false;
            }
        }
        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void*)vaddr, filesz);
    return true;
}
/*从文件系统上加载用户程序pathname, 成功则返回程序的起始地址, 否则返回-1*/
static int32_t load(const char* pathname){
    int32_t ret = -1;
    struct Elf32_Ehdr elf_header;
    struct Elf32_Phdr prog_header;
    memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));
    
    int32_t fd = sys_open(pathname, O_RDONLY);
    if(fd == -1){
        return -1;
    }
    if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)){
        ret = -1;
        goto done;
    }


    if(memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) \
    || elf_header.e_type != 2 \
    || elf_header.e_machine != 3 \
    || elf_header.e_version != 1 \
    || elf_header.e_phnum > 1024 \
    || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
        ret = -1;
        goto done;
    }

    Elf32_Off prog_header_offset = elf_header.e_phoff;
    Elf32_Half prog_header_size = elf_header.e_phentsize;
    
    uint32_t prog_idx = 0;
    //遍历所有程序头
    while(prog_idx < elf_header.e_phnum){
        memset(&prog_header, 0, prog_header_size);

        sys_lseek(fd, prog_header_offset, SEEK_SET);

        if(sys_read(fd, &prog_header, prog_header_size) != prog_header_size){
            ret = -1;
            goto done;
        }
        if(PT_LOAD == prog_header.p_type){
            if(!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)){
                ret = -1;
                goto done;
            }
        }
        prog_header_offset += elf_header.e_phentsize;
        prog_idx++;
    }
    ret = elf_header.e_entry;
    done:
    sys_close(fd);
    return ret;
}

/*用path指向的程序替换当前进程*/
int32_t sys_execv(const char* path, const char* argv[]){
    uint32_t argc = 0;
    while(argv[argc])
        argc++;
    int32_t entry_point = load(path);
    if(entry_point == -1)
        return -1;
    
    struct task_struct* cur = running_thread();
    memcpy(cur->name, path, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN-1] = 0;
    struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));
    intr_0_stack->ebx = (int32_t) argv;
    intr_0_stack->ecx = argc;
    intr_0_stack->eip = (void*)entry_point;
    intr_0_stack->esp = (void*)0xc0000000;
    asm volatile("movl %0, %%esp; jmp intr_exit" :: "g"(intr_0_stack):"memory");
    return 0;
}
