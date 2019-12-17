#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

using std::ofstream;

KNOB<std::string> TraceOut(KNOB_MODE_WRITEONCE, "pintool",
    "o", "", "specify output trace file name");

static UINT64 insn_count = 0; // Track how many instructions we have already instrumented.
// ofstream trace_out;
static void increCount() { ++insn_count; }

/*
// Function: memory access simulation
static void memTrace(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    trace_out << num_exes_before_mem << " ";
    trace_out << eip << " ";
    if (is_store) { trace_out << "S "; }
    else { trace_out << "L "; }
    trace_out << mem_addr << "\n";
}
*/

static void pumTrace(CONTEXT *ctxt)
{
    int **sp = (int **)PIN_GetContextReg(ctxt, REG_STACK_PTR);

    int arg0, arg1;
    PIN_SafeCopy(&arg1, *sp, sizeof(int));
    PIN_SafeCopy(&arg0, *(sp + 1), sizeof(int));
    std::cout << "arg 0: " << arg0 << "\n";
    std::cout << "arg 1: " << arg1 << "\n";
}

// "Main" function: decode and simulate the instruction
static void instructionAna(INS ins)
{
    // Step one, increment instruction count.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_END);

    // Step two, detect UD2 instruction
    if (INS_IsHalt(ins))
    {
        INS_InsertCall(ins, IPOINT_BEFORE, 
                       (AFUNPTR)pumTrace,
                       IARG_CONST_CONTEXT,
                       IARG_END);
        // Since UD2 is an invalid instruction, we should delete it after analysis.
        INS_Delete(ins);
    }

    /*
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
    */
}

static void traceCallback(TRACE trace, VOID *v)
{
    BBL bbl_head = TRACE_BblHead(trace);

    for (BBL bbl = bbl_head; BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for(INS ins = BBL_InsHead(bbl); ; ins = INS_Next(ins))
        {
            instructionAna(ins);
            if (ins == BBL_InsTail(bbl))
            {
                break;
            }
        }
    }
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
    // assert(!TraceOut.Value().empty());

    // trace_out.open(TraceOut.Value().c_str());

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

