#ifndef __HOOKS_H__
#define __HOOKS_H__

#include <stdint.h>
#include <stdio.h>

//Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory");}

//These need to be in sync with the simulator
#define ROI_BEGIN         (1025)
#define ROI_END           (1026)

#ifdef __x86_64__
#define HOOKS_STR  "HOOKS"
static inline void magic_op(uint64_t op) {
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
}
#else
#define HOOKS_STR  "NOP-HOOKS"
static inline void magic_op(uint64_t op) {
    //NOP
}
#endif

static inline void roi_begin() {
    printf("[" HOOKS_STR "] ROI begin\n");
    magic_op(ROI_BEGIN);
}

static inline void roi_end() {
    magic_op(ROI_END);
    printf("[" HOOKS_STR  "] ROI end\n");
}

#endif
