#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void) {
    struct cpu *c;

    // Map "logical" addresses to virtual addresses using identity map.
    // Cannot share a CODE descriptor for both kernel and user
    // because it would have to have DPL_USR, but the CPU forbids
    // an interrupt from CPL=0 to DPL=3.
    c = &cpus[cpuid()];
    c->gdt[SEG_KCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, 0);
    c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
    c->gdt[SEG_UCODE] = SEG(STA_X | STA_R, 0, 0xffffffff, DPL_USER);
    c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc) {
    pde_t *pde;
    pte_t *pgtab;

    pde = &pgdir[PDX(va)];
    if (*pde & PTE_P) {
        pgtab = (pte_t *) P2V(PTE_ADDR(*pde));
    } else {
        if (!alloc || (pgtab = (pte_t *) kalloc()) == 0)
            return 0;
        // Make sure all those PTE_P bits are zero.
        memset(pgtab, 0, PGSIZE);
        // The permissions here are overly generous, but they can
        // be further restricted by the permissions in the page table
        // entries, if necessary.
        *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
    }
    return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) {
    char *a, *last;
    pte_t *pte;

    a = (char *) PGROUNDDOWN((uint) va);
    last = (char *) PGROUNDDOWN(((uint) va) + size - 1);
    for (;;) {
        if ((pte = walkpgdir(pgdir, a, 1)) == 0)
            return -1;
        if (*pte & PTE_P)
            panic("remap");
        *pte = pa | perm | PTE_P;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
    void *virt;
    uint phys_start;
    uint phys_end;
    int perm;
} kmap[] = {
        {(void *) KERNBASE, 0,             EXTMEM,  PTE_W}, // I/O space
        {(void *) KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
        {(void *) data,     V2P(data),     PHYSTOP, PTE_W}, // kern data+memory
        {(void *) DEVSPACE, DEVSPACE, 0,            PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t *
setupkvm(void) {
    pde_t *pgdir;
    struct kmap *k;
    if ((pgdir = (pde_t *) kalloc()) == 0) {
        return 0;
    }
    memset(pgdir, 0, PGSIZE);

    if (P2V(PHYSTOP) > (void *) DEVSPACE)
        panic("PHYSTOP too high");
    for (k = kmap; k < &kmap[NELEM(kmap)];
    k++)
    if (mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                 (uint) k->phys_start, k->perm) < 0) {
        freevm(pgdir);
        return 0;
    }
    return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void) {
    kpgdir = setupkvm();
    switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void) {
    lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p) {
    if (p == 0)
        panic("switchuvm: no process");
    if (p->kstack == 0)
        panic("switchuvm: no kstack");
    if (p->pgdir == 0)
        panic("switchuvm: no pgdir");

    pushcli();
    mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                  sizeof(mycpu()->ts) - 1, 0);
    mycpu()->gdt[SEG_TSS].s = 0;
    mycpu()->ts.ss0 = SEG_KDATA << 3;
    mycpu()->ts.esp0 = (uint) p->kstack + KSTACKSIZE;
    // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
    // forbids I/O instructions (e.g., inb and outb) from user space
    mycpu()->ts.iomb = (ushort) 0xFFFF;
    ltr(SEG_TSS << 3);
    lcr3(V2P(p->pgdir));  // switch to process's address space
    popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz) {
    char *mem;

    if (sz >= PGSIZE)
        panic("inituvm: more than a page");
    mem = kalloc();
    memset(mem, 0, PGSIZE);
    mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W | PTE_U);
    memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz) {
    uint i, pa, n;
    pte_t *pte;

    if ((uint) addr % PGSIZE != 0)
        panic("loaduvm: addr must be page aligned");
    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pgdir, addr + i, 0)) == 0)
            panic("loaduvm: address should exist");
        pa = PTE_ADDR(*pte);
        if (sz - i < PGSIZE)
            n = sz - i;
        else
            n = PGSIZE;
        if (readi(ip, P2V(pa), offset + i, n) != n)
            return -1;
    }
    return 0;
}

int is_system_proc() {
    return (myproc()->pid <= 2);
}

void load_mem_page_entry_to_mem(uint va, int index){
    struct proc* p = myproc();
    p->mem_pages[index].va = va;
    p->mem_pages[index].available = 0;
#ifdef NFUA
    p->mem_pages[index].nfu_counter = 0;
#endif
#ifdef LAPA
    p->mem_pages[index].nfu_counter = ALL_ONES;
#endif
}


void handle_cow_fault(uint cow_fault_addr, pde_t *pgdir) {
    if (is_system_proc()) return;
    uint cow_fault_addr_rounded = PGROUNDDOWN(cow_fault_addr);
    pte_t *pte = walkpgdir(pgdir, (void *) cow_fault_addr_rounded, 0);
    char* v_addr = P2V(PTE_ADDR(*pte));

    int ref_count = get_num_of_refs((void *) v_addr);
    cprintf("ref_count: %d\n",ref_count);
    if (ref_count == 1) {
        cprintf("pid: %d, im the only 1 refering.. just changing flags!\n",myproc()->pid);
        *pte |= PTE_W;
        *pte &= ~PTE_COW;
    } else {
        cprintf("pid: %d, creating my copy for page: %d\n",myproc()->pid, v_addr);
        // todo: kill a process if he is asking cow but there is no more memory
        if(myproc()->num_of_mem_pages<MAX_PSYC_PAGES){
            int free_i = find_next_available_mempage();
            myproc()->dont_touch_me = 1;
            load_mem_page_entry_to_mem(cow_fault_addr_rounded, free_i);
//            myproc()->mem_pages[free_i].va = cow_fault_addr_rounded;
//            myproc()->mem_pages[free_i].available = 0;
//            myproc()->mem_pages[free_i].nfu_counter = 0;
            myproc()->num_of_mem_pages += 1;
            QueuePut(free_i, &myproc()->mem_page_q);
            myproc()->dont_touch_me = 0;
        } else{
            cprintf("cow fault, move_page_to_swap, num of swap pages: %d, num of mem pages: %d\n", myproc()->num_of_swap_pages,
                        myproc()->num_of_mem_pages);
                move_page_to_swap(cow_fault_addr_rounded, pgdir);
        }
        // copy on write
        char *new_page = kalloc();
        memmove(new_page, v_addr, PGSIZE);
        // uint old_flags = PTE_FLAGS(*pte);
        uint new_pa = V2P(new_page);
        *pte |= new_pa;
        *pte |= PTE_W;
//        *pte = new_pa | PTE_W | PTE_P|PTE_U;
        // uint new_flags = V2P(new_page) | PTE_FLAGS(*pte) | PTE_W | PTE_P;
        *pte &= ~PTE_COW;
//        *pte &= ~PTE_PG;
        // *pte |= new_flags; 

        
        update_num_of_refs(v_addr, -1);
        cprintf("pid: %d, for va: %d, dec ref_count to: %d\n", myproc()->pid,v_addr,get_num_of_refs(v_addr));
        // cprintf("pid: %d, for va: %d, updated ref_count to: %d\n",
        //  myproc()->pid,v_addr,get_num_of_refs(v_addr));

    }
    lcr3(V2P(pgdir));
}

void print_process_mem_data(uint pg_fault_adrr){
//    uint page_fault_addr_rounded = PGROUNDDOWN(pg_fault_adrr);
//    cprintf("page fault addr: %d, rounded: %d, pid: %d\n", pg_fault_adrr, page_fault_addr_rounded, myproc()->pid);
    cprintf("%d mem pages\n", myproc()->num_of_mem_pages);
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        cprintf("[%d,%d] ", myproc()->mem_pages[i].va, myproc()->mem_pages[i].available);
    }
    cprintf("\n");
    cprintf("nfu counters:\n");
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        cprintf("[%x] ", myproc()->mem_pages[i].nfu_counter);
    }
    cprintf("\n");
    cprintf("%d swap pages\n", myproc()->num_of_swap_pages);
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        cprintf("[%d,%d] ", myproc()->swapped_pages[i].va, myproc()->swapped_pages[i].available);
    }
    cprintf("\n");
}

void swap_pages(uint page_fault_addr, pde_t *pgdir) {
    if (is_system_proc()) return;
    uint page_fault_addr_rounded = PGROUNDDOWN(page_fault_addr);
    myproc()->dont_touch_me = 1;
    int ind_page_to_replace = pick_page_to_replace(pgdir);
    struct mem_page page_to_swap = myproc()->mem_pages[ind_page_to_replace];
    int swap_index = find_next_available_swappage();
    if (swap_index == -1) panic("no room for swapping");
    myproc()->swapped_pages[swap_index].available = 0;
    myproc()->swapped_pages[swap_index].va = page_to_swap.va;

    // write to swap
    int n;
    if ((n = writeToSwapFile(myproc(), (char *) PTE_ADDR(page_to_swap.va), swap_index * PGSIZE, PGSIZE)) <= 0)
        panic("could not write to swap file!");

    // clean memory
    pte_t *old_pte = walkpgdir(pgdir, (void *) page_to_swap.va, 0);
    if (!*old_pte)
        panic("swap_pages: old_pte is empty");
    
    char* old_v_addr = P2V(PTE_ADDR(*old_pte));
    int refsToSwappedFile = get_num_of_refs(old_v_addr);
    cprintf("swap_pages: page: %d with %d refs was swapped!, flags: %x\n",old_v_addr,refsToSwappedFile, PTE_FLAGS(*old_pte));

    pte_t *new_pte = walkpgdir(pgdir, (char *) page_fault_addr_rounded, 0);
//    uint old_flags, old_pa;
//    uint old_pa;
//    old_pa = PTE_ADDR(*old_pte);
//    old_flags = PTE_FLAGS(*old_pte);
    *old_pte &= ~PTE_P;
    *old_pte |= PTE_PG;
    *old_pte &= ~PTE_COW;
    cprintf("swap_pages: page: %d with %d refs was swapped!, flags: %x\n",old_v_addr,refsToSwappedFile, PTE_FLAGS(*old_pte));

    int index_to_read_from = 0;
    char buf[PGSIZE];
    for (; index_to_read_from < MAX_PSYC_PAGES; index_to_read_from++) {
        if (myproc()->swapped_pages[index_to_read_from].va == page_fault_addr_rounded) {
            break;
        }
    }
    if ((n = readFromSwapFile(myproc(), buf, index_to_read_from * PGSIZE, PGSIZE)) <= 0)
        panic("could not read from swap file");


    if(refsToSwappedFile == 1) {
        cprintf("swap_pages - refsToSwappedFile == 1\n");
//        *old_pte &= ~PTE_P;
        kfree(old_v_addr);
//        *new_pte = old_pa | old_flags | PTE_P;
//        *new_pte = old_pa | PTE_P | old_flags;

        char * mem = kalloc();
        memmove(mem, buf,PGSIZE);
        uint new_pa = V2P(mem);

        uint new_flags = PTE_FLAGS(*new_pte);
//        *new_pte = old_pa | new_flags | PTE_P;
        *new_pte = new_pa | new_flags | PTE_P;
        *new_pte &= ~PTE_PG;
//        set_num_of_refs(old_v_addr, 0);

        memmove((char *) page_fault_addr_rounded, buf, PGSIZE); //copy page to physical memory

    } else {
        cprintf("swap_pages: pid: %d, dec count for swapped page, vaddr: %d\n",myproc()->pid, old_v_addr);
        update_num_of_refs(old_v_addr, -1);
        char * mem = kalloc();

        memmove(mem, buf,PGSIZE);
        uint new_pa = V2P(mem);
        uint new_flags = PTE_FLAGS(*new_pte);
        *new_pte = new_pa | new_flags | PTE_P;
        *new_pte &= ~PTE_PG;

    }
    set_num_of_refs(P2V(PTE_ADDR(*new_pte)), 1);
    // | PTE_W
    // *old_pte &= ~PTE_P;

    // re-use old address for page fault page to be loaded
//    pte_t *new_pte = walkpgdir(pgdir, (char *) page_fault_addr_rounded, 0);
//    *new_pte = PTE_ADDR(*old_pte) | PTE_W | PTE_U | PTE_P;
//    *new_pte &= ~PTE_PG;

    // set ref count=1 to the file just came from swap
//    cprintf("reseting ref_count for page came from swap!\n");
//    update_num_of_refs(P2V(PTE_ADDR(*new_pte)), 1);

    // load addr from swap file to ram
//    int index_to_read_from = 0;
//    for (; index_to_read_from < MAX_PSYC_PAGES; index_to_read_from++) {
//        if (myproc()->swapped_pages[index_to_read_from].va == page_fault_addr_rounded) {
//            break;
//        }
//    }
//    char buf[PGSIZE];
//    if ((n = readFromSwapFile(myproc(), buf, index_to_read_from * PGSIZE, PGSIZE)) <= 0)
//        panic("could not read from swap file");
//    memmove((char *) page_fault_addr_rounded, buf, PGSIZE); //copy page to physical memory
    myproc()->swapped_pages[index_to_read_from].available = 1;
    myproc()->swapped_pages[index_to_read_from].va = 0;

    // update mem pages struct
    load_mem_page_entry_to_mem(page_fault_addr_rounded, ind_page_to_replace);
    QueuePut(ind_page_to_replace, &myproc()->mem_page_q);
    myproc()->total_paged_outs++;
    myproc()->dont_touch_me = 0;

    // refresh TLB
    lcr3(V2P(pgdir));
}

int get_num_of_ones(uint x){
    int num_of_ones = 0;
    uint bit = 1;
    while(x > 0){
        if (x & bit)
            num_of_ones += 1;
        x >>= 1;
    }
    return num_of_ones;
}

void move_page_to_swap(uint new_page, pde_t *pgdir) {
    if (is_system_proc()) return;
    // choose mem_page from mem_pages to write to swap
    myproc()->dont_touch_me = 1; // prevent trap updates of the Queue
    int ind_page_to_replace = pick_page_to_replace(pgdir); // also takes out from q
    // cprintf("move_page_to_swap pick_page_to_replace: %d\n", ind_page_to_replace);

    struct mem_page page_to_swap = myproc()->mem_pages[ind_page_to_replace];
    int swap_index = find_next_available_swappage();
    myproc()->swapped_pages[swap_index].available = 0;
    myproc()->swapped_pages[swap_index].va = page_to_swap.va;
    // write to swap
    int n;
    if ((n = writeToSwapFile(myproc(), (char *) PTE_ADDR(page_to_swap.va), swap_index * PGSIZE, PGSIZE)) <= 0)
        panic("could not write to swap file!");
    // cprintf("ballac: %d\n",n);
    pte_t *old_pte = walkpgdir(pgdir, (void *) page_to_swap.va, 0);

    if (page_to_swap.va >= KERNBASE || (*old_pte & PTE_U)==0)
        panic("----------------------------should not happen to us because we are nice-----------------------------------------");


        if (!*old_pte)
        panic("move_page_to_swap: old_pte is empty");
    char* old_v_addr = P2V(PTE_ADDR(*old_pte));
    int refsToSwappedFile = get_num_of_refs(old_v_addr);
//    if (1) print_process_mem_data(0);

//    uint old_pa, old_flags;
//    old_pa = PTE_ADDR(*old_pte);
//    old_flags = PTE_FLAGS(*old_pte);
    *old_pte |= PTE_PG;
    *old_pte &= ~PTE_COW;
    *old_pte &= ~PTE_P;
    cprintf("page that is going to swap va: %d, flags: %d, cow=%d, paged=%d\n", old_v_addr,
            PTE_FLAGS(*old_pte), PTE_FLAGS(*old_pte) & PTE_COW, PTE_FLAGS(*old_pte) & PTE_PG);
    if(refsToSwappedFile == 1) {
        cprintf("move_page_to_swap.. kfree! , vadrr: %d\n", old_v_addr);
        kfree(old_v_addr);
        set_num_of_refs(old_v_addr, 0);
    }
    else {
        cprintf("move_page_to_swap: pid: %d, dec count for swapped page %d\n",myproc()->pid,old_v_addr);
        update_num_of_refs(old_v_addr, -1);
    }

    // | PTE_W - todo: cow+fork decide which flags needed
    // *old_pte &= ~PTE_P;
    cprintf("ind_page_to_replace : %d\n", ind_page_to_replace);
    load_mem_page_entry_to_mem(new_page, ind_page_to_replace);
    myproc()->num_of_swap_pages += 1;

    QueuePut(ind_page_to_replace, &myproc()->mem_page_q);
    myproc()->total_paged_outs++;
    myproc()->dont_touch_me = 0;

    // refresh TLB
    lcr3(V2P(pgdir));
}

int find_next_available_swappage() {
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        if (myproc()->swapped_pages[i].available) {
//            cprintf("find_next_available_swappage return: %d\n", i);
            return i;
        }
    }
    return -1;
}

int find_next_available_mempage() {
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        if (myproc()->mem_pages[i].available) {
            return i;
        }
    }
    return -1;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
#ifndef NONE
//     cprintf("--------------ifndef NONE------------------\n");
    char *mem;
    uint a;

    if (newsz >= KERNBASE)
        return 0;
    if (newsz < oldsz)
        return oldsz;

    a = PGROUNDUP(oldsz);
    for (; a < newsz; a += PGSIZE) {
        if (!is_system_proc()) {
            if (myproc()->num_of_mem_pages + myproc()->num_of_swap_pages > MAX_TOTAL_PAGES) {
                panic("max total pages cannot exceed 32");
            }
            if (myproc()->num_of_mem_pages < MAX_PSYC_PAGES) {
                // cprintf("alloc for pid: %d, num of mem pages: %d, a: %d, pgdir: %d\n"
                //   ,myproc()->pid,myproc()->num_of_mem_pages,a, pgdir);
                int free_i = find_next_available_mempage();
                myproc()->dont_touch_me = 1;
                load_mem_page_entry_to_mem(a, free_i);
//                myproc()->mem_pages[free_i].va = a;
//                myproc()->mem_pages[free_i].available = 0;
//                myproc()->mem_pages[free_i].nfu_counter = 0;
                myproc()->num_of_mem_pages += 1;

                QueuePut(free_i, &myproc()->mem_page_q);
                myproc()->dont_touch_me = 0;
                // cprintf("QueuePut: %d\n", free_i);

            } else {
                cprintf("allocuvm, move_page_to_swap, num of swap pages: %d, num of mem pages: %d\n", myproc()->num_of_swap_pages,
                        myproc()->num_of_mem_pages);
                move_page_to_swap(a, pgdir);
            }
            mem = kalloc();
            if (mem == 0) {
                cprintf("allocuvm out of memory\n");
                deallocuvm(pgdir, newsz, oldsz);
                return 0;
            }
            memset(mem, 0, PGSIZE);
            cprintf("map: pid: %d, va %d, pa %d\n",myproc()->pid,(void*)a,V2P(mem));
            if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
                cprintf("allocuvm out of memory (2)\n");
                deallocuvm(pgdir, newsz, oldsz);
                kfree(mem);
                return 0;
            }
        }else{
            mem = kalloc();
            if (mem == 0) {
                cprintf("allocuvm out of memory\n");
                deallocuvm(pgdir, newsz, oldsz);
                return 0;
            }
            memset(mem, 0, PGSIZE);
            if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
                cprintf("allocuvm out of memory (2)\n");
                deallocuvm(pgdir, newsz, oldsz);
                kfree(mem);
                return 0;
            }
        }
        
    }
    return newsz;
#endif
#ifdef NONE
//    cprintf("--------------ifdef NONE------------------\n");
    // cprintf("inside allocuvm!\n");
    char *mem;
    uint a;

    if (newsz >= KERNBASE)
        return 0;
    if (newsz < oldsz)
        return oldsz;

    a = PGROUNDUP(oldsz);
    for (; a < newsz; a += PGSIZE) {
        if (!is_system_proc()) {
            int free_i = find_next_available_mempage();
            myproc()->mem_pages[free_i].va = a;
            myproc()->mem_pages[free_i].available = 0;
            myproc()->num_of_mem_pages += 1;
            QueuePut(free_i, &myproc()->mem_page_q);
            mem = kalloc();
            if (mem == 0) {
                cprintf("allocuvm out of memory\n");
                deallocuvm(pgdir, newsz, oldsz);
                return 0;
            }
            memset(mem, 0, PGSIZE);
            cprintf("map: pid: %d, va %d, pa %d\n",myproc()->pid,(void*)a,V2P(mem));
            if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
                cprintf("allocuvm out of memory (2)\n");
                deallocuvm(pgdir, newsz, oldsz);
                kfree(mem);
                return 0;
            }
        }else{
            mem = kalloc();
            if (mem == 0) {
                cprintf("allocuvm out of memory\n");
                deallocuvm(pgdir, newsz, oldsz);
                return 0;
            }
            memset(mem, 0, PGSIZE);
            if (mappages(pgdir, (char *) a, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
                cprintf("allocuvm out of memory (2)\n");
                deallocuvm(pgdir, newsz, oldsz);
                kfree(mem);
                return 0;
            }
        }

    }
    return newsz;
#endif
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz) {
    pte_t *pte;
    uint a, pa;

    if (newsz >= oldsz)
        return oldsz;

    a = PGROUNDUP(newsz);
    for (; a < oldsz; a += PGSIZE) {
        pte = walkpgdir(pgdir, (char *) a, 0);
        if (!pte)
            a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
        else if ((*pte & PTE_P) != 0) {
            pa = PTE_ADDR(*pte);
            if (pa == 0)
                panic("kfree");
            char *v = P2V(pa);
            if (get_num_of_refs(v) == 1){
//                cprintf("dealloc.. kfree!\n");
                kfree(v);
            } else {
                update_num_of_refs(v, -1);
                cprintf("dealloc pid: %d, for va: %d, dec ref_count to: %d\n",
                myproc()->pid,v,get_num_of_refs(v));
            }
            // paging
            clear_mem_page_entry(a);
            // paging
            *pte = 0;
            // }
        }
    }
    return newsz;
}

void clear_mem_page_entry(uint va){
    for (int i = 0; i < MAX_PSYC_PAGES; i++) {
        if (myproc()->mem_pages[i].va == va && !myproc()->mem_pages[i].available) {
            myproc()->mem_pages[i].va = 0;
            myproc()->mem_pages[i].available = 1;
            myproc()->mem_pages[i].nfu_counter = 0;
            myproc()->num_of_mem_pages -= 1;
            QueueRemove(&myproc()->mem_page_q, i);
            break;
        }
    }
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir) {
    uint i;
    cprintf("freevm!!!\n");
    if (pgdir == 0)
        panic("freevm: no pgdir");
    deallocuvm(pgdir, KERNBASE, 0);
    for (i = 0; i < NPDENTRIES; i++) {
        if (pgdir[i] & PTE_P) {
            char *v = P2V(PTE_ADDR(pgdir[i]));
            kfree(v);
        }
    }
    cprintf("freevm.. kfree!\n");
    kfree((char *) pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva) {
    pte_t *pte;

    pte = walkpgdir(pgdir, uva, 0);
    if (pte == 0)
        panic("clearpteu");
    *pte &= ~PTE_U;
}

// Given a parent process's page table, dont copy to child.
// Instead, increment ref count on that page (called upon a fork()) 
pde_t *
copyuvm_cow(pde_t *pgdir, uint sz) {
    if(is_system_proc()) panic("sys proc using cow");
    pde_t *d;
    pte_t *pte;
    uint i, flags, pa;
    char *mem;
    cprintf("calling setupkvm from copyuvm_cow!\n");
    if ((d = setupkvm()) == 0){
        cprintf("setupkvm failed!\n");
        return 0;
    }

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
            panic("copyuvm_cow: pte should exist");
        if (!(*pte & PTE_P) && !(*pte & PTE_PG))
            panic("copyuvm_cow: page unknown state!");
        // parents swapped pages will be copied to child mem
        pa = PTE_ADDR(*pte);
        flags = PTE_FLAGS(*pte);
//        if (!(*pte & PTE_U)){ // regular copy
        if (0){ // regular copy
            if ((mem = kalloc()) == 0)
                goto bad;
            memmove(mem, (char *) P2V(pa), PGSIZE);

            if (mappages(d, (void *) i, PGSIZE, V2P(mem), flags) < 0) {
                cprintf("mappings failed\n");
                kfree(mem);
                goto bad;
            }
            lcr3(V2P(d));
            continue;
        }
        if (*pte & PTE_PG) {
            uint new_flags = flags;
            new_flags |= PTE_P;
            new_flags &= ~PTE_COW;
            new_flags &= ~PTE_PG;

            if ((mem = kalloc()) == 0)
                goto bad;
            memmove(mem, (char *) P2V(pa), PGSIZE);

            if (mappages(d, (void *) i, PGSIZE, V2P(mem), new_flags) < 0) {
                cprintf("mappings failed\n");
                kfree(mem);
                goto bad;
            }
            lcr3(V2P(d));
            continue;
        }
        *pte |= PTE_COW;
        *pte &= ~PTE_W;

        cprintf("map: pid: %d, va %d, pa %d\n",  myproc()->pid,(void*)i,pa);
        if (mappages(d, (void *) i, PGSIZE, pa, flags) < 0)
            goto bad;
        update_num_of_refs(P2V(pa), 1);
        lcr3(V2P(pgdir));
        lcr3(V2P(d));
    }
    return d;

    bad:
        freevm(d);
        return 0;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t *
copyuvm(pde_t *pgdir, uint sz) {
    pde_t *d;
    pte_t *pte;
    uint pa, i, flags;
    char *mem;
    cprintf("calling setupkvm from copyuvm regular!\n");
    if ((d = setupkvm()) == 0){
        cprintf("setupkvm failed!\n");
        return 0;
    }

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
            panic("copyuvm: pte should exist");
        if (!(*pte & PTE_P))
            panic("copyuvm: page not present");
        pa = PTE_ADDR(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = kalloc()) == 0)
            goto bad;
        memmove(mem, (char *) P2V(pa), PGSIZE);

        if (mappages(d, (void *) i, PGSIZE, V2P(mem), flags) < 0) {
            cprintf("mappings failed\n");
            kfree(mem);
            goto bad;
        }
    }
    return d;

    bad:
    freevm(d);
    return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char *
uva2ka(pde_t *pgdir, char *uva) {
    pte_t *pte;

    pte = walkpgdir(pgdir, uva, 0);
    if ((*pte & PTE_P) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return 0;
    return (char *) P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len) {
    char *buf, *pa0;
    uint n, va0;

    buf = (char *) p;
    while (len > 0) {
        va0 = (uint) PGROUNDDOWN(va);
        pa0 = uva2ka(pgdir, (char *) va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (va - va0);
        if (n > len)
            n = len;
        memmove(pa0 + (va - va0), buf, n);
        len -= n;
        buf += n;
        va = va0 + PGSIZE;
    }
    return 0;
}

void handle_page_fault(uint pgFaultAddr) {
    uint rounded_p_fault = PGROUNDDOWN(pgFaultAddr);
    pte_t *pte = walkpgdir(myproc()->pgdir, (void *) rounded_p_fault, 0);
    
    cprintf("pid: %d, page fault: %d, cow: %d, pg: %d\n", 
        myproc()->pid,P2V(PTE_ADDR(*pte)), *pte & PTE_COW, *pte & PTE_PG);
    if (pgFaultAddr >= KERNBASE)
        cprintf("pgFaultAddr : %d>= KERNBASE---------------------\n", pgFaultAddr);
    if (!(*pte & PTE_U))
        cprintf("pgFaultAddr : not user----------------\n", pgFaultAddr);
//    if (1) print_process_mem_data(0);
    if (*pte & PTE_COW) {
        // cprintf("page fault: cow\n");
        handle_cow_fault(pgFaultAddr, myproc()->pgdir);
        myproc()->total_page_faults++;
    }

    if (*pte & PTE_PG) {
        // cprintf("page fault: swap_pages\n");
        swap_pages(pgFaultAddr, myproc()->pgdir);
        myproc()->total_page_faults++;
    }
}

int pick_page_to_replace(pde_t *pgdir) {
    if (0) print_process_mem_data(0);
    int i = _pick_page_to_replace(pgdir);
    cprintf("picked page: %d\n", i);
    return i;
}
int _pick_page_to_replace(pde_t *pgdir) {
#ifdef AQ
    return QueueGet(&myproc()->mem_page_q);
#endif
#ifdef SCFIFO
    while (1) {
        int next = QueueGet(&myproc()->mem_page_q);
        // cprintf("next is: %d\n", next);

        pte_t *pte = walkpgdir(pgdir, (void *) myproc()->mem_pages[next].va, 0);
        if (!*pte)
            panic("swap_pages: old_pte is empty");

        if (*pte & PTE_A) { // second chance
            cprintf("index %d (with va: %d) was a good boy, giving second chance\n", next, myproc()->mem_pages[next].va);
            QueuePut(next, &myproc()->mem_page_q);
            *pte &= ~PTE_A;
        } else {
            return next;
        }
    }
#endif
#ifdef NFUA
    uint lowest = 0; // not really lowest
    // set someone to lowest
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            lowest = myproc()->mem_pages[i].nfu_counter;
            break;
        }
    }
    //find real lowest
    int lowest_index = -1;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            if (myproc()->mem_pages[i].nfu_counter <= lowest){
                lowest = myproc()->mem_pages[i].nfu_counter;
                lowest_index = i;
            }
        }
    }
    if (lowest_index == -1)
        panic("NFU didnt find anyone :(");
    return lowest_index;
#endif
#ifdef LAPA
    uint lowest_num_of_ones = 0; // not really lowest_num_of_ones
    // set someone to lowest_num_of_ones
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            lowest_num_of_ones = get_num_of_ones(myproc()->mem_pages[i].nfu_counter);
            break;
        }
    }
    //find lowest_num_of_ones
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            if (get_num_of_ones(myproc()->mem_pages[i].nfu_counter) <= lowest_num_of_ones){
                lowest_num_of_ones = get_num_of_ones(myproc()->mem_pages[i].nfu_counter);
            }
        }
    }

    //find lowest from lowest_num_of_ones

    // set someone to be lowest
    uint lowest = 0;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            if (get_num_of_ones(myproc()->mem_pages[i].nfu_counter)  == lowest_num_of_ones){
                lowest = myproc()->mem_pages[i].nfu_counter;
                break;
            }
        }
    }

    // find lowest
    int lowest_index = -1;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (!myproc()->mem_pages[i].available){
            if (get_num_of_ones(myproc()->mem_pages[i].nfu_counter)  == lowest_num_of_ones){
                if (myproc()->mem_pages[i].nfu_counter <= lowest){
                    lowest = myproc()->mem_pages[i].nfu_counter;
                    lowest_index = i;
                }
            }
        }
    }
    if (lowest_index == -1)
        panic("LAPA lowest_index not found");
    return lowest_index;
#endif
    panic("pick_page_to_replace with no policy");
}

void update_paging_data_nfu_lapa(){
    struct proc* p = myproc();
    pte_t *pte;
    for (int i = 0; i < MAX_PSYC_PAGES; i++){
        if (p->mem_pages[i].available == 0 && !p->dont_touch_me){
            p->mem_pages[i].nfu_counter >>= 1;
            uint counter = p->mem_pages[i].nfu_counter;
            pte = walkpgdir(p->pgdir, (void*)p->mem_pages[i].va, 0);
            if (*pte & PTE_A){
                counter |= LEFT_MOST_BIT;
//                cprintf("updating nfu/lapa counter at index: %d, from: %x to %x\n", i, p->mem_pages[i].nfu_counter, counter);
                p->mem_pages[i].nfu_counter = counter;
                *pte &= ~PTE_A;
            }
        }
    }
}


void update_paging_data_aq(){
    struct proc* p = myproc();
    pte_t *pte;
    for (int i = 0; i < MAX_PSYC_PAGES; i++){
        if (p->mem_pages[i].available == 0 && !p->dont_touch_me){
            pte = walkpgdir(p->pgdir, (void*)p->mem_pages[i].va, 0);
            if (*pte & PTE_A){
                QueueMoveBackOne(i, &p->mem_page_q);
                *pte &= ~PTE_A;
            }
        }
    }
}
void update_paging_data(){
//    cprintf("update_paging_data\n");
#ifdef NFUA
    update_paging_data_nfu_lapa();
#endif
#ifdef LAPA
    update_paging_data_nfu_lapa();
#endif
#ifdef AQ
    update_paging_data_aq();
#endif
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

