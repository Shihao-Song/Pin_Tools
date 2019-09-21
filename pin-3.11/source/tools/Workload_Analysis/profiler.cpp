#include <iostream>

#include "pin.H"

static UINT64 insn_count = 0;

static void increICount()
{
    ++insn_count;
}

static void Instruction(INS ins, VOID *v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increICount, IARG_END);
}

static void print_results(int dummy, VOID *p)
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

    INS_AddInstrumentFunction(Instruction, NULL);
    PIN_AddFiniFunction(print_results, NULL);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

