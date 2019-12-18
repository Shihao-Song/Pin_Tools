#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

#include "assembly_instructions/assm.h" // Our PMU class

using std::ofstream;

KNOB<std::string> TraceOut(KNOB_MODE_WRITEONCE, "pintool",
    "o", "", "specify output trace file name");

static unsigned num_exes_before_mem = 0;
static void nonMem() // Should disinguish different operations
{
    num_exes_before_mem++;
}

static void memTrace(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (num_exes_before_mem != 0)
    {
        std::cout << num_exes_before_mem << " ";
        num_exes_before_mem = 0;
    }

    if (is_store) { std::cout << "S "; }
    else { std::cout << "L "; }
    std::cout << mem_addr << "\n";
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
                    else
                    {
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
    // assert(!TraceOut.Value().empty());

    // trace_out.open(TraceOut.Value().c_str());

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    IMG_AddInstrumentFunction(Image, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

