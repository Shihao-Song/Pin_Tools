#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

#include "assembly_instructions/assm.h" // Our PMU class

using std::ofstream;
ofstream trace_out;

KNOB<std::string> TraceOut(KNOB_MODE_WRITEONCE, "pintool",
    "t", "", "specify output trace file name");

ofstream data_out;
KNOB<std::string> DataOut(KNOB_MODE_WRITEONCE, "pintool",
    "d", "", "specify output data file name");

// Extract Processing-using-Memory (PUM) traces
typedef PUM::UINT8 UINT8;
typedef PUM::Operation Operation;

static void pumTrace(CONTEXT *ctxt)
{
    UINT8 **sp = (UINT8 **)PIN_GetContextReg(ctxt, REG_STACK_PTR);

    UINT8 *opr = *sp;
    UINT8 *len = *(sp + 1);
    UINT8 *dest = *(sp + 2);
    UINT8 *src_2 = *(sp + 3);
    UINT8 *src_1 = *(sp + 4);

    if (*opr == UINT8(Operation::AND))
    {
        trace_out << "ROWAND ";
    }
    else if (*opr == UINT8(Operation::OR))
    {
        trace_out << "ROWOR ";
    }
    else if (*opr == UINT8(Operation::NOT))
    {
        trace_out << "ROWNOT ";
    }

    unsigned size = pow(2.0, (double)*len);

    trace_out << (uint64_t)src_1 << " ";
    trace_out << (uint64_t)src_2 << " ";
    trace_out << (uint64_t)dest << " ";
    trace_out << size << "\n";

    // Extrace data (for src_1 and src_2)
    UINT8 *src_1_check = (UINT8 *)malloc(size * sizeof(UINT8));
    UINT8 *src_2_check = (UINT8 *)malloc(size * sizeof(UINT8));

    PIN_SafeCopy(src_1_check, src_1, size * sizeof(UINT8));
    if (*opr != UINT8(Operation::NOT))
    {
        PIN_SafeCopy(src_2_check, src_2, size * sizeof(UINT8));
    }

    data_out << (uint64_t)src_1 << " "
             << size << " ";
    for (unsigned i = 0; i < size; i++)
    {
        data_out << int(src_1_check[i]) << " ";
    }
    data_out << "\n";

    if (*opr != UINT8(Operation::NOT))
    {
        data_out << (uint64_t)src_2 << " "
                 << size << " ";
        for (unsigned i = 0; i < size; i++)
        {
            data_out << int(src_2_check[i]) << " ";
        }
        data_out << "\n";
    }

    free(src_1_check);
    free(src_2_check);
}

VOID Image(IMG img, VOID *v)
{
    // Walk through the symbols in the symbol table.
    for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym))
    {
        std::string undFuncName = 
            PIN_UndecorateSymbolName(SYM_Name(sym), UNDECORATION_NAME_ONLY);

        if (undFuncName.find("ROW") != std::string::npos)
        {
            RTN rtn = RTN_FindByAddress(IMG_LowAddress(img) + SYM_Value(sym));
            
            if (RTN_Valid(rtn))
            {
                RTN_Open(rtn);
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                {
                    if (INS_IsHalt(ins))
                    {
                        INS_InsertCall(ins, IPOINT_BEFORE,
                                      (AFUNPTR)pumTrace,
                                       IARG_CONST_CONTEXT,
                                       IARG_END);
                        INS_Delete(ins);
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
    assert(!DataOut.Value().empty());

    trace_out.open(TraceOut.Value().c_str());
    data_out.open(DataOut.Value().c_str());

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    IMG_AddInstrumentFunction(Image, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

