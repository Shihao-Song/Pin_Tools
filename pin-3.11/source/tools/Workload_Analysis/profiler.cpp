#include <iostream>

#include "pin.H"
#include "src/simulation.h"

/*
 * Profiling Tool: 
 * (1) Divide the entire program into intervals (100M instructions);
 * (2) For each interval, record the followings (not limited):
 *     1) Number of (new) first-touch instructions; (Not yet finished)
 *     2) Number of (new) touched pages/page faults; (Not yet finished)
 *     3) Number of correct branch predictions; (Not yet finished)
 *     4) Number of in-correct branch predictions; (Not yet finished)
 *     5) Number of cache hits (all cache levels); (Not yet finished)
 *     6) Number of cache misses (all cache levels); (Not yet finished)
 *     7) Number of cache loads (all cache levels); (Not yet finished)
 *     8) Number of cache evictions (all cache levels); (Not yet finished)
 * */
// Simulation components
BP::Branch_Predictor *bp; // A branch predictor

static bool start_sim = false;
static bool end_sim = false; 
static UINT64 insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount() { ++insn_count;
                           if (insn_count == 10000000000) {start_sim = true; }
                           if (insn_count == 30000000000) { end_sim = true; } }

// Function: branch predictor simulation
static void bpSim(ADDRINT eip, BOOL taken, ADDRINT target)
{
//    if (!start_sim) { return; }

    if (end_sim)
    {
        std::cout << "\nNumber of instructions: " << insn_count << "\n";
        std::cout << "Correctness rate: " << bp->perf() << "%.\n";
        exit(0);
    }

    Instruction instr;

    instr.setPC(eip);
    instr.setBranch();
    instr.setTaken(taken);

    // Sending to the branch predictor.
    bp->predict(instr, insn_count); // I'm using insn_count as time-stamp.
}

// Function: memory access simulation
static void memAccessSim(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    /*
    MICRO_OP micro_op;

    micro_op.setPC(eip);
    if (is_store)
    {
        micro_op.setStore();
    }
    else
    {
        micro_op.setLoad();
    }
    micro_op.setMemAddr(mem_addr);

    // TODO, send to a MMU simulator and a cache simulator
    std::cout << insn_count << ": ";

    if (micro_op.isStore())
    {
        std::cout << "W ";
    }
    else
    {
        std::cout << "R ";
    }

    std::cout << micro_op.getPC() << ", "
              << micro_op.getMemAddr() << "\n";
    */
}

//Function: Other function
static void nonBranchNorMem()
{
//    std::cout << insn_count << ": E\n";
}

// "Main" function: decode and simulate the instruction
static void instructionSim(INS ins)
{
    // Step one, increment instruction count.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_END);

    // Step two, decode and simulate instruction.
    if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
    {
        // Why two calls for a branch?
        // A branch has two path: a taken path and a fall-through path.
        INS_InsertCall(
            ins,
            IPOINT_TAKEN_BRANCH, // Insert a call on the taken edge of the control-flow instruction
            (AFUNPTR)bpSim,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, TRUE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);

        INS_InsertCall(
            ins,
            IPOINT_AFTER, // Insert a call on the fall-through path of 
                          // the last instruction of the instrumented object
            (AFUNPTR)bpSim,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, FALSE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);
    }
    else if (INS_IsMemoryRead (ins) || INS_IsMemoryWrite (ins))
    {
        for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); i++)
        {
            // Why predicated call?
            // For conditional move, the memory access is only executed when predicated true.

            // Why invoke twice?
            // Note that in some architectures a single memory operand can be
            // both read and written (for instance incl (%eax) on IA-32)
            // In that case we instrument it once for read and once for write.

            if (INS_MemoryOperandIsRead(ins, i))
            {
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)memAccessSim,
                    IARG_ADDRINT, INS_Address(ins),
                    IARG_BOOL, FALSE,
                    IARG_MEMORYOP_EA, i,
                    IARG_UINT32, INS_MemoryOperandSize(ins, i),
                    IARG_END);
            }

            if (INS_MemoryOperandIsWritten(ins, i))
            {
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)memAccessSim,
                    IARG_ADDRINT, INS_Address(ins),
                    IARG_BOOL, TRUE,
                    IARG_MEMORYOP_EA, i,
                    IARG_UINT32, INS_MemoryOperandSize(ins, i),
                    IARG_END);
            }
        }
    }
    else
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)nonBranchNorMem, IARG_END);
    }
}

static void traceCallback(TRACE trace, VOID *v)
{
    BBL bbl_head = TRACE_BblHead(trace);

    for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for(INS ins = BBL_InsHead(bbl); ; ins = INS_Next(ins))
        {
            instructionSim(ins);
            if (ins == BBL_InsTail(bbl))
            {
                break;
            }
        }
    }
}

static void printResults(int dummy, VOID *p)
{
    std::cout << "Total number of instructions: " << insn_count << "\n";
    delete bp;
}

int
main(int argc, char *argv[])
{
    PIN_InitSymbols(); // Initialize all the PIN API functions

    // Initialize PIN, e.g., process command line options
    if(PIN_Init(argc,argv))
    {
        return 1;
    }

//    bp = new BP::Two_Bit_Local();
//    bp = new BP::Tournament();
    bp = new BP::PentiumM();

    // Simulate each instruction 
    TRACE_AddInstrumentFunction(traceCallback, 0);

    // Print stats
    PIN_AddFiniFunction(printResults, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

