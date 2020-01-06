#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

using std::ofstream;
ofstream trace_out;

KNOB<std::string> TraceOut(KNOB_MODE_WRITEONCE, "pintool",
    "t", "", "specify output trace file name");

static bool fast_forwarding = true;

static bool prev_is_write = false;
static ADDRINT prev_write_addr = 0;
static UINT32 prev_write_size = 0;

static void writeData()
{
    if (prev_is_write)
    {
        uint8_t data[prev_write_size];
        PIN_SafeCopy(&data, (const uint8_t*)prev_write_addr, prev_write_size);
	
	trace_out << "S ";
        trace_out << prev_write_addr << " ";

        for (unsigned int i = 0; i < prev_write_size - 1; i++)
        {
            trace_out << int(data[i]) << " ";
        }
        trace_out << int(data[prev_write_size - 1]);
        trace_out << "\n";

        prev_is_write = false;
    }
}

static unsigned num_exes_before_mem = 0;
static void nonMem() // Should disinguish different operations in the future
{
    if (fast_forwarding) { return; }
    writeData(); // Finish up prev store insturction
    num_exes_before_mem++;
}

static void memTrace(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (fast_forwarding) { return; }
    writeData(); // Finish up prev store insturction

    if (num_exes_before_mem != 0)
    {
        trace_out << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    if (is_store) 
    {
        prev_is_write = true;
        prev_write_addr = mem_addr;
        prev_write_size = payload_size;
    }
    else
    {
        uint8_t data[payload_size];
        PIN_SafeCopy(&data, (const uint8_t*)mem_addr, payload_size);
        trace_out << "L ";
        trace_out << mem_addr << " ";
        for (unsigned int i = 0; i < payload_size - 1; i++)
        {
            trace_out << int(data[i]) << " ";
        }
        trace_out << int(data[payload_size - 1]);
        trace_out << "\n";
    }
}

#define ROI_BEGIN    (1025)
#define ROI_END      (1026)
void HandleMagicOp(ADDRINT op)
{
    switch (op)
    {
        case ROI_BEGIN:
            fast_forwarding = false;
            // std::cout << "Captured roi_begin() \n";
            return;
        case ROI_END:
            fast_forwarding = true;
            // std::cout << "Captured roi_end() \n";
            return;
    }
}

// "Main" function: decode and simulate the instruction
static void instructionSim(INS ins)
{
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
                    IARG_ADDRINT, INS_Address(ins),
                    IARG_BOOL, TRUE,
                    IARG_MEMORYOP_EA, i,
                    IARG_UINT32, INS_MemoryOperandSize(ins, i),
                    IARG_END);
            }
        }
    }
    else if (INS_IsXchg(ins) && 
             INS_OperandReg(ins, 0) == REG_RCX && 
             INS_OperandReg(ins, 1) == REG_RCX)
    {
        INS_InsertCall(
            ins,
            IPOINT_BEFORE, 
            (AFUNPTR) HandleMagicOp,
            IARG_REG_VALUE, REG_ECX,
            IARG_END);
    }
    else
    {
        // Everything else except memory operations.
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)nonMem, IARG_END);
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
    PIN_InitSymbols(); // Initialize all the PIN API functions

    // Initialize PIN, e.g., process command line options
    if(PIN_Init(argc,argv))
    {
        return 1;
    }
    assert(!TraceOut.Value().empty());

    trace_out.open(TraceOut.Value().c_str());

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

