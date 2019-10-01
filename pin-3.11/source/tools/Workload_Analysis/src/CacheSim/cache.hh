#ifndef __CACHE_HH__
#define __CACHE_HH__

#include "../Sim/mem_object.hh"

#include "tags/cache_tags.hh"
#include "tags/set_assoc_tags.hh"

#include <string>

namespace CacheSimulator
{
// Should be a template.
class Cache : public MemObject
{
  public:
    Cache(Config::Cache_Level lev, Config &cfg) : tags(int(lev), cfg) {}

    bool send(Request &req) override
    {
        return false;
    }

  protected:
    LRUSetWayAssocTags tags;
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
        if (auto [hit, aligned_addr] = tags->accessBlock(req.addr,
                                       req.req_type != Request::Request_Type::READ ?
                                       true : false,
                                       clk);
            hit)
        {
        }
        else
        {
            // Step three, if there is a write-back (eviction). We should allocate the space
            // directly.
            if (req.req_type == Request::Request_Type::WRITE_BACK)
            {
                    // A write-back from higher level must be a dirty block.
                    if (auto [wb_required, wb_addr] = tags->insertBlock(aligned_addr,
                                                                        true,
                                                                        clk);
                        wb_required)
                    {
                    }

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
