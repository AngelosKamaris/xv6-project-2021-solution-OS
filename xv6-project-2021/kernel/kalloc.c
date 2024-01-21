// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int reference_count[PHYSTOP/PGSIZE];        //array that stores the amount of times a readable page is called
                                            //I will use pa/PGSIZE as hashing. pa is unique in each page and it is smaller than PHYSTOP.

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  memset(reference_count, 0, sizeof(int)*(PHYSTOP/PGSIZE));   //initialize array filled with zeroes
  freerange(end, (void*)PHYSTOP);
  
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  reference_count[(uint64)pa/PGSIZE]--;             //decrease reference count
  if (reference_count[(uint64)pa/PGSIZE] <= 0){     //if the page is no longer referenced destroy the page
    r = (struct run*)pa;
    acquire(&kmem.lock);
    reference_count[(uint64)pa/PGSIZE]=0;           //make reference count there zero again, so that it may be used again
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
}

void addref(uint64 pa){                          //for adding reference i created a function to use in vm.c
  reference_count[(uint64)pa/PGSIZE]++;
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  int num=(uint64)r/PGSIZE;
  if(r){
    reference_count[num]++;           //add reference counter
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
