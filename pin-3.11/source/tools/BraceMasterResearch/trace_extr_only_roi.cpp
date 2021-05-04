#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

// output
using std::ofstream;
ofstream stats_out;
KNOB<std::string> StatsOut(KNOB_MODE_WRITEONCE, "pintool",
    "o", "", "specify output stats file name");

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

PIN_LOCK pinLock;

static bool entering_roi = false;

static uint64_t insn_count = 0; // Track how many instructions we have already instrumented.

static void increCount(THREADID t_id) 
{
    // When entered into ROI, skip the first 1 billion of instructions then extract the next 
    // 1 billion of instructions.

    if (!entering_roi) { return; }

    PIN_GetLock(&pinLock, t_id + 1);

    ++insn_count; // Increment

    PIN_ReleaseLock(&pinLock);
}

/*
static void printInst(INS ins)
{
    if (!entering_roi) { return; }

    std::cout << INS_Disassemble(ins) << std::endl;
}
*/
// Thread local data
class thread_data_t
{
  public:
    thread_data_t() {}

    int tid = -1;

    // UINT64 num_add_or_sub = 0;
    // UINT64 num_mul_or_div = 0;
    // UINT64 num_arith = 0;
    UINT64 num_stores = 0;
    UINT64 num_loads = 0;
    UINT64 num_add = 0;
    UINT64 num_mul = 0;
};

static TLS_KEY tls_key = INVALID_TLS_KEY;

INT32 numThreads = 0;
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    numThreads++;
    thread_data_t* tdata = new thread_data_t;
    tdata->tid = threadid;

    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
    {
        std::cerr << "PIN_SetThreadData failed" << std::endl;
        PIN_ExitProcess(1);
    }

    std::cerr << "Thread " << threadid << " is created" << std::endl;
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    thread_data_t* tdata = 
        static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, threadIndex));

    PIN_GetLock(&pinLock, tdata->tid + 1);
    stats_out << insn_count << " " 
              << tdata->num_add << " "
              << tdata->num_mul << " "
              << tdata->num_loads << " "
              << tdata->num_stores << std::endl;

    PIN_ReleaseLock(&pinLock);
    delete tdata;
}

static void memOps(THREADID t_id,
                   ADDRINT eip,
                   bool is_store,
                   ADDRINT mem_addr,
                   UINT32 payload_size)
{
    if (!entering_roi) { return; }

    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));

    if (is_store) { t_data->num_stores += 1; }
    else { t_data->num_loads += 1; }
}
/*
static void arithOps(THREADID t_id)
{
    if (!entering_roi) { return; }

    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));
    t_data->num_arith += 1;
}
*/
/*
static void addOrSubOps(THREADID t_id)
{
    if (!entering_roi) { return; }

    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));
    t_data->num_add_or_sub += 1;
}

*/
static void mulOps(THREADID t_id)
{
    if (!entering_roi) { return; }

    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));
    t_data->num_mul += 1;
}
static void addOps(THREADID t_id)
{
    if (!entering_roi) { return; }

    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));
    t_data->num_add += 1;
}

#define ROI_BEGIN    (1025)
#define ROI_END      (1026)
void HandleMagicOp(THREADID t_id, ADDRINT op)
{
    switch (op)
    {
        case ROI_BEGIN:
            PIN_GetLock(&pinLock, t_id + 1);
            entering_roi = true;
            PIN_ReleaseLock(&pinLock);
            std::cout << "[PINTOOL] Captured roi_begin()" << std::endl;
            return;
        case ROI_END:
            PIN_GetLock(&pinLock, t_id + 1);
            entering_roi = false;
            PIN_ReleaseLock(&pinLock);
            std::cout << "[PINTOOL] Captured roi_end()" << std::endl;
            return;
    }
}

// "Main" function: decode and simulate the instruction
static void instructionSim(INS ins)
{
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_THREAD_ID, IARG_END);
    // INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)printInst, ins, IARG_END);
    // if (entering_roi)
    // {
    //     std::cout << INS_Disassemble(ins) << std::endl;
    // }

    if (INS_IsMemoryRead (ins) || INS_IsMemoryWrite (ins))
    {
        for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); i++)
        {
            if (INS_MemoryOperandIsRead(ins, i))
            {
                INS_InsertPredicatedCall(
                    ins,
                    IPOINT_BEFORE,
                    (AFUNPTR)memOps,
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
                    (AFUNPTR)memOps,
		    IARG_THREAD_ID,
                    IARG_ADDRINT, INS_Address(ins),
                    IARG_BOOL, TRUE,
                    IARG_MEMORYOP_EA, i,
                    IARG_UINT32, INS_MemoryOperandSize(ins, i),
                    IARG_END);
            }
        }
    }
    /*
    // arithmetic operations
    else if (INS_Category(ins) == XED_CATEGORY_BINARY)
    {
        INS_InsertCall(ins, 
                       IPOINT_BEFORE,
                       (AFUNPTR)arithOps,
                       IARG_THREAD_ID, 
                       IARG_END);
    }
    */    
    /*
    else if ((INS_Opcode(ins) == XED_ICLASS_ADD) ||
             (INS_Opcode(ins) == XED_ICLASS_SUB))
    {
        INS_InsertCall(ins, 
                       IPOINT_BEFORE,
                       (AFUNPTR)addOrSubOps,
                       IARG_THREAD_ID, 
                       IARG_END);
    }
    */  
    else if ((INS_Opcode(ins) == XED_ICLASS_MUL) ||
             (INS_Opcode(ins) == XED_ICLASS_IMUL) ||
             (INS_Opcode(ins) == XED_ICLASS_FMUL))
    {
        
        INS_InsertCall(ins, 
                       IPOINT_BEFORE,
                       (AFUNPTR)mulOps,
                       IARG_THREAD_ID, 
                       IARG_END);
        
    }
    else if ((INS_Opcode(ins) == XED_ICLASS_ADD) ||
             (INS_Opcode(ins) == XED_ICLASS_FADD) ||
             (INS_Opcode(ins) == XED_ICLASS_FIADD))
    {
        
        INS_InsertCall(ins, 
                       IPOINT_BEFORE,
                       (AFUNPTR)addOps,
                       IARG_THREAD_ID, 
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

VOID Fini(INT32 code, VOID *v)
{
    stats_out.close();
}

int
main(int argc, char *argv[])
{
    PIN_InitLock(&pinLock);
    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY)
    {
        std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" 
                  << std::endl;
        PIN_ExitProcess(1);
    }

    PIN_InitSymbols(); // Initialize all the PIN API functions

    // Initialize PIN, e.g., process command line options
    if(PIN_Init(argc,argv))
    {
        return 1;
    }
    assert(!StatsOut.Value().empty());

    stats_out.open(StatsOut.Value().c_str());

    // std::cerr << roi_skippings << std::endl;
    // exit(0);

    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);

    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    PIN_AddFollowChildProcessFunction(FollowChild, 0);

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    PIN_AddFiniFunction(Fini, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}
