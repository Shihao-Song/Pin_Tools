#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

// Data trace output
using std::ofstream;
ofstream trace_out;
KNOB<std::string> TraceOut(KNOB_MODE_WRITEONCE, "pintool",
    "o", "", "specify output trace file name");

static bool fast_forwarding = true; // Fast-forwarding mode? Initially, we should be
                                    // in fast-forwarding mode.

PIN_LOCK pinLock;

BOOL FollowChild(CHILD_PROCESS childProcess, VOID * userData)
{
    INT appArgc;
    CHAR const * const * appArgv;

    CHILD_PROCESS_GetCommandLine(childProcess, &appArgc, &appArgv);
    std::string childApp(appArgv[0]);

    std::cerr << std::endl;
    std::cerr << "[Pintool] Warning: Child application to execute: " << childApp << std::endl;
    std::cerr << "[Pintool] Warning: We do not run Pin under the child process." << std::endl;
    std::cerr << std::endl;
    return FALSE;
}

static const uint64_t LIMIT = 1000000000;
static uint64_t insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount(THREADID t_id) 
{
    if (fast_forwarding) { return; }

    PIN_GetLock(&pinLock, t_id + 1);

    ++insn_count;

    // Exit if it exceeds a threshold.
    if (insn_count >= LIMIT)
    {
        trace_out << std::flush;
        exit(0);
    }

    PIN_ReleaseLock(&pinLock);
}

static unsigned num_exes_before_mem = 0;
static void nonBranchNorMem(THREADID t_id)
{
    if (fast_forwarding) { return; }

    PIN_GetLock(&pinLock, t_id + 1);
    num_exes_before_mem++;
    PIN_ReleaseLock(&pinLock);

    return;
}

static void bpTrace(THREADID t_id,
                    ADDRINT eip,
		    BOOL taken,
                    ADDRINT target)
{
    if (fast_forwarding) { return; }

    // Lock the print out
    PIN_GetLock(&pinLock, t_id + 1);
    // Absorb non-mem and non-branch instructions.
    if (num_exes_before_mem != 0)
    {
        trace_out << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    trace_out << eip << " " << taken << "\n";
    PIN_ReleaseLock(&pinLock);
}

static void memTrace(THREADID t_id,
                     ADDRINT eip,
                     bool is_store,
                     ADDRINT mem_addr,
                     UINT32 payload_size)
{
    if (fast_forwarding) { return; }

    // Lock the print out
    PIN_GetLock(&pinLock, t_id + 1);
    // Absorb non-mem and non-branch instructions.
    if (num_exes_before_mem != 0)
    {
        trace_out << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    trace_out << eip << " ";
    if (is_store) { trace_out << "S "; }
    else { trace_out << "L "; }
    trace_out << mem_addr << "\n";

    PIN_ReleaseLock(&pinLock);
}

#define ROI_BEGIN    (1025)
#define ROI_END      (1026)
void HandleMagicOp(THREADID t_id, ADDRINT op)
{
    switch (op)
    {
        case ROI_BEGIN:
            PIN_GetLock(&pinLock, t_id + 1);
            fast_forwarding = false;
            PIN_ReleaseLock(&pinLock);
            // std::cout << "Captured roi_begin() \n";
            return;
        case ROI_END:
            PIN_GetLock(&pinLock, t_id + 1);
            fast_forwarding = true;
            PIN_ReleaseLock(&pinLock);
            // std::cout << "Captured roi_end() \n";
            return;
    }
}

// "Main" function: decode and simulate the instruction
static void instructionSim(INS ins)
{
    // Count number of instructions.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_THREAD_ID, IARG_END);

    if (INS_IsMemoryRead (ins) || INS_IsMemoryWrite (ins))
    {
        for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); i++)
        {
            if (INS_MemoryOperandIsRead(ins, i))
            {
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)memTrace,
		    IARG_THREAD_ID,
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
                    (AFUNPTR)memTrace,
		    IARG_THREAD_ID,
                    IARG_ADDRINT, INS_Address(ins),
                    IARG_BOOL, TRUE,
                    IARG_MEMORYOP_EA, i,
                    IARG_UINT32, INS_MemoryOperandSize(ins, i),
                    IARG_END);
            }
        }
    }
    else if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
    {
        // Why two calls for a branch?
        // A branch has two path: a taken path and a fall-through path.
        INS_InsertCall(
            ins,
            IPOINT_TAKEN_BRANCH, // Insert a call on the taken edge 
                                 // of the control-flow instruction
            (AFUNPTR)bpTrace,
            IARG_THREAD_ID,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, TRUE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);

        INS_InsertCall(
            ins,
            IPOINT_AFTER, // Insert a call on the fall-through path of 
                          // the last instruction of the instrumented object
            (AFUNPTR)bpTrace,
            IARG_THREAD_ID,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, FALSE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);
    }
    else if (INS_IsXchg(ins) &&
             INS_OperandReg(ins, 0) == REG_RCX &&
             INS_OperandReg(ins, 1) == REG_RCX)
    {
        INS_InsertCall(
            ins,
            IPOINT_BEFORE,
            (AFUNPTR) HandleMagicOp,
            IARG_THREAD_ID,
            IARG_REG_VALUE, REG_ECX,
            IARG_END);
    }
    else
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)nonBranchNorMem, IARG_THREAD_ID, IARG_END);
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

int
main(int argc, char *argv[])
{
    PIN_InitLock(&pinLock);

    PIN_InitSymbols(); // Initialize all the PIN API functions

    // Initialize PIN, e.g., process command line options
    if(PIN_Init(argc,argv))
    {
        return 1;
    }
    assert(!TraceOut.Value().empty());

    trace_out.open(TraceOut.Value().c_str());

    PIN_AddFollowChildProcessFunction(FollowChild, 0);

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}
