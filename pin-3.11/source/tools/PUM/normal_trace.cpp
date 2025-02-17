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

// TODO, make the followings as vector and see what happens.
static bool prev_is_write = false;
static ADDRINT prev_write_addr = 0;
static UINT32 prev_write_size = 0;

static void getData()
{
    if (prev_is_write)
    {
        uint8_t data[prev_write_size];
        PIN_SafeCopy(&data, (const uint8_t*)prev_write_addr, prev_write_size);

        trace_out << "S ";
        trace_out << prev_write_addr << " ";

	double *real = (double *)data;
	trace_out << *real << "\n";
        /*
        for (unsigned int i = 0; i < prev_write_size - 1; i++)
        {
            trace_out << int(data[i]) << " ";
        }
        trace_out << int(data[prev_write_size - 1]);
        trace_out << "\n";
        */
        prev_is_write = false;
    }
}

static unsigned num_exes_before_mem = 0;
static void nonMem() // Should disinguish different operations
{
    num_exes_before_mem++;
}

static void memTrace(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (num_exes_before_mem != 0)
    {
        //trace_out << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    if (is_store)
    {
        uint8_t data[payload_size];
        PIN_SafeCopy(&data, (const uint8_t*)mem_addr, payload_size);
        trace_out << "S ";
        trace_out << mem_addr << " ";

	double *real = (double *)data;
	trace_out << *real << "\n";
        /*
        for (unsigned int i = 0; i < payload_size - 1; i++)
        {
            trace_out << int(data[i]) << " ";
        }
        trace_out << int(data[payload_size - 1]);
        trace_out << "\n";
        */
        prev_is_write = true;
        prev_write_addr = mem_addr;
        prev_write_size = payload_size;
    }

    /*
    if (num_exes_before_mem != 0)
    {
        trace_out << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    if (is_store) { trace_out << "S "; }
    else { trace_out << "L "; }
    trace_out << mem_addr << "\n";
    */
}

VOID Image(IMG img, VOID *v)
{
    // Walk through the symbols in the symbol table.
    for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym))
    {
        std::string undFuncName = 
            PIN_UndecorateSymbolName(SYM_Name(sym), UNDECORATION_NAME_ONLY);

        if (undFuncName.find("regionOfInterest") != std::string::npos)
        {
            RTN rtn = RTN_FindByAddress(IMG_LowAddress(img) + SYM_Value(sym));
            
            if (RTN_Valid(rtn))
            {
                RTN_Open(rtn);
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)getData, IARG_END);

                    if (INS_IsMemoryRead (ins) || INS_IsMemoryWrite (ins))
                    {
                        for (unsigned int i = 0; i < INS_MemoryOperandCount(ins); i++)
                        {
                            /*
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
                            */

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
                    else
                    {
                        // Everything else besides memory operations.
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)nonMem, IARG_END);
                    }
                }
            }
            RTN_Close(rtn);
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

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    IMG_AddInstrumentFunction(Image, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

