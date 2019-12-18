#ifndef __PMU_ASSEMBLY_HH__
#define __PMU_ASSEMBLY_HH__

#include <cmath>
#include <cstdint>
#include <iostream>

namespace PMU
{
typedef uint8_t UINT8;

enum class Operation : UINT8 {AND, OR, NOT}; // Will be extended soon.

struct Args
{
    UINT8 *src_1_addr;
    UINT8 *src_2_addr;
    UINT8 *dest_addr;
    UINT8 len_of_opr;
    UINT8 opr;
};

inline void ROWOPR(Args &args)
{
    UINT8 *src_1_addr = args.src_1_addr;
    UINT8 *src_2_addr = args.src_2_addr;
    UINT8 *dest_addr = args.dest_addr;
    UINT8 *len_of_opr = &(args.len_of_opr);
    UINT8 *opr = &(args.opr);

    asm volatile ("push %[src1]\n\t"
                  "push %[src2]\n\t"
                  "push %[dest]\n\t"
                  "push %[len]\n\t"
                  "push %[op]\n\t"
                  "ud2\n\t"
                  "pop %[op]\n\t"
                  "pop %[len]\n\t"
                  "pop %[dest]\n\t"
                  "pop %[src2]\n\t"
                  "pop %[src1]"
                  : // No outputs
                  : [src1] "r" (src_1_addr), [src2] "r" (src_2_addr), [dest] "r" (dest_addr),
                    [len] "r" (len_of_opr), [op] "r" (opr)
                  : "rsp");
}

// The final length of operation is 2^len_of_opr.
// For example, if len_of_opr = 2, the size of *src/dest_addr should be 2^2 = 4
inline void ROWAND(UINT8 *src_1_addr, UINT8 *src_2_addr, UINT8 *dest_addr, UINT8 len_of_opr)
{
    Args args {src_1_addr, src_2_addr, dest_addr, len_of_opr, UINT8(Operation::AND)};
    ROWOPR(args);
}

inline void ROWOR(UINT8 *src_1_addr, UINT8 *src_2_addr, UINT8 *dest_addr, UINT8 len_of_opr)
{
    Args args {src_1_addr, src_2_addr, dest_addr, len_of_opr, UINT8(Operation::OR)};
    ROWOPR(args);
}

inline void ROWNOT(UINT8 *src_1_addr, UINT8 *dest_addr, UINT8 len_of_opr)
{
    Args args {src_1_addr, nullptr, dest_addr, len_of_opr, UINT8(Operation::NOT)};
    ROWOPR(args);
}

// Some common sizes defined here
// 256
inline void ROWAND_256(UINT8 *src_1_addr, UINT8 *src_2_addr, UINT8 *dest_addr)
{
    ROWAND(src_1_addr, src_2_addr, dest_addr, log2(256));
}

inline void ROWOR_256(UINT8 *src_1_addr, UINT8 *src_2_addr, UINT8 *dest_addr)
{
    ROWOR(src_1_addr, src_2_addr, dest_addr, log2(256));
}

inline void ROWNOT_256(UINT8 *src_1_addr, UINT8 *dest_addr)
{
    ROWNOT(src_1_addr, dest_addr, log2(256));
}
}

#endif
