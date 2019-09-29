#ifndef __PENTIUM_M_GLOBAL_PREDICTOR_HH__
#define __PENTIUM_M_GLOBAL_PREDICTOR_HH__

#include "../branch_predictor.hh"

#include <vector>

namespace BP
{
// The Pentium M Global Branch Predictor
// 2048-entries
// 6-tag bits per entry
// 4-way set associative
class Pentium_M_Global_Predictor : public Branch_Predictor
{
  public:
    Pentium_M_Global_Predictor() : sets(NUM_ENTRIES / NUM_WAYS, NUM_WAYS)
    {}

    int lookup(Addr pc, Addr pir, Count timer)
    {
        Addr index, tag;

        gen_index_tag(pc, pir, index, tag);

        for (unsigned w = 0; w < NUM_WAYS; w++)
        {
            if (sets[index].ways[w].valid && sets[index].ways[w].tag == tag)
            {
                return sets[index].ways[w].predict();
            }
        }

        return -1; // Not found.
    }

    void evict(Addr pc, Addr pir)
    {
        Addr index, tag;

        gen_index_tag(pc, pir, index, tag);

        for (unsigned w = 0; w < NUM_WAYS; w++)
        {
            if (sets[index].ways[w].valid && sets[index].ways[w].tag == tag)
            {
                sets[index].ways[w].valid = false;
                return;
            }
        }
    }

    void update(bool actual, Addr pc, Addr pir, Count timer)
    {
        Addr index, tag;

        gen_index_tag(pc, pir, index, tag);

        // Start with way 0 as the least recently used
        unsigned lru_way = 0;

        for (unsigned w = 0; w < NUM_WAYS; ++w)
        {
            if (sets[index].ways[w].valid && sets[index].ways[w].tag == tag)
            {
                if (actual) { sets[index].ways[w].increment(); }
                else { sets[index].ways[w].decrement(); }

                sets[index].ways[w].lru = timer;

                return;
            }

            if (sets[index].ways[w].lru < sets[index].ways[lru_way].lru)
            {
                lru_way = w;
            }
        }

        sets[index].ways[lru_way].init(tag, timer, actual);
    }

  protected:
    class Way
    {
      public:
        Way() : sat_counter(2), valid(false), lru(0) {}
        
        bool predict() { return sat_counter.predict(); }
        void increment() { sat_counter.increment(); }
        void decrement() { sat_counter.decrement(); }

        void init(Addr _tag, Count timer, bool actual) 
        { valid = true; tag = tag; lru = timer;
          if (actual) { sat_counter.val = sat_counter.max_val; }
          else { sat_counter.val = 0; } }

        Sat_Counter sat_counter; // Each way has a counter;

        bool valid; // Is the way valid?
        Addr tag; // Tag of the way.
        Count lru; // LRU counter.
    };

    class Set
    {
      public:
        Set(unsigned num_ways) : ways(num_ways) {}

        std::vector<Way> ways;
    };
    std::vector<Set> sets;

    static const unsigned NUM_ENTRIES = 2048;
    static const unsigned TAG_BITS = 6;
    static const unsigned NUM_WAYS = 4;

    // Hardcoded for pentium M
    // Pentium M specific.
    void gen_index_tag(Addr pc, Addr pir, Addr &index, Addr &tag)
    {
        index = ((pc >> 4) ^ (pir >> 6)) & 0x1FF; // 512 sets
        tag = ((pc >> 13) ^ pir) & 0x3F; // 6-bit
    }
};
}

#endif
