// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

int num_of_free_pages = PHYSTOP/PGSIZE;

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
  int ref_count;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  struct run run_arr[MAX_PAGES];
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree_no_panic(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
//  r = (struct run*)v;
    r = &kmem.run_arr[(V2P(v) / PGSIZE)];

    if (r->ref_count>1){
        cprintf("kfree with r->ref_count=%d\n", r->ref_count);
        panic("kfree with r->ref_count > 1");
    }
  r->next = kmem.freelist;
  kmem.freelist = r;
  r->ref_count = 0;

  num_of_free_pages++;

  if(kmem.use_lock)
    release(&kmem.lock);
}
void
kfree_no_panic(char *v)
{
    struct run *r;

    if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
        panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(v, 1, PGSIZE);

    if(kmem.use_lock)
        acquire(&kmem.lock);
//  r = (struct run*)v;
    r = &kmem.run_arr[(V2P(v) / PGSIZE)];
//    if (r->ref_count>1) panic("kfree with r > 1, maybe we did needed that change....");

    r->next = kmem.freelist;
    kmem.freelist = r;
    r->ref_count = 0;

    num_of_free_pages++;

    if(kmem.use_lock)
        release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;
  if(kmem.use_lock){
    acquire(&kmem.lock);
  }
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    r->ref_count = 1;
    num_of_free_pages--;
  }
    
  if(kmem.use_lock){
    release(&kmem.lock);
  }
  if (r) return P2V((r - kmem.run_arr) * PGSIZE);
  return (char*)r;
}

int getNumberOfFreePages(){
  return num_of_free_pages;
}


void update_num_of_refs(char *v, int update_value){
    struct run *r;
    if (kmem.use_lock)
        acquire(&kmem.lock);

    r = &kmem.run_arr[V2P(v)/PGSIZE];
    r->ref_count += update_value;

    if (kmem.use_lock)
        release(&kmem.lock);
}

int get_num_of_refs(char *v){
    struct run *r = &kmem.run_arr[V2P(v)/PGSIZE];
    return r->ref_count;
}

