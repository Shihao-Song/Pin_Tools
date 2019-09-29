#ifndef __PENTIUM_M_BTB_HH__
#define __PENTIUM_M_BTB_HH__

#include "../branch_predictor.hh"

#include <vector>

namespace BP
{
class Pentium_M_BTB : public Branch_Predictor
{
  public:
    Pentium_M_BTB() : sets(NUM_ENTRIES / NUM_WAYS, NUM_WAYS) {}

    int lookup(Addr pc, Count timer)
    {
        Addr index, tag;

        gen_index_tag(pc, index, tag);

        for (unsigned w = 0; w < NUM_WAYS; w++)
        {
            if (sets[index].ways[w].valid && sets[index].ways[w].tag == tag)
            {
                return 1;
            }
        }

        return -1; // Not found.
    }

    void update(bool actual, Addr pc, Count timer)
    {
        Addr index, tag;

        gen_index_tag(pc, index, tag);

        // Start with way 0 as the least recently used
        unsigned lru_way = 0;

        for (unsigned w = 0; w < NUM_WAYS; ++w)
        {
            if (sets[index].ways[w].valid && sets[index].ways[w].tag == tag)
            {
                sets[index].ways[w].lru = timer;
                return;
            }

            if (sets[index].ways[w].lru < sets[index].ways[lru_way].lru)
            {
                lru_way = w;
            }
        }
        sets[index].ways[lru_way].init(tag, timer);
    }

  protected:
    class Way
    {
      public:
        Way() : valid(false), lru(0) {}

        void init(Addr _tag, Count timer)
        { valid = true; tag = tag; lru = timer; }

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
    static const unsigned NUM_WAYS = 4;

    // Hardcoded for pentium M
    // Pentium M specific.
    void gen_index_tag(Addr pc, Addr &index, Addr &tag)
    {
        index = (pc >> 4) & 0x1FF; // 512 sets
        tag = pc & 0x3fe00f;
    }
};
}

#endif
