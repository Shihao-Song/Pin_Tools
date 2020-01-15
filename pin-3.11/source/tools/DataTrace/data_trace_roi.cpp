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
static Config *cfg;

// Define MMU
#include "include/System/mmu.hh"
static unsigned int NUM_CORES;
typedef System::SingleNode SingleNode;
static SingleNode *mmu;

// Define cache here
static unsigned BLOCK_SIZE;
#include "include/CacheSim/cache.hh"
typedef CacheSimulator::SetWayAssocCache Cache;
static std::vector<Cache*> L1s, L2s, L3s, eDRAMs;

// Define data storage unit
#include "include/Sim/data.hh"
static Data *data_storage;

// We rely on the followings to capture the data to program.
class thread_data_t
{
  public:
    thread_data_t() {}

    bool prev_is_write = false;
    std::vector<ADDRINT> prev_write_addrs;
    std::vector<UINT32> prev_write_sizes;
};

static TLS_KEY tls_key = INVALID_TLS_KEY;

INT32 numThreads = 0;
VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    numThreads++;
    thread_data_t* tdata = new thread_data_t;
    if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE)
    {
        std::cerr << "PIN_SetThreadData failed" << std::endl;
        PIN_ExitProcess(1);
    }
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    thread_data_t* tdata = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, threadIndex));
    delete tdata;
}

PIN_LOCK pinLock;
static uint64_t insn_count = 0; // Track how many instructions we have already instrumented.
static void increCount(THREADID t_id) 
{
    if (fast_forwarding) { return; }	
    PIN_GetLock(&pinLock, t_id + 1);
    ++insn_count;
    PIN_ReleaseLock(&pinLock);
}

static void writeData(THREADID t_id)
{
    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));

    if (t_data->prev_is_write)
    {
        // Lock storage-access
        PIN_GetLock(&pinLock, t_id + 1);
        for (unsigned int i = 0; i < NUM_CORES; i++)
        {
            for (unsigned int j = 0; j < (t_data->prev_write_addrs).size(); j++)
	    {
                UINT32 prev_write_size = (t_data->prev_write_sizes)[j];
                ADDRINT prev_write_addr = (t_data->prev_write_addrs)[j];

                uint8_t new_write_data[prev_write_size];
                PIN_SafeCopy(&new_write_data, (const uint8_t*)prev_write_addr, prev_write_size);

                Request req;
                req.req_type = Request::Request_Type::WRITE;
                req.addr = (uint64_t)prev_write_addr;

                req.core_id = i;
                mmu->va2pa(req);

                data_storage->modifyData((req.addr & ~((uint64_t)BLOCK_SIZE - (uint64_t)1)),
                                         new_write_data,
                                         prev_write_size);
            }
        }
        (t_data->prev_write_addrs).clear();
        (t_data->prev_write_sizes).clear();
        t_data->prev_is_write = false;
        PIN_ReleaseLock(&pinLock);
    }
}

// TODO, simulate store and load.
static void simMemOpr(THREADID t_id, ADDRINT eip, bool is_store, ADDRINT mem_addr, UINT32 payload_size)
{
    if (fast_forwarding) { return; }
   
    thread_data_t* t_data = static_cast<thread_data_t*>(PIN_GetThreadData(tls_key, t_id));

    // Important! Check cross-block situations. Common in Python program.
    std::vector<ADDRINT> addrs;
    std::vector<UINT32> sizes;

    ADDRINT aligned_addr_begin = mem_addr & ~((ADDRINT)BLOCK_SIZE - (ADDRINT)1);
    ADDRINT aligned_addr_end = (mem_addr + (ADDRINT)payload_size - (ADDRINT)1) 
                               & ~((ADDRINT)BLOCK_SIZE - (ADDRINT)1);
    addrs.push_back(mem_addr); 
    for (ADDRINT addr = aligned_addr_begin + BLOCK_SIZE; addr <= aligned_addr_end; addr += BLOCK_SIZE)
    {
        addrs.push_back(addr);
    }

    UINT32 payload_left = payload_size;
    if (addrs.size() == 1)
    {
        sizes.push_back(payload_left);
        payload_left = 0;
    }
    else
    {
        UINT32 chunk_size = (UINT32)BLOCK_SIZE - (UINT32)(mem_addr & ((ADDRINT)BLOCK_SIZE - (ADDRINT)1));
        sizes.push_back(chunk_size);
        payload_left -= chunk_size;
    }
    while (payload_left > 0)
    {
        if (payload_left > BLOCK_SIZE)
        {
            sizes.push_back(BLOCK_SIZE);
            payload_left -= (UINT32)BLOCK_SIZE;
        }
        else
        {
            sizes.push_back(payload_left);
            payload_left = 0;
        }
    }
    assert(sizes.size() == addrs.size());

    Request req;

    req.eip = eip;
    if (is_store)
    {
        req.req_type = Request::Request_Type::WRITE;

        t_data->prev_is_write = true;
        for (auto addr : addrs) { (t_data->prev_write_addrs).push_back(addr); }
        for (auto size : sizes) { (t_data->prev_write_sizes).push_back(size); }
    }
    else
    {
        req.req_type = Request::Request_Type::READ;
    }

    // Lock access-cache
    PIN_GetLock(&pinLock, t_id + 1);
    // std::cout << "Thread " << t_id << " is accessing cache..." << std::endl;
    for (unsigned int i = 0; i < NUM_CORES; i++)
    {
        for (unsigned int j = 0; j < addrs.size(); j++)
        {
            ADDRINT addr = addrs[j];

            req.core_id = i;
            req.addr = (uint64_t)addr;
            mmu->va2pa(req);

            bool hit = L1s[i]->send(req);

	    if (!hit)
            {
                uint8_t data[BLOCK_SIZE];

                ADDRINT aligned_addr = addr & ~((ADDRINT)BLOCK_SIZE - (ADDRINT)1);
                PIN_SafeCopy(&data, (const uint8_t*)aligned_addr, BLOCK_SIZE);

                data_storage->loadData((req.addr & ~((uint64_t)BLOCK_SIZE - (uint64_t)1)),
                                       data,
                                       (unsigned)BLOCK_SIZE);
            }
        }
    }
    PIN_ReleaseLock(&pinLock);
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
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)increCount, IARG_THREAD_ID, IARG_END);

    // Finish up prev store
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)writeData, IARG_THREAD_ID, IARG_END);

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
                    (AFUNPTR)simMemOpr,
		    IARG_THREAD_ID,
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
    std::cout << "Total number of threads = " << numThreads << std::endl;
    Stats stat;
    stat.registerStats("Number of instructions: "
		        + to_string(insn_count));

    for (auto cache : L1s) { cache->registerStats(stat); }
    for (auto cache : L2s) { cache->registerStats(stat); }
    for (auto cache : L3s) { cache->registerStats(stat); }
    for (auto cache : eDRAMs) { cache->registerStats(stat); }

    stat.printf();

    for (auto cache : L1s) { delete cache; }
    for (auto cache : L2s) { delete cache; }
    for (auto cache : L3s) { delete cache; }
    for (auto cache : eDRAMs) { delete cache; }

    delete cfg;
    delete mmu;
    delete data_storage;
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
    assert(!CfgFile.Value().empty());

    trace_out.open(TraceOut.Value().c_str());

    // Parse configuration file
    cfg = new Config(CfgFile.Value());
    NUM_CORES = cfg->num_cores;
    BLOCK_SIZE = cfg->block_size;
    
    // Create MMU
    mmu = new SingleNode(NUM_CORES);

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

    // Data storage
    data_storage = new Data(BLOCK_SIZE);

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
    L3s[0]->setStorageUnit(data_storage);


    // Obtain  a key for TLS storage.
    tls_key = PIN_CreateThreadDataKey(NULL);
    if (tls_key == INVALID_TLS_KEY)
    {
        std::cerr << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit" << std::endl;
        PIN_ExitProcess(1);
    }

    // Register ThreadStart to be called when a thread starts.
    PIN_AddThreadStartFunction(ThreadStart, NULL);

    // Register Fini to be called when thread exits.
    PIN_AddThreadFiniFunction(ThreadFini, NULL);

    // Register Fini to be called when the application exits.
    PIN_AddFiniFunction(Fini, NULL);

    // RTN_AddInstrumentFunction(routineCallback, 0);
    // Simulate each instruction, to eliminate overhead, we are using Trace-based call back.
    // IMG_AddInstrumentFunction(Image, 0);
    TRACE_AddInstrumentFunction(traceCallback, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}

