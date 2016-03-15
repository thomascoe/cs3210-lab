// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW         0x800

extern void _pgfault_upcall();

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
  void *addr = (void*)utf->utf_fault_va;
  uint32_t err = utf->utf_err;
  int r;

  // Check that the faulting access was (1) a write, and (2) to a
  // copy-on-write page.  If not, panic.
  // Hint:
  //   Use the read-only page table mappings at uvpt
  //   (see <inc/memlayout.h>).

  // LAB 4: Your code here.
  if (!((err & FEC_WR) && (uvpt[PGNUM(addr)] & PTE_COW))) {
    panic("pagefault not caused by write to a COW page");
  }

  // Allocate a new page, map it at a temporary location (PFTEMP),
  // copy the data from the old page to the new page, then move the new
  // page to the old page's address.
  // Hint:
  //   You should make three system calls.

  // LAB 4: Your code here.

  // Allocate the new user-writeable page
  r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W);
  if (r < 0) {
    panic("user pgfault page_alloc failed");
  }

  // Copy all data from old page to new page
  memmove(PFTEMP, ROUNDDOWN(addr, PGSIZE), PGSIZE);

  // Map PFTEMP to addr
  r = sys_page_map(0, PFTEMP, 0, ROUNDDOWN(addr, PGSIZE), PTE_P|PTE_U|PTE_W);
  if (r < 0) {
    panic("user pgfault page_map failed");
  }

  // Remove temporary mapping of new page
  r = sys_page_unmap(0, PFTEMP);
  if (r < 0) {
    panic("user pgfault page_unmap failed");
  }
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
  int r;

  // LAB 4: Your code here.
  void *addr = (void*)(pn*PGSIZE);

  uint32_t perms = PTE_U|PTE_P;
  // Check if this page is writeable or COW
  if(uvpt[pn] & (PTE_W|PTE_COW)) {
    perms |= PTE_COW;
  }

  // Map this for the passed in envid
  r = sys_page_map(0, addr, envid, addr, perms);
  if (r < 0) {
    panic("page_map COW envid in duppage()");
  }

  // If this is a COW page, remap for the current envid
  if (perms & PTE_COW) {
    r = sys_page_map(0, addr, 0, addr, perms);
    if (r < 0) {
      panic("page_map COW 0 in duppage()");
    }
  }

  return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
  // LAB 4: Your code here.

  //Set up our custom pagefault handler
  set_pgfault_handler(pgfault);

  //Fork a new process through the syscall
  envid_t envid = sys_exofork();
  if (envid < 0) {
    panic("exofork failed in fork()!");
  }

  if (envid == 0) { // we are the child..
    // Remember to fix "thisenv" in the child process
    thisenv = &envs[ENVX(sys_getenvid())];
    return envid; // Child doesn't need to do anything more
  }

  // Loop through memory until top of user stack, if the page is present dup it
  uintptr_t va;
  for (va = 0; va < USTACKTOP; va += PGSIZE) {
    if ((uvpd[PDX(va)] & PTE_P) && (uvpt[PGNUM(va)] & (PTE_P|PTE_U))) {
      duppage(envid, PGNUM(va));
    }
  }

  // Allocate a new page for the child's exception stack
  int rc = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), (PTE_P|PTE_U|PTE_W));
  if (rc < 0) {
    panic("page_alloc in user fork()");
  }

  // Set pgfault_upcall for the child
  rc = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
  if (rc < 0) {
    panic("set_pgfault_upcall in user fork()");
  }

  // Mark the child as runnable
  rc = sys_env_set_status(envid, ENV_RUNNABLE);
  if (rc < 0) {
    panic("env_set_status RUNNABLE fork()");
  }

  return envid;
}

// Challenge!
int
sfork(void)
{
  panic("sfork not implemented");
  return -E_INVAL;
}
