/* The program's entry point without glibc's crt1.o (there is no MIPS sysroot in the build):
 * __start hands main, argc, argv and the stack to __libc_start_main, as crt1.o does on MIPS o32,
 * with no init or fini of our own (the loader's rtld_fini in $2 is passed on). Q2 Pod's helpers
 * start the same way (patch/start.c). */
__asm__(".set noreorder\n.globl __start\n.type __start, @function\n__start:\n"
        "lui $28, %hi(_gp)\naddiu $28, $28, %lo(_gp)\n"
        "move $31, $0\n"
        "lw $5, 0($29)\n"       /* argc */
        "addiu $6, $29, 4\n"    /* argv */
        "li $8, -8\nand $29, $29, $8\naddiu $29, $29, -32\n"
        "lui $4, %hi(main)\naddiu $4, $4, %lo(main)\n"
        "move $7, $0\n"         /* init */
        "sw $0, 16($29)\n"      /* fini */
        "sw $2, 20($29)\n"      /* rtld_fini */
        "sw $29, 24($29)\n"     /* stack_end */
        "jal __libc_start_main\nnop\n1: b 1b\nnop\n.set reorder\n");

/* crt1.o's marker: without it glibc gives the program libc5-era stdio FILEs */
extern const int _IO_stdin_used;
const int _IO_stdin_used = 0x20001;
