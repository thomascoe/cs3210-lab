/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)                                          \
        .data;              /*reserve 4 bytes for the func array*/      \
        .long name;                                                     \
        .text;              /*switch back into text section*/           \
        .globl name;            /* define global symbol for 'name' */   \
        .type name, @function;  /* symbol type is function */           \
        .align 2;               /* align function definition */         \
        name:                   /* function starts here */              \
        pushl $(num);                                                   \
        jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)                                     \
        .data;              /*reserve 4 bytes for the func array*/      \
        .long name;                                                     \
        .text;              /*switch back into text section*/           \
        .globl name;                                                    \
        .type name, @function;                                          \
        .align 2;                                                       \
        name:                                                           \
        pushl $0;                                                       \
        pushl $(num);                                                   \
        jmp _alltraps

/* trap_funcs array */
.data
.align 2
.globl trap_funcs
trap_funcs:

//.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

/* Traps provided by x86 */
TRAPHANDLER_NOEC(   trap_divide,    T_DIVIDE);
TRAPHANDLER_NOEC(   trap_debug,     T_DEBUG);
TRAPHANDLER_NOEC(   trap_nmi,       T_NMI);
TRAPHANDLER_NOEC(   trap_brkpt,     T_BRKPT);
TRAPHANDLER_NOEC(   trap_oflow,     T_OFLOW);
TRAPHANDLER_NOEC(   trap_bound,     T_BOUND);
TRAPHANDLER_NOEC(   trap_illop,     T_ILLOP);
TRAPHANDLER_NOEC(   trap_device,    T_DEVICE);
TRAPHANDLER(        trap_dblflt,    T_DBLFLT);
// Trap 9 reserved
.data
.long 0 // Reserve 4 bytes in the array to preserve indexing
TRAPHANDLER(        trap_tss,       T_TSS);
TRAPHANDLER(        trap_segnp,     T_SEGNP);
TRAPHANDLER(        trap_stack,     T_STACK);
TRAPHANDLER(        trap_gpflt,     T_GPFLT);
TRAPHANDLER(        trap_pgflt,     T_PGFLT);
// Trap 15 reserved
.data
.long 0 // Reserve 4 bytes in the array to preserve indexing
TRAPHANDLER_NOEC(   trap_fperr,     T_FPERR);
TRAPHANDLER(        trap_align,     T_ALIGN);
TRAPHANDLER_NOEC(   trap_mchk,      T_MCHK);
TRAPHANDLER_NOEC(   trap_simderr,   T_SIMDERR);

/* Arbitrary traps for syscall and catchall */
.data
.space ((T_SYSCALL-T_SIMDERR-1)*4); // insert space before SYSCALL
TRAPHANDLER_NOEC(   trap_syscall,   T_SYSCALL);
.data
.space ((T_DEFAULT-T_SYSCALL-1)*4); // insert space before DEFAULT
TRAPHANDLER_NOEC(   trap_default,   T_DEFAULT);


/*
 * Lab 3: Your code here for _alltraps
 */
.globl _alltraps
.type _alltraps, @function
.align 2
_alltraps:

// 1. push values to make the stack look like a struct Trapframe
// Error code and trap number already on stack. Push remainder of Trapframe
pushl %ds   // Push 32-bit value for %ds
pushl %es   // Push 32-bit value for %es
pushal      // Push EAX, ECX, EDX, EBX, original ESP, EBP, ESI, and EDI

// 2. load GD_KD into %ds and %es (cannot directly load ds/es, so load eax temp)
mov $GD_KD, %eax
mov %eax, %ds
mov %eax, %es

// 3. pushl %esp to pass a pointer to the Trapframe as an argument to trap()
pushl %esp

// 4. call trap (can trap ever return?)
call trap

// Handle return
addl $4, %esp   // Remove %esp pointer from stack
popal           // Restore EAX, ECX, EDX, EBX, original ESP, EBP, ESI, and EDI
popl %es        // Restore %es
popl %ds        // Restore %ds
addl $8, %esp   // Remove trap number and error code from stack
iret
