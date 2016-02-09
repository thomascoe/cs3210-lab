// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE 80 // enough for one VGA text line

uint32_t hex2int(char* str);

struct Command {
  const char *name;
  const char *desc;
  // return -1 to force monitor to exit
  int (*func)(int argc, char **argv, struct Trapframe * tf);
};

static struct Command commands[] = {
  { "help",      "Display this list of commands",        mon_help       },
  { "info-kern", "Display information about the kernel", mon_infokern   },
  { "backtrace", "Run a backtrace to show calling functions", mon_backtrace },
  { "showmappings", "Show physical page mappings", mon_showmappings },
  { "showallmappings", "Show physical page mappings", mon_showallmappings },
  { "setperms", "Change permissions of page mapping", mon_setperms },
  { "dumpp", "Dump memory contents of a physical region", mon_dumpp },
  { "dumpv", "Dump memory contents of a virtual region", mon_dumpv },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
  int i;

  for (i = 0; i < NCOMMANDS; i++)
    cprintf("%s - %s\n", commands[i].name, commands[i].desc);
  return 0;
}

int
mon_infokern(int argc, char **argv, struct Trapframe *tf)
{
  extern char _start[], entry[], etext[], edata[], end[];

  cprintf("Special kernel symbols:\n");
  cprintf("  _start                  %08x (phys)\n", _start);
  cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
  cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
  cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
  cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
  cprintf("Kernel executable memory footprint: %dKB\n",
          ROUNDUP(end - entry, 1024) / 1024);
  return 0;
}


int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
  // Your code here.
  struct Eipdebuginfo info;
  cprintf("Stack backtrace:\n");
  int *ebp = (int *)read_ebp();
  while (ebp != 0) {
    cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", ebp, ebp[1], ebp[2], ebp[3], ebp[4], ebp[5], ebp[6]);
    // Get debug info
    debuginfo_eip(ebp[1], &info);
    cprintf("         %s:%d: %.*s+%d\n", info.eip_file, info.eip_line,
            info.eip_fn_namelen, info.eip_fn_name, ebp[1]-info.eip_fn_addr);
    ebp = (int *)*ebp;
  }
  return 0;
}

// Show pagetable mappings and permission bits for a range of addresses
int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
  // Check arguments
  if (argc != 3) {
    cprintf("Usage: %s 0xaddr 0xlimit\n", argv[0]);
    return -1;
  }
  // Convert hex strings to unsigned int values
  uint32_t addr = hex2int(argv[1]), limit = hex2int(argv[2]);
  // Loop through the addresses, jumping by PGSIZE
  while (addr <= limit) {
    // Get the pagetable entry for this address
    pte_t *pte = pgdir_walk(kern_pgdir, (void *) addr, false);
    if (!pte) {
      cprintf("VA 0x%08x is not mapped to a PA\n", addr);
    } else {
      if (*pte & PTE_P) {
        cprintf("VA 0x%08x | PA 0x%08x | Perms ", addr, PTE_ADDR(*pte));
        cprintf("PTE_P(%x), PTE_U(%x), PTE_W(%x)\n", !!(*pte&PTE_P), !!(*pte&PTE_U), !!(*pte&PTE_W));
      }
    }
    addr += PGSIZE;
  }
  return 0;
}

// Show all valid pagetable mappings and permission bits
int
mon_showallmappings(int argc, char **argv, struct Trapframe *tf)
{
  // Check arguments
  if (argc != 1) {
    cprintf("Usage: %s\n", argv[0]);
    return -1;
  }
  // Start from VA 0x0 and go up to highest address
  uint32_t addr = 0x0, limit = 0xffffffff;
  // Loop through the addresses, jumping by PGSIZE
  while (addr <= limit) {
    // Get the pagetable entry for this address
    pte_t *pte = pgdir_walk(kern_pgdir, (void *) addr, false);
    if (pte) {
      if (*pte & PTE_P) {
        cprintf("VA 0x%08x | PA 0x%08x | Perms ", addr, PTE_ADDR(*pte));
        cprintf("PTE_P(%x), PTE_U(%x), PTE_W(%x)\n", !!(*pte&PTE_P), !!(*pte&PTE_U), !!(*pte&PTE_W));
      }
    }
    addr += PGSIZE;
  }
  return 0;
}

// Update a permission bit for a page
int
mon_setperms(int argc, char **argv, struct Trapframe *tf)
{
  // Check arguments
  if (argc != 4) {
    cprintf("Usage: %s 0xaddr (P|U|W) (0|1)\n", argv[0]);
    return -1;
  }
  // Get arguments
  uint32_t addr = hex2int(argv[1]);
  char *perm_type = argv[2];
  int set_bit = *argv[3] - '0';
  // Get the pagetable entry
  pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, false);
  if (!pte) {
    cprintf("Given address not mapped\n");
    return -1;
  }
  // Print out the original permissions
  cprintf("0x%08x original perms: PTE_P(%x), PTE_U(%x), PTE_W(%x)\n",
          addr, !!(*pte&PTE_P), !!(*pte&PTE_U), !!(*pte&PTE_W));
  if (set_bit) { // set the permission
    if (*perm_type == 'P') {
      *pte |= PTE_P;
    } else if (*perm_type == 'U') {
      *pte |= PTE_U;
    } else if (*perm_type == 'W') {
      *pte |= PTE_W;
    }
  } else { // unset the permission
    if (*perm_type == 'P') {
      uint32_t tmp = ~PTE_P;
      *pte &= tmp;
    } else if (*perm_type == 'U') {
      uint32_t tmp = ~PTE_U;
      *pte &= tmp;
    } else if (*perm_type == 'W') {
      uint32_t tmp = ~PTE_W;
      *pte &= tmp;
    }
  }
  // Print out the new permissions
  cprintf("0x%08x new perms: PTE_P(%x), PTE_U(%x), PTE_W(%x)\n",
          addr, !!(*pte&PTE_P), !!(*pte&PTE_U), !!(*pte&PTE_W));
  return 0;
}

// Dump a range of virtual addresses
int
mon_dumpv(int argc, char **argv, struct Trapframe *tf)
{
  // Check arguments
  if (argc != 3) {
    cprintf("Usage: %s 0xaddr 0xcount\n", argv[0]);
    return -1;
  }
  // Get address and count arguments
  uint32_t *addr = (uint32_t *)hex2int(argv[1]);
  int count = hex2int(argv[2]);
  // Loop through count addresses, printing the contents
  while (count > 0) {
    cprintf("VA 0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", addr, addr[0], addr[1], addr[2], addr[3]);
    addr += 4;
    count -= 0x10;
  }
  return 0;
}

// Dump a range of physical addresses
int
mon_dumpp(int argc, char **argv, struct Trapframe *tf)
{
  // Check arguments
  if (argc != 3) {
    cprintf("Usage: %s 0xaddr 0xcount\n", argv[0]);
    return -1;
  }
  // Get address and count arguments
  uint32_t *addr = (uint32_t *)KADDR(hex2int(argv[1])); // KADDR to make virt from phys
  int count = hex2int(argv[2]);
  // Loop through count addresses, printing the contents
  while (count > 0) {
    cprintf("PA 0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x \n", PADDR(addr), addr[0], addr[1], addr[2], addr[3]);
    addr += 4;
    count -= 0x10;
  }
  return 0;
}

// Take in a hex-formatted string and return a uint32_t
uint32_t
hex2int(char *str)
{
  uint32_t number = 0; // holds final result
  uint32_t temp; // holds each character
  str += 2; // chop off the prefix
  while (*str) { // until we hit the null byte
    // Set temp to char's integer value
    if (*str >= 'a') {
      temp = *str - 'a' + 10;
    } else {
      temp = *str - '0';
    }
    // Add temp to total number
    number = (number * 16) + temp;
    str++;
  }
  return number;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
  int argc;
  char *argv[MAXARGS];
  int i;

  // Parse the command buffer into whitespace-separated arguments
  argc = 0;
  argv[argc] = 0;
  while (1) {
    // gobble whitespace
    while (*buf && strchr(WHITESPACE, *buf))
      *buf++ = 0;
    if (*buf == 0)
      break;

    // save and scan past next arg
    if (argc == MAXARGS-1) {
      cprintf("Too many arguments (max %d)\n", MAXARGS);
      return 0;
    }
    argv[argc++] = buf;
    while (*buf && !strchr(WHITESPACE, *buf))
      buf++;
  }
  argv[argc] = 0;

  // Lookup and invoke the command
  if (argc == 0)
    return 0;
  for (i = 0; i < NCOMMANDS; i++)
    if (strcmp(argv[0], commands[i].name) == 0)
      return commands[i].func(argc, argv, tf);
  cprintf("Unknown command '%s'\n", argv[0]);
  return 0;
}

void
monitor(struct Trapframe *tf)
{
  char *buf;

  cprintf("Welcome to the JOS kernel monitor!\n");
  cprintf("Type 'help' for a list of commands.\n");


  while (1) {
    buf = readline("K> ");
    if (buf != NULL)
      if (runcmd(buf, tf) < 0)
        break;
  }
}
