#include <iostream>
#include <string>

#include "pin.H"
#include "src/simulation.h"

// For creating directory.
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Profiling Tool: 
 * (1) Divide the entire program into intervals (100M instructions);
 * (2) For each interval, record the followings (not limited):
 *     1) Number of (new) first-touch instructions; (Finished)
 *     2) Number of (new) touched pages/page faults; (Finished)
 *     3) Number of correct branch predictions; (Finished)
 *     4) Number of in-correct branch predictions; (Finished)
 *     5) Number of cache hits (all cache levels); (Finished)
 *     6) Number of cache misses (all cache levels); (Finished)
 *     7) Number of cache loads (all cache levels); (Finished)
 *     8) Number of cache evictions (all cache levels); (Finished)
 * */
// KNOB<std::string> (KNOB_MODE_WRITEONCE, "pintool",
//    "p", "", "specify profiling output directory");
KNOB<std::string> CfgFile(KNOB_MODE_WRITEONCE, "pintool",
    "i", "", "specify system configuration file name");

// Simulation components
static unsigned NUM_CORES = 1;
//static const Count SKIP = 10000000000;
//static const Count END = 20000000000;

static const Count SKIP = 10000000000;
static const Count PROFILING_LIMIT = 10000000;
static const Count INFERENCE_LIMIT = 100000000;
static const Count NUM_RUNS = 100;
//static const Count END = 3000000000;

//static const Count SKIP = 100000000;
//static const Count END = 1000000000;

Config *cfg;

BP::Branch_Predictor *bp;

System::MMU *mmu;

std::vector<MemObject*> l1;
std::vector<MemObject*> l2;
std::vector<MemObject*> l3;
std::vector<MemObject*> eDRAM;

static bool start_sim = false;
static unsigned cur_run = 0;
// static bool end_sim = false; 
static UINT64 insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount() { ++insn_count;
                           if (insn_count < SKIP) { return; }
                           if (insn_count == SKIP) { start_sim = true; return; }
                           if (cur_run == 0 && insn_count == (SKIP + PROFILING_LIMIT))
                           {
                               std::string page_info_out = "page_profiling/10M.csv";
                               mmu->printPageInfo(page_info_out);
                               mmu->reInitialize();
                               cur_run++;

                               return;
                           }

                           if (cur_run > 0 && cur_run <= NUM_RUNS && 
                               insn_count == (SKIP + PROFILING_LIMIT + 
                                              cur_run * INFERENCE_LIMIT))
                           {
                               // Print phase stats.
                               Stats stat;
                               stat.registerStats("Number of instructions: " 
                                                  + to_string(insn_count));

                               bp->registerStats(stat);
                               mmu->registerStats(stat);

                               for (auto cache : l1) { cache->registerStats(stat); }
                               for (auto cache : l2) { cache->registerStats(stat); }
                               for (auto cache : l3) { cache->registerStats(stat); }
                               for (auto cache : eDRAM) { cache->registerStats(stat); }
                               
                               // Print page profilings
                               std::string page_info_out = "page_profiling/" + 
                                                           to_string(cur_run) + "00M.csv";
                               mmu->printPageInfo(page_info_out);
                               // Print phase stats
                               std::string phase_stats_out = "phase_stats/" +
                                                             to_string(cur_run) + "00M.csv";
                               stat.outputStats(phase_stats_out);

                               bp->reInitialize(); 
                               mmu->reInitialize();
                               for (auto cache : l1) { cache->reInitialize(); }
 
                               cur_run++;
                               return;
                           }

                           if (cur_run > NUM_RUNS)
                           {
                               exit(0);
                           }
}

// Function: branch predictor simulation
static void bpSim(ADDRINT eip, BOOL taken, ADDRINT target)
{
    if (!start_sim) { return; }
    
    Instruction instr;

    instr.setPC(eip);
    instr.setBranch();
    instr.setTaken(taken);

    // Sending to the branch predictor.
    bp->predict(instr, insn_count); // I'm using insn_count as time-stamp.
}

// Function: memory access simulation
static void memAccessSim(ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (!start_sim) { return; }

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

    for (unsigned i = 0; i < NUM_CORES; i++)
    {
        req.core_id = i;
        mmu->va2pa(req);

        l1[i]->send(req);
    }
}

//Function: Other function
static void nonBranchNorMem()
{
//    std::cout << insn_count << ": E\n";
}

// "Main" function: decode and simulate the instruction
static void instructionSim(INS ins)
{
    // Step one, increment instruction count.
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_END);

    // Step two, decode and simulate instruction.
    if (INS_IsBranch(ins) && INS_HasFallThrough(ins))
    {
        // Why two calls for a branch?
        // A branch has two path: a taken path and a fall-through path.
        INS_InsertCall(
            ins,
            IPOINT_TAKEN_BRANCH, // Insert a call on the taken edge 
                                 // of the control-flow instruction
            (AFUNPTR)bpSim,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, TRUE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);

        INS_InsertCall(
            ins,
            IPOINT_AFTER, // Insert a call on the fall-through path of 
                          // the last instruction of the instrumented object
            (AFUNPTR)bpSim,
            IARG_ADDRINT, INS_Address(ins),
            IARG_BOOL, FALSE,
            IARG_BRANCH_TARGET_ADDR,
            IARG_END);
    }
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
    else
    {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)nonBranchNorMem, IARG_END);
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

static void printResults(int dummy, VOID *p)
{
    std::cout << "Total number of instructions: " << insn_count << "\n";
}

using std::ofstream;
int
main(int argc, char *argv[])
{
    PIN_InitSymbols(); // Initialize all the PIN API functions

    // Initialize PIN, e.g., process command line options
    if(PIN_Init(argc,argv))
    {
        return 1;
    }
    assert(!CfgFile.Value().empty());

    // Read configuration files
    cfg = new Config(CfgFile.Value());
    NUM_CORES = cfg->num_cores;

    // Initialize an MMU
    mmu = new System::MMU(NUM_CORES);

    // Initialize memory system
    assert(cfg->caches[int(Config::Cache_Level::L1D)].valid);
    ofstream prof_cfg("profiling.cfg");
    if (cfg->caches[int(Config::Cache_Level::L1D)].valid)
    {
        for (unsigned i = 0; i < NUM_CORES; i++)
        {
            l1.emplace_back(new 
                CacheSimulator::SetWayAssocCache(Config::Cache_Level::L1D, *cfg));
            l1[i]->setId(i);
        }

        prof_cfg << "L1D-Cache is enabled. \n";
        prof_cfg << "L1D-Cache size (KB): "
                 << cfg->caches[int(Config::Cache_Level::L1D)].size << "\n";
        if (cfg->caches[int(Config::Cache_Level::L1D)].assoc != -1)
        {
            prof_cfg << "L1D-Cache assoc: "
                     << cfg->caches[int(Config::Cache_Level::L1D)].assoc << "\n";
        }
        else
        {
            prof_cfg << "L1D-Cache is fully-associative.\n";
        }

        if (!cfg->caches[int(Config::Cache_Level::L1D)].shared)
        { prof_cfg << "L1D-Cache is private (per core). \n\n"; }
        else { prof_cfg << "L1D-Cache is shared. \n\n"; }
    }

    if (cfg->caches[int(Config::Cache_Level::L2)].valid)
    {
        if (cfg->caches[int(Config::Cache_Level::L2)].shared)
        {
            l2.emplace_back(new 
                CacheSimulator::SetWayAssocCache(Config::Cache_Level::L2, *cfg));
        }
        else
        {
            for (unsigned i = 0; i < NUM_CORES; i++)
            {
                l2.emplace_back(new 
                    CacheSimulator::SetWayAssocCache(Config::Cache_Level::L2, *cfg));
                l2[i]->setId(i);
            }
        }

        prof_cfg << "L2-Cache is enabled. \n";
        prof_cfg << "L2-Cache size (KB): "
                 << cfg->caches[int(Config::Cache_Level::L2)].size << "\n";
        if (cfg->caches[int(Config::Cache_Level::L2)].assoc != -1)
        {
            prof_cfg << "L2-Cache assoc: "
                     << cfg->caches[int(Config::Cache_Level::L2)].assoc << "\n";
        }
        else
        {
            prof_cfg << "L2-Cache is fully-associative.\n";
        }

        if (!cfg->caches[int(Config::Cache_Level::L2)].shared)
        { prof_cfg << "L2-Cache is private (per core). \n\n"; }
        else { prof_cfg << "L2-Cache is shared. \n\n"; }
    }

    if (cfg->caches[int(Config::Cache_Level::L3)].valid)
    {
        if (cfg->caches[int(Config::Cache_Level::L3)].shared)
        {
            l3.emplace_back(new 
                CacheSimulator::SetWayAssocCache(Config::Cache_Level::L3, *cfg));
        }
        else
        {
            for (unsigned i = 0; i < NUM_CORES; i++)
            {
                l3.emplace_back(new 
                    CacheSimulator::SetWayAssocCache(Config::Cache_Level::L3, *cfg));
                l3[i]->setId(i);
            }
        }

        prof_cfg << "L3-Cache is enabled. \n";
        prof_cfg << "L3-Cache size (KB): "
                 << cfg->caches[int(Config::Cache_Level::L3)].size << "\n";
        if (cfg->caches[int(Config::Cache_Level::L3)].assoc != -1)
        {
            prof_cfg << "L3-Cache assoc: "
                     << cfg->caches[int(Config::Cache_Level::L3)].assoc << "\n";
        }
        else
        {
            prof_cfg << "L3-Cache is fully-associative.\n";
        }

        if (!cfg->caches[int(Config::Cache_Level::L3)].shared)
        { prof_cfg << "L3-Cache is private (per core). \n\n"; }
        else { prof_cfg << "L3-Cache is shared. \n\n"; }
    }
    if (cfg->caches[int(Config::Cache_Level::eDRAM)].valid)
    {
        if (cfg->caches[int(Config::Cache_Level::eDRAM)].shared)
        {
            // TODO, I assume all the eDRAMs are FA and write-only.
            eDRAM.emplace_back(new 
                CacheSimulator::WOFACache(Config::Cache_Level::eDRAM, *cfg));
        }
        else
        {
            for (unsigned i = 0; i < NUM_CORES; i++)
            {
                eDRAM.emplace_back(new 
                    CacheSimulator::FACache(Config::Cache_Level::eDRAM, *cfg));
                eDRAM[i]->setId(i);
            }
        }

        prof_cfg << "eDRAM-Cache is enabled. \n";
        prof_cfg << "eDRAM-Cache size (KB): "
                 << cfg->caches[int(Config::Cache_Level::eDRAM)].size << "\n";
        if (cfg->caches[int(Config::Cache_Level::eDRAM)].assoc != -1)
        {
            prof_cfg << "eDRAM-Cache assoc: "
                     << cfg->caches[int(Config::Cache_Level::eDRAM)].assoc << "\n";
        }
        else
        {
            prof_cfg << "eDRAM-Cache is fully-associative.\n";
        }

        if (!cfg->caches[int(Config::Cache_Level::eDRAM)].shared)
        { prof_cfg << "eDRAM-Cache is private (per core). \n\n"; }
        else { prof_cfg << "eDRAM-Cache is shared. \n\n"; }
    }
    prof_cfg.close();

    // TODO, Connecting all levels of caches. Still testing CacheSim...
    for (unsigned i = 0; i < NUM_CORES; i++)
    {
        l1[i]->setNextLevel(l2[0]);
    }
    // l1[0]->setNextLevel(l2[0]);
    // l2[0]->setNextLevel(eDRAM[0]);

    mkdir("page_profiling", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    mkdir("phase_stats", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // Let's keep tournament fixed.
    bp = new BP::Two_Bit_Local();

    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    TRACE_AddInstrumentFunction(traceCallback, 0);

    // Print stats
    PIN_AddFiniFunction(printResults, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

