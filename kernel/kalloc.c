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
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i) {
    char lock_name[1024];
    snprintf(lock_name, sizeof(lock_name), "kmem_%d", i);
    initlock(&kmem[i].lock, lock_name);
  }
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();

  int id = cpuid();
  acquire(&kmem[id].lock);

  r->next = kmem[id].freelist;
  kmem[id].freelist = r;

  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();

  int id = cpuid();
  acquire(&kmem[id].lock);
  // try self
  r = kmem[id].freelist;
  if (r) {
    // self success
    kmem[id].freelist = r->next;
    release(&kmem[id].lock);
  } else {
    // try others
    release(&kmem[id].lock);
    struct run *tail = 0;
    for (int i = 0; r == 0 && i < NCPU; ++i) {
      if (i == id)
        continue;
      r = steal(i, &tail);
    }

    if (r && r != tail) {
      // steal success
      acquire(&kmem[id].lock);
      tail->next = kmem[id].freelist;
      kmem[id].freelist = r->next;
      release(&kmem[id].lock);
    }
  }

  pop_off();

  if (r) {
    memset((char *)r, 5, PGSIZE);
  }

  return (void *)r;
}

void *steal(int cpu_id, struct run **tail) {
#define BATCH_SIZE 4

  acquire(&kmem[cpu_id].lock);
  struct run *head = kmem[cpu_id].freelist;

  if (head == 0) {
    release(&kmem[cpu_id].lock);
    return 0;
  }

  struct run *walk_tail = head;
  for (int i = 1; i < BATCH_SIZE && walk_tail->next != 0; ++i) {
    walk_tail = walk_tail->next;
  }

  kmem[cpu_id].freelist = walk_tail->next;
  *tail = walk_tail;

  release(&kmem[cpu_id].lock);
  return head;
}
