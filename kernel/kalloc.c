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
  int *refcnts;
  char *pa_start;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  if(PGSIZE & (PGSIZE - 1))
    panic("kinit: PGSIZE must be power of 2");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end) {
  pa_start = (char *)PGROUNDUP((uint64)pa_start);
  // Reserved the first few pages for refcnts.
  kmem.refcnts = pa_start;
  int n = (pa_end - pa_start) / (PGSIZE + sizeof(int));
  uint64 sz = PGROUNDUP(n * sizeof(int));
  kmem.pa_start = (char *)pa_start + sz;
  int i = 0;
  for (char *p = kmem.pa_start; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    kmem.refcnts[i++] = 1;
    kfree(p);
  }
}

void
krefadd(void *pa, int n) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < kmem.pa_start ||
      (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  int *refcnt = &kmem.refcnts[((char *)pa - (char *)kmem.pa_start) / PGSIZE];
  *refcnt += n;
  if (*refcnt < 0)
    panic("kfree");
  if (*refcnt == 0) {
    memset(pa, 1, PGSIZE);
    r = (struct run *)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

int
krefcnt(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < kmem.pa_start ||
      (uint64)pa >= PHYSTOP)
    panic("krefcnt");

  acquire(&kmem.lock);
  int refcnt = kmem.refcnts[((char *)pa - (char *)kmem.pa_start) / PGSIZE];
  release(&kmem.lock);
  return refcnt;
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  krefadd(pa, -1);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void){
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    ++kmem.refcnts[((char *)r - (char *)kmem.pa_start) / PGSIZE];
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
