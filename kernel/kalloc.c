// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

/*
4k 8k 16k 32k 64k 128k 256k 512k 1m 2m
*/
#define BUDDY_MAX_ORDER 10

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist[BUDDY_MAX_ORDER];
  struct run *usedlist[BUDDY_MAX_ORDER];
} kmem;

inline uint64 getBuddySize(uint order) { return (uint64)PGSIZE << order; }

inline uint64 getBuddy(uint64 address, uint order) {
  return address ^ getBuddySize(order);
}

void buddyAdd(struct run *r, struct run *list) {
  if (list == 0 || r < list) {
    r->next = 0;
    list = r;
  } else {
    struct run *previous = list, *next = previous->next;
    for (; next != 0 && next < r; previous = next, next = next->next)
      ;
    previous->next = r;
    r->next = next;
  }
}

/*
remove r in list
return 0 for no error
*/
int buddyRemove(const struct run *r, struct run *list) {
  if (list == 0)
    return -1;

  if (list == r) {
    list = r->next;
  }

  struct run *previous = list, *next = previous->next;
  while (1) {
    if (next == r) {
      previous->next = next->next;
      return 0;
    } else if (next == 0) {
      return -1;
    }

    previous = next;
    next = previous->next;
  }
}

// if r exist in list return 1, otherwise 0
int buddyExist(const struct run *r, const struct run *list) {
  while (list != 0) {
    if (list == r)
      return 1;
    else
      list = list->next;
  }
  return 0;
}

void buddySplit(uint order) {
  if (kmem.freelist[order] == 0) {
    buddySplit(order + 1);
    if (kmem.freelist[order] == 0)
      return;
  }

  struct run *r = kmem.freelist[order];
  buddyRemove(r, kmem.freelist[order]);
  buddyAdd(r, kmem.freelist[order - 1]);
  buddyAdd((struct run *)getBuddy((uint64)r, order - 1),
           kmem.freelist[order - 1]);
}

void buddyCoalesce(struct run *r, uint order) {

  buddyRemove(r, kmem.usedlist[order]);
  // search buddy at every order except max order
  while (order < BUDDY_MAX_ORDER - 1) {
    struct run *buddy = (struct run *)getBuddy((uint64)r, order);
    if (buddyExist(buddy, kmem.freelist[order]) == 0)
      break;
    buddyRemove(buddy, kmem.freelist[order]);
    // make sure r is the start address
    if (r > buddy)
      r = buddy;
    ++order;
  }
  buddyAdd(r, kmem.freelist[order]);
}

void *buddyAlloc(uint npages) {
  static const uint mask = 1 << (sizeof(npages) * 8 - 1);
  uint order = sizeof(npages) * 8;
  // get min order to store npages
  while (1) {
    if (npages & mask)
      break;

    --order;
    npages <<= 1;
  }

  acquire(&kmem.lock);
  if (kmem.freelist[order] == 0x0) {
    buddySplit(order + 1);
    if (kmem.freelist[order] == 0x0)
      return 0;
  }

  struct run *r = kmem.freelist[order];
  buddyRemove(r, kmem.freelist[order]);
  buddyAdd(r, kmem.usedlist[order]);
  release(&kmem.lock);

  // fill with junk
  memset((char *)r, 5, getBuddySize(order));
  return (void *)r;
}

void buddyFree(void *pa) {
  acquire(&kmem.lock);

  for (uint order = 0; order < BUDDY_MAX_ORDER; ++order) {
    if (buddyExist((struct run *)(pa), kmem.usedlist[order])) {
      buddyCoalesce((struct run *)(pa), order);
      break;
    }
  }

  release(&kmem.lock);
}

void kinit() {
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

/*
Get npages of continuous physical memory.
*/
void *getContinuousMemory(uint64 npages) { return 0; }

void demoteSuperPage(void *pa) {
  if (((uint64)pa % SUPERPGSIZE) != 0 || (char *)pa < end ||
      (uint64)pa >= PHYSTOP)
    panic("demoteSuperPage");
}

// Free the superpage of physical memory pointed at by pa,
// which normally should have been returned by a
// call to superalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void superFree(void *pa) {
  // struct run *r;

  if (((uint64)pa % SUPERPGSIZE) != 0 || (char *)pa < end ||
      (uint64)pa >= PHYSTOP)
    panic("superfree");

  for (uint64 end = (uint64)pa + SUPERPGSIZE; (uint64)pa < end; pa += PGSIZE) {
    kfree(pa);
  }
}

// Allocate one 2M-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *superAlloc(void) {
  void *address = getContinuousMemory(SUPERPGSIZE / PGSIZE);
  memset(address, 5, SUPERPGSIZE);
  return address;
}
