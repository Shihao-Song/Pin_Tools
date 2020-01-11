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

static bool fast_forwarding = true; // Fast-forwarding mode?

// Define config here
KNOB<std::string> CfgFile(KNOB_MODE_WRITEONCE, "pintool",
    "c", "", "specify system configuration file name");
#include "include/Sim/config.hh"
Config *cfg;

// Define MMU
#include "include/System/mmu.hh"
static unsigned int NUM_CORES;
typedef System::SingleNode MMU;
MMU *mmu;

// Define cache here
static unsigned BLOCK_SIZE;
#include "include/CacheSim/cache.hh"
typedef CacheSimulator::SetWayAssocCache Cache;
std::vector<Cache*> L1s, L2s, L3s, eDRAMs;

// Trace how many instructions executed.
static const uint64_t SKIP = 10000000000;
static uint64_t insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount() { ++insn_count; if (insn_count == SKIP) { fast_forwarding = false; } 
                                         if (insn_count == SKIP + 250000000)
                                         {
                                             Stats stat;
                                             stat.registerStats("Number of instructions: "
                                                                + to_string(insn_count));

                                             for (auto cache : L1s) { cache->registerStats(stat); }
                                             for (auto cache : L2s) { cache->registerStats(stat); }
                                             for (auto cache : L3s) { cache->registerStats(stat); }
                                             //for (auto cache : eDRAMs) { cache->registerStats(stat); }

                                             stat.printf();
					     exit(0);
                                         }}

// We rely on the followings to capture the data to program.
static bool prev_is_write = false;
static ADDRINT prev_write_addr = 0;
static UINT32 prev_write_size = 0;
// TODO, copy the new data to cache-line.
static void writeData()
{
    if (prev_is_write)
    {
        uint8_t new_data[prev_write_size];
        PIN_SafeCopy(&new_data, (const uint8_t*)prev_write_addr, prev_write_size);

        Request req;
        req.req_type = Request::Request_Type::WRITE;
        req.addr = (uint64_t)prev_write_addr;

        for (unsigned i = 0; i < NUM_CORES; i++)
        {
            req.core_id = i;
            mmu->va2pa(req);
        
            L1s[i]->modifyBlock(req.addr, new_data, prev_write_size);
        }

        prev_is_write = false;
    }
}

static void nonMem() // Should disinguish different operations in the future
{
    if (fast_forwarding) { return; }
    writeData(); // Finish up prev store insturction
}

// TODO, simulate store and load.
static void simMemOpr(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (fast_forwarding) { return; }
    writeData(); // Finish up prev store insturction

    Request req;

    req.eip = eip;
    if (is_store)
    {
        req.req_type = Request::Request_Type::WRITE;

        prev_is_write = true;
        prev_write_addr = mem_addr;
        prev_write_size = payload_size;
    }
    else
    {
        req.req_type = Request::Request_Type::READ;
    }
    req.addr = (uint64_t)mem_addr;

    for (unsigned i = 0; i < NUM_CORES; i++)
    {
        req.core_id = i;
        mmu->va2pa(req);

        bool hit = L1s[i]->send(req);
        if (!hit)
        {
            uint8_t data[BLOCK_SIZE];

            ADDRINT aligned_addr = mem_addr & ~((ADDRINT)BLOCK_SIZE - (ADDRINT)1);
            PIN_SafeCopy(&data, (const uint8_t*)aligned_addr, BLOCK_SIZE);
            L1s[i]->loadBlock(req.addr, data, BLOCK_SIZE); // Load the entire block.

            // if (!is_store) { std::cout << "\n"; }
        }
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
                    (AFUNPTR)simMemOpr,
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
                    (AFUNPTR)simMemOpr,
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
    BLOCK_SIZE = cfg->block_size;
    
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
        L2s[0]->setPrevLevel(L1s[i]); // Should be a vector
    }

    L2s[0]->setNextLevel(L3s[0]);
    L3s[0]->setPrevLevel(L2s[0]);
    L3s[0]->traceOutput(&trace_out);

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

