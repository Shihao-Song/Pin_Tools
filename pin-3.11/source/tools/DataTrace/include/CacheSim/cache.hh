#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../Sim/mem_object.hh"
#include "../Sim/util.hh"

#include "tags/cache_tags.hh"
#include "tags/set_assoc_tags.hh"

#include <sstream>
#include <string>

// Let's consider an inclusive cache (easier to manage).
//    (1) Every-time there is an eviction, the same block address from higher level must be invalidated;
//    (2) Considering data-awareness, 1) all the new data needs to be sent to all levels; in this
//                                        situation, the cache block must be in all levels of caches;
//                                    2) the original data needs to be sent to all levels only when
//                                        cache misses from all levels;
//                                    3) when bringing block to higher level (hit in lower level),
//                                        data from lower level should be copied.
namespace CacheSimulator
{
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

    bool send(Request &req) override
    {
        accesses++; // Emulate a timer for LRU.
        
        auto access_info = tags.accessBlock(req.addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);

        bool hit = access_info.first;
        Addr aligned_addr = access_info.second;
        
        if (hit) { ++num_hits; return true; }
        // For any read/write miss, the cache needs to load the block from lower level.
        bool next_level_hit = false;
        if (req.req_type != Request::Request_Type::WRITE_BACK)
        {
            ++num_misses;
            ++num_loads;

            // Send a loading request to next level.
            if (next_level != nullptr)
            {
                Request req;

                req.addr = aligned_addr; // Address of the missed block.
                req.req_type = Request::Request_Type::READ; // Loading (Always)

                next_level_hit = next_level->send(req);

            }
	    // TODO, extract traces from LLC
        }

        // Insert the missed block
        auto insert_info = tags.insertBlock(aligned_addr,
                                            req.req_type != Request::Request_Type::READ ?
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

            // TODO-1, extract traces from LLC
        }
        // Invalidate upper levels (inclusive)
        for (auto &prev_level : prev_levels) { prev_level->inval(victim_addr); }
        tags.clearData(aligned_addr); // Clear all the old data

        // Load new data from lower level if there is a hit there.
        if (next_level_hit)
        {
            std::vector<uint8_t> ori_data;
            std::vector<uint8_t> new_data;

            next_level->getBlock(aligned_addr, ori_data, new_data);

            assert(ori_data.size() > 0);
            assert(new_data.size() > 0);

            setBlock(aligned_addr, ori_data, new_data);
        }

	return next_level_hit;
    }

    void inval(uint64_t _addr) override
    {
        // Invalidate the block address
        tags.inval(_addr);
        for (auto &prev_level : prev_levels) { prev_level->inval(_addr); }
    }

    void loadBlock(uint64_t _addr, uint8_t *data, unsigned int size) override
    {
        tags.loadBlock(_addr, data, size);

        if (next_level != nullptr) { next_level->loadBlock(_addr, data, size); }
    }

    void modifyBlock(uint64_t _addr, uint8_t *data, unsigned int size) override
    {
        tags.modifyBlock(_addr, data, size);

        if (next_level != nullptr) { next_level->modifyBlock(_addr, data, size); }
    }

    void getBlock(uint64_t _addr,
                  std::vector<uint8_t> &ori_data,
                  std::vector<uint8_t> &new_data) override
    {
        tags.getBlock(_addr, ori_data, new_data);
    }

    void setBlock(uint64_t _addr,
                  std::vector<uint8_t> &ori_data,
                  std::vector<uint8_t> &new_data) override
    {
        tags.setBlock(_addr, ori_data, new_data);
    }

    void registerStats(Stats &stats) override
    {
        std::string registeree_name = level_name;
        if (id != -1)
        {
            registeree_name = "Core-" + to_string(id) + "-" + 
                              registeree_name;
        }

        stats.registerStats(registeree_name +
                            ": Number of hits = " + to_string(num_hits));
        stats.registerStats(registeree_name +
                            ": Number of misses = " + to_string(num_misses));

        double hit_ratio = double(num_hits) / (double(num_hits) + double(num_misses)) * 100;
        stats.registerStats(registeree_name + 
                            ": Hit ratio = " + to_string(hit_ratio) + "%");

        stats.registerStats(registeree_name +
                            ": Number of Loads = " + to_string(num_loads));        
        stats.registerStats(registeree_name +
                            ": Number of Evictions = " + to_string(num_evicts));
    }

  protected:
    Tick accesses = 0; // We are using this for LRU policy.

    uint64_t num_loads = 0;
    uint64_t num_evicts = 0;
    uint64_t num_misses = 0;
    uint64_t num_hits = 0;

    T tags;

    Config::Cache_Level level;
    std::string level_name;
    std::string toString()
    {
        if (level == Config::Cache_Level::L1D)
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
