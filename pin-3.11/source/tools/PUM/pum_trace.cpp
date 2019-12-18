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

// Extract Processing-using-Memory (PUM) traces
typedef PMU::UINT8 UINT8;
typedef PMU::Operation Operation;

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
        std::cout << "ROWAND ";
    }
    else if (*opr == UINT8(Operation::OR))
    {
        std::cout << "ROWOR ";
    }
    else if (*opr == UINT8(Operation::NOT))
    {
        std::cout << "ROWNOT ";
    }

    unsigned size = pow(2.0, (double)*len);

    std::cout << (uint64_t)src_1 << " ";
    std::cout << (uint64_t)src_2 << " ";
    std::cout << (uint64_t)dest << " ";
    std::cout << size << "\n";

    /*
    UINT8 *src_1_check = (UINT8 *)malloc(size * sizeof(UINT8));
    UINT8 *src_2_check = (UINT8 *)malloc(size * sizeof(UINT8));

    PIN_SafeCopy(src_1_check, src_1, size * sizeof(UINT8));
    PIN_SafeCopy(src_2_check, src_2, size * sizeof(UINT8));

    for (unsigned i = 0; i < size; i++)
    {
        std::cout << int(src_1_check[i]) << " ";
    }
    std::cout << "\n";

    for (unsigned i = 0; i < size; i++)
    {
        std::cout << int(src_2_check[i]) << " ";
    }
    std::cout << "\n";
    
    free(src_1_check);
    free(src_2_check);
    */
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
    // assert(!TraceOut.Value().empty());

    // trace_out.open(TraceOut.Value().c_str());

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    IMG_AddInstrumentFunction(Image, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

