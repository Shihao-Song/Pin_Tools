#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../Sim/mem_object.hh"
#include "../Sim/util.hh"

#include "tags/cache_tags.hh"
#include "tags/set_assoc_tags.hh"

#include <sstream>
#include <string>

// Let's consider an inclusive cache (easier to manage).
namespace CacheSimulator
{
// TODO, the data-awareness is disabled for some other experiments.
// Should be a template.
template<typename T>
class Cache : public MemObject
{
  public:
    Cache(Config::Cache_Level lev, Config &cfg) : 
        tags(int(lev), cfg),
        level(lev),
        level_name(toString())
    {
        tags.level_str = level_name;
    }

    bool send(Request &request) override
    {
        accesses++; // Emulate a timer for LRU.
        // Collect more stats
        if (request.req_type == Request::Request_Type::READ)
        {
            read_accesses++;
        }
        else
        {
            write_accesses++;
        }

        auto access_info = tags.accessBlock(request.addr,
                                            request.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);

        bool hit = access_info.first;
        Addr aligned_addr = access_info.second;

        if (hit)
        {
            // I don't consider write-back hit as normal cache hits.
            // if (req.req_type != Request::Request_Type::WRITE_BACK)
            // {
                ++num_hits;
            // }
            return true;
        }
        ++num_misses;

        // For any read/write miss, the cache needs to load the block from lower level.
        bool next_level_hit = false;
        if (request.req_type != Request::Request_Type::WRITE_BACK)
        {
            // Instruction loadings are not in the critical path.
            if (request.instr_loading == false)
            {
                ++num_data_loads;
            }
            else
            {
                ++num_instr_loads;
            }

            // Send a loading request to next level.
            if (next_level != nullptr)
            {
                Request req;
                req.instr_loading = request.instr_loading;

                req.addr = aligned_addr; // Address of the missed block.
                req.req_type = Request::Request_Type::READ; // Loading (Always)

                next_level_hit = next_level->send(req);

            }
            /*
            else
            {
                // Output off-chip read traffic
                *trace_out << aligned_addr << " R\n";
            }
            */
        }

        // Insert the missed block
        auto insert_info = tags.insertBlock(aligned_addr,
                                            request.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);
        bool wb_required = insert_info.first;
        Addr victim_addr = insert_info.second;
        
        // Send a write-back request to next level if there is an eviction.
        if (wb_required)
        {
            ++num_evicts;

            if (next_level != nullptr)
            {
                Request req;

                req.addr = victim_addr; // Address of the evicted block.
                req.req_type = Request::Request_Type::WRITE_BACK;

                next_level->send(req); // send to lower levels
            }
            /*
            else
            {
                // Output off-chip write traffic
                std::vector<uint8_t> ori_data;
                std::vector<uint8_t> new_data;

                assert(data != nullptr);
                data->getData(victim_addr, ori_data, new_data);
                assert(ori_data.size() > 0);
                assert(new_data.size() > 0);

                *trace_out << victim_addr << " W " << new_data.size() << " ";

                for (unsigned int i = 0; i < ori_data.size(); i++)
                {
                    *trace_out << int(ori_data[i]) << " ";
                }
                
                for (unsigned int i = 0; i < new_data.size() - 1; i++)
                {
                    *trace_out << int(new_data[i]) << " ";
                }
                *trace_out << int(new_data[new_data.size() - 1]) << "\n";

                // unsigned num_diff = 0;
                // for (unsigned int i = 0; i < ori_data.size(); i++)
                // {
                //     if (ori_data[i] != new_data[i]) { num_diff++; }
                // }
                // *trace_out << num_diff << "\n";
            }
            */
        }
        
        // Invalidate upper levels (inclusive)
        if (victim_addr != MaxAddr)
        {
            for (auto &prev_level : prev_levels) { prev_level->inval(victim_addr); }
        }

        /*
        // Delete data from data storage when there is a (valid) eviction from LLC.
        if (victim_addr != MaxAddr && next_level == nullptr)
        {
            assert(data != nullptr);
            data->deleteData(victim_addr);
        }
        */

	return next_level_hit;
    }

    void inval(uint64_t _addr) override
    {
        // Invalidate the block address
        tags.inval(_addr);
        for (auto &prev_level : prev_levels) { prev_level->inval(_addr); }
    }

    void registerStats(Stats &stats) override
    {
        std::string registeree_name = level_name;
        /*
        // Let's only consider one core so far.
        if (id != -1)
        {
            registeree_name = "Core-" + to_string(id) + "-" + 
                              registeree_name;
        }
        */
         
        stats.registerStats(registeree_name +
                            ": Number of accesses = " + to_string(accesses));
        stats.registerStats(registeree_name +
                            ": Number of read accesses = " + to_string(read_accesses)); 
        stats.registerStats(registeree_name +
                            ": Number of write accesses = " + to_string(write_accesses));
        stats.registerStats(registeree_name +
                            ": Number of hits = " + to_string(num_hits));
        stats.registerStats(registeree_name +
                            ": Number of misses = " + to_string(num_misses));
        double hit_ratio = double(num_hits) / (double(num_hits) + double(num_misses)) * 100;
        stats.registerStats(registeree_name + 
                            ": Hit ratio = " + to_string(hit_ratio) + "%");
        stats.registerStats(registeree_name +
            ": Number of instruction loadings = " + to_string(num_instr_loads));
        stats.registerStats(registeree_name +
            ": Number of data loadings = " + to_string(num_data_loads));
        stats.registerStats(registeree_name +
                            ": Number of evictions = " + 
                            to_string(num_evicts) + "\n");
    }

  protected:
    const Addr MaxAddr = (Addr) - 1;

    Tick accesses = 0; // We are using this for LRU policy.
    
    uint64_t read_accesses = 0;
    uint64_t write_accesses = 0;

    uint64_t num_instr_loads = 0;
    uint64_t num_data_loads = 0;
    uint64_t num_evicts = 0;
    uint64_t num_misses = 0;
    uint64_t num_hits = 0;

    T tags;

    Config::Cache_Level level;
    std::string level_name;
    std::string toString()
    {
        if (level == Config::Cache_Level::L1I)
        {
            return std::string("L1-I");
        }
	else if (level == Config::Cache_Level::L1D)
        {
            return std::string("L1-D");
        }
        else if (level == Config::Cache_Level::L2)
        {
            return std::string("L2");
        }
        else if (level == Config::Cache_Level::L3)
        {
            return std::string("L3");
        }
        else if (level == Config::Cache_Level::eDRAM)
        {
            return std::string("eDRAM");
        }
        else
        {
            return std::string("Unsupported Cache Level");
        }
    }
};

typedef Cache<LRUSetWayAssocTags> SetWayAssocCache;
}
#endif
