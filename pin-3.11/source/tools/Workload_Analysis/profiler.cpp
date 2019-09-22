#include <iostream>

#include "pin.H"
#include "src/micro_op.hh"

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

static UINT64 insn_count = 0; // Track how many instructions we have already instrumented.
MICRO_OP micro_op; // Container for the decoded instruction.
// Function: Increment instruction count
static void increICount()
{
    ++insn_count;
}
// Function: Prepare branch instruction.
//static void prepBranch()
//{

//}

static void instructionSim(INS ins, VOID *v)
{

    // Step one, update the instruction count.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increICount, IARG_END);

    // Step two, decoding instruction.
    if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
    {
        
    }

}

static void printResults(int dummy, VOID *p)
{
    std::cout << "Total number of instructions: " << insn_count << "\n";
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

    // Simulate each instruction 
    INS_AddInstrumentFunction(instructionSim, NULL);

    // Print stats
    PIN_AddFiniFunction(printResults, NULL);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

