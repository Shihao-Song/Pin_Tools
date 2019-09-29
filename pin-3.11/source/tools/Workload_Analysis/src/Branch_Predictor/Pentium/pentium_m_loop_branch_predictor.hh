#ifndef __PENTIUM_M_LOOP_PREDICTOR_HH__
#define __PENTIUM_M_LOOP_PREDICTOR_HH__

#include "../branch_predictor.hh"

#include <vector>

namespace BP
{
// 128 entries
// 6 bit tag
// 2 ways
class Pentium_M_Loop_Branch_Predictor : public Branch_Predictor
{
  public:
    Pentium_M_Loop_Branch_Predictor() : sets(NUM_ENTRIES / NUM_WAYS, NUM_WAYS)
    {
    }

    // The goal of a loop predictor: 
    // 000001 -> predict() == 0; 111110 -> predict() == 1.
    // Intuition: we do not expect to see two of the opposite type in a row.
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
                bool current_prediction = sets[index].ways[w].predict();
                bool match = prediction_match(w, index, actual);
                bool previous_actual = sets[index].ways[w].prev_actual;
                Count &next_counter = sets[index].ways[w].count;
                Count &next_limit = sets[index].ways[w].limit;
                bool &next_enabled = sets[index].ways[w].enabled;
                Count current_counter = next_counter;
                Count current_limit = next_limit;
                bool current_enabled = next_enabled;

                Count same_direction_counter;
                Count same_direction_limit;
                bool same_direction_enabled = next_enabled;

                if (!match)
                {
                    if (current_counter < current_limit)
                    {
                        // (1) the loop becomes shorter
                        // (2) current_counter records the length of the loop
                        // (3) update the loop length with current_counter
                        // (4) reset counter since we are starting a new loop
                        same_direction_counter = 0;
                        same_direction_limit = current_counter;
                    }
                    else
                    {
                        // (1) the loop becomes longer
                        same_direction_counter = current_counter + 1;
                        same_direction_limit = current_counter + 1;
                        same_direction_enabled = 0;
                    }
                }
                else
                {
                    // On a match, we can continue as normal
                    // Update the counter, and nothing else
                    // If we have reached the limit, reset the counter
                    if (current_counter == current_limit)
                    {
                        same_direction_counter = 0;
                    }
                    else
                    {
                        same_direction_counter = current_counter + 1;
                    }

                    // The limit will remain the same
                    same_direction_limit = current_limit;
                }

                // Determine the state
                if (previous_actual == actual && actual == !current_prediction)
                {
                    // Here, we have seen two of the opposite type in a row
                    // Reset the limit and the counter for the next pass
                    // We assume that the two predictions seen here are not part of the same branch
                    // structure, but of two seperate branch structures, and therefore count it as one loop
                    // iteration instead of two. (setting next_counter to 1)
                    next_counter = 1;
                    next_limit = 1;

                    // Update the predictor
                    if (actual) { sets[index].ways[w].increment(); }
                    else { sets[index].ways[w].decrement(); }

                    // Disable the entry since we have just started to look in another direction
                    next_enabled = false;
                }
                else
                {
                    if (previous_actual != actual && actual == !current_prediction)
                    {
                        // We are now in the situations of 1110 or 0001
                        if (!current_enabled)
                        {
                            next_enabled = true;

                            // At the same time, setup the count and limits correctly
                            next_counter = 0;
                            next_limit = current_limit;
                        }
                        else
                        {
                            // Here, we haven't see a direction change.
                            // Use the state from the previous, same direction, section
                            next_enabled = same_direction_enabled;
                            next_counter = same_direction_counter;
                            next_limit = same_direction_limit;
                        }
                    }
		    else
                    {
                        // Here, we haven't see a direction change.
                        // Use the state from the previous, same direction, section
                        next_enabled = same_direction_enabled;
                        next_counter = same_direction_counter;
                        next_limit = same_direction_limit;
                    }
                }
                sets[index].ways[w].lru = timer;
                sets[index].ways[w].prev_actual = actual;


                if (sets[index].ways[w].lru < sets[index].ways[lru_way].lru)
                {
                    lru_way = w;
                }
            }
        }	
    }

  protected:
    static const unsigned NUM_ENTRIES = 128;
    static const unsigned TAG_BITS = 6;
    static const unsigned NUM_WAYS = 2;

  protected:
    class Way
    {
      public:
        // We are using 1-bit counter here.
        Way() : sat_counter(1), valid(false), lru(0) {}

        bool predict() { return sat_counter.predict(); }
        void increment() { sat_counter.increment(); }
        void decrement() { sat_counter.decrement(); }

        void init(Addr _tag, Count timer, bool actual)
        { valid = true; tag = tag; lru = timer;
          if (actual) { sat_counter.val = sat_counter.max_val; }
          else { sat_counter.val = 0; } }

        Sat_Counter sat_counter; // Each way has a counter;

        bool valid;
        Addr tag; // Tag of the way.
        Count lru; // LRU counter.

        bool enabled = false;
        Count count = 0;
        Count limit = 0;

        bool prev_actual = false;
    };

    class Set
    {
      public:
        Set(unsigned num_ways) : ways(num_ways) {}

        std::vector<Way> ways;
    };
    std::vector<Set> sets;

    // Pentium M-specific indexing and tag values
    void gen_index_tag(Addr pc, Addr &index, Addr &tag)
    {
        index = (pc >> 4) & 0x3F; // 64 sets
        tag = (pc >> 10) & 0x3F;
    }

    // Correctness means: (1) predict loop limit correctly;
    //                    (2) predict outcome correctly;
    bool prediction_match(unsigned way, Addr index, bool actual)
    {
        bool prediction = sets[index].ways[way].predict();
        Count count = sets[index].ways[way].count;
        Count limit = sets[index].ways[way].limit;

        // At our count limit
        if (count == limit)
        {
            // Did we predict correctly?
            if (prediction != actual)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        // Not at our count limit
        else
        {
            // Did we predict correctly?
            if (prediction == actual)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
};
}

#endif
