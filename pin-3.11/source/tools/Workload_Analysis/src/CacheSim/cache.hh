#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../Sim/mem_object.hh"
#include "../Sim/util.hh"

#include "tags/cache_tags.hh"
#include "tags/set_assoc_tags.hh"
#include "tags/fa_tags.hh"

#include <sstream>
#include <string>

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
    {}

    void send(Request &req) override
    {
        accesses++;

        auto access_info = tags.accessBlock(req.addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);

        bool hit = access_info.first;
        Addr aligned_addr = access_info.second;

        // If cache hit, simply return;
        if (hit) { ++num_hits; return;}
        // For any read/write miss, the cache needs to load the block from lower level.
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

                next_level->send(req);
            }
        }

        // Insert the missed block
        auto insert_info = tags.insertBlock(aligned_addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);
        bool wb_required = insert_info.first;
        Addr wb_addr = insert_info.second;
        
        // Send a write-back request to next level if there is an eviction.
        if (wb_required)
        {
            ++num_evicts;

            if (next_level != nullptr)
            {
                Request req;

                req.addr = wb_addr; // Address of the evicted block.
                req.req_type = Request::Request_Type::WRITE_BACK;

                next_level->send(req);
            }
	}
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
typedef Cache<LRUFATags> FACache;

class WOFACache : public FACache
{
  public:
    WOFACache(Config::Cache_Level lev, Config &cfg) : FACache(lev, cfg) {}

    void send(Request &req) override
    {
        accesses++;

        auto access_info = tags.accessBlock(req.addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);

        bool hit = access_info.first;
        Addr aligned_addr = access_info.second;

        // If cache hit, simply return;
        if (hit) { ++num_hits; return; }

        // We won't insert any read miss blocks.
        if (req.req_type == Request::Request_Type::READ) { ++num_misses; return; }

        // For any write miss, the cache needs to load the block from lower level.
        // When WOFACache is used as the LLC, this should not happen.
        if (req.req_type == Request::Request_Type::WRITE)
        {
            ++num_misses;
            ++num_loads;

            // Send a loading request to next level.
            if (next_level != nullptr)
            {
                Request req;

                req.addr = aligned_addr; // Address of the missed block.
                req.req_type = Request::Request_Type::READ; // Loading (Always)

                next_level->send(req);
            }
        }

        // Insert the missed block
        auto insert_info = tags.insertBlock(aligned_addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);
        bool wb_required = insert_info.first;
        Addr wb_addr = insert_info.second;
        
        // Send a write-back request to next level if there is an eviction.
        if (wb_required)
        {
            ++num_evicts;

            if (next_level != nullptr)
            {
                Request req;

                req.addr = wb_addr; // Address of the evicted block.
                req.req_type = Request::Request_Type::WRITE_BACK;

                next_level->send(req);
            }
	}
    }
};
}
#endif
