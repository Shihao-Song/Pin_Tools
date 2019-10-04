#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../Sim/mem_object.hh"

#include "tags/cache_tags.hh"
#include "tags/set_assoc_tags.hh"
#include "tags/fa_tags.hh"

#include <string>

namespace CacheSimulator
{
// Should be a template.
class Cache : public MemObject
{
  public:
    Cache(Config::Cache_Level lev, Config &cfg) : tags(int(lev), cfg){}

    void send(Request &req) override
    {
        accesses++;

        auto access_info = tags.accessBlock(req.addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);

        bool hit = access_info.first;
        Addr aligned_addr = access_info.second;
        if (hit) { ++num_hits; return;}

        // Insert the missed block
        auto insert_info = tags.insertBlock(aligned_addr,
                                            req.req_type != Request::Request_Type::READ ?
                                            true : false,
                                            accesses);
        bool wb_required = insert_info.first;
        Addr wb_addr = insert_info.second;

        // If there is a write-back miss, the cache can allocate the block imme.
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
                req.req_type = Request::Request_Type::READ; // Loading

                next_level->send(req);
            }
        }

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

    void setNextLevel(Cache *_next_level) { next_level = _next_level; }

//  protected:
    Tick accesses = 0; // We are using this for LRU policy.

    uint64_t num_loads = 0;
    uint64_t num_evicts = 0;
    uint64_t num_misses = 0;
    uint64_t num_hits = 0;

    LRUSetWayAssocTags tags;

    Cache *next_level = nullptr;
};
/*
struct OnChipToOffChip{}; // Next level is off-chip
struct OnChipToOnChip{}; // Next level is still on-chip

struct NormalMode{};
struct ReadOnly{}; // The cache is read only
struct WriteOnly{}; // The cache is write only

template<class Tag, class Mode, class Position>
class Cache : public Simulator::MemObject
{
  protected:

    std::string level_name;

    Tick clk;

  protected:
    uint64_t num_hits;
    uint64_t num_misses;
    uint64_t num_loads;
    uint64_t num_evicts;

  public:
    Cache()
        : Simulator::MemObject(),
          level(_level),
          level_name(toString()),
          clk(0),
          tags(new Tag(int(_level), cfg)),
          mshr_queue(new CacheQueue(cfg.caches[int(_level)].num_mshrs)),
          wb_queue(new CacheQueue(cfg.caches[int(_level)].num_wb_entries)),
          tag_lookup_latency(cfg.caches[int(_level)].tag_lookup_latency),
          nclks_to_tick_next_level(nclksToTickNextLevel(cfg)),
          num_hits(0),
          num_misses(0),
          num_loads(0),
          num_evicts(0)
    {}
    
    bool send(Request &req) override
    {

        // Step one, check whether it is a hit or not
        
    }


    void reInitialize() override
    {
        clk = 0;
        tags->reInitialize();

        num_hits = 0;
        num_misses = 0;
        num_loads = 0;
        num_evicts = 0;

        next_level->reInitialize();
    }

    void registerStats(Simulator::Stats &stats) override
    {
        std::string registeree_name = level_name;
        if (id != -1)
        {
            registeree_name = "Core-" + std::to_string(id) + "-" + 
                              registeree_name;
        }

        stats.registerStats(registeree_name +
                            ": Number of hits = " + std::to_string(num_hits));
        stats.registerStats(registeree_name +
                            ": Number of misses = " + std::to_string(num_misses));

        double hit_ratio = double(num_hits) / (double(num_hits) + double(num_misses));
        stats.registerStats(registeree_name + 
                            ": Hit ratio = " + std::to_string(hit_ratio));

        stats.registerStats(registeree_name +
                            ": Number of Loads = " + std::to_string(num_loads));        
        stats.registerStats(registeree_name +
                            ": Number of Evictions = " + std::to_string(num_evicts));
    }
};
*/
/*
typedef Cache<LRUFATags,NormalMode,OnChipToOffChip> FA_LRU_LLC;
typedef Cache<LRUFATags,WriteOnly,OnChipToOffChip> FA_LRU_LLC_WRITE_ONLY;
typedef Cache<LRUSetWayAssocTags,NormalMode,OnChipToOffChip> SET_WAY_LRU_LLC;
typedef Cache<LRUSetWayAssocTags,NormalMode,OnChipToOnChip> SET_WAY_LRU_NON_LLC;
*/

}
#endif
