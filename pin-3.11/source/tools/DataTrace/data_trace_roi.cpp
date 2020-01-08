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
    "t", "", "specify output trace file name");

static bool fast_forwarding = true; // Fast-forwarding mode?

// Define config here
KNOB<std::string> CfgFile(KNOB_MODE_WRITEONCE, "pintool",
    "c", "", "specify system configuration file name");
#include "include/Sim/config.hh"
Config *cfg;

// Define MMU
#include "include/System/mmu.hh"
static unsigned int NUM_CORES;
typedef System::MMU MMU;
MMU *mmu;

// Define cache here
#include "include/CacheSim/cache.hh"
typedef CacheSimulator::SetWayAssocCache Cache;
std::vector<Cache*> L1s, L2s, L3s, eDRAMs;

// Trace how many instructions executed.
static UINT64 insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount() { if (fast_forwarding) { return; } ++insn_count; }

// We rely on the followings to capture the data to program.
static bool prev_is_write = false;
static ADDRINT prev_write_addr = 0;
static UINT32 prev_write_size = 0;
// TODO, copy the new data to cache-line.
static void writeData()
{
    if (prev_is_write)
    {
        uint8_t data[prev_write_size];
        PIN_SafeCopy(&data, (const uint8_t*)prev_write_addr, prev_write_size);
	
	trace_out << "S ";
        trace_out << prev_write_addr << " " << prev_write_size << " ";

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

// TODO, simulate store and load.
static void memTrace(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (fast_forwarding) { return; }
    writeData(); // Finish up prev store insturction

    Request req;

    req.eip = eip;
    if (is_store)
    {
        req.req_type = Request::Request_Type::WRITE;
    }
    else
    {
        req.req_type = Request::Request_Type::READ;
    }
    req.addr = mem_addr;

    for (unsigned i = 0; i < 1; i++)
    {
        req.core_id = i;
        mmu->va2pa(req);

        L1s[i]->send(req);
    }

    /*
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
        trace_out << mem_addr << " " << payload_size << " ";
        for (unsigned int i = 0; i < payload_size - 1; i++)
        {
            trace_out << int(data[i]) << " ";
        }
        trace_out << int(data[payload_size - 1]);
        trace_out << "\n";
    }
    */
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
    // Count number of instructions.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_END);

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
    assert(!CfgFile.Value().empty());

    trace_out.open(TraceOut.Value().c_str());

    // Parse configuration file
    cfg = new Config(CfgFile.Value());
    NUM_CORES = cfg->num_cores;

    // Create MMU
    mmu = new MMU(NUM_CORES);

    // Create caches
    if (cfg->caches[int(Config::Cache_Level::L1D)].valid)
    {
        for (unsigned i = 0; i < NUM_CORES; i++)
        {
            L1s.emplace_back(new Cache(Config::Cache_Level::L1D, *cfg));
            L1s[i]->setId(i);
        }
    }

    if (cfg->caches[int(Config::Cache_Level::L2)].valid)
    {
        if (cfg->caches[int(Config::Cache_Level::L2)].shared)
        {
            L2s.emplace_back(new Cache(Config::Cache_Level::L2, *cfg));
        }
        else
        {
            for (unsigned i = 0; i < NUM_CORES; i++)
            {
                L2s.emplace_back(new Cache(Config::Cache_Level::L2, *cfg));
                L2s[i]->setId(i);
            }
        }
    }

    if (cfg->caches[int(Config::Cache_Level::L3)].valid)
    {
        if (cfg->caches[int(Config::Cache_Level::L3)].shared)
        {
            L3s.emplace_back(new Cache(Config::Cache_Level::L3, *cfg));
        }
        else
        {
            for (unsigned i = 0; i < NUM_CORES; i++)
            {
                L3s.emplace_back(new Cache(Config::Cache_Level::L3, *cfg));
                L3s[i]->setId(i);
            }
        }
    }

    // Power-9 setup
    assert(L2s.size() == 1);
    for (unsigned i = 0; i < NUM_CORES; i++)
    {
        L1s[i]->setNextLevel(L2s[0]);
    }

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

