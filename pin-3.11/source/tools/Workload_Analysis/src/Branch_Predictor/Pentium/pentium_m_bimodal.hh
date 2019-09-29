#ifndef __PENTIUM_M_BIMODAL_HH__
#define __PENTIUM_M_BIMODAL_HH__

#include "../branch_predictor.hh"

#include <vector>

namespace BP
{
class Pentium_M_Bimodal : public Branch_Predictor
{
  public:
    Pentium_M_Bimodal() : index_mask(NUM_ENTRIES - 1), local_counters(NUM_ENTRIES, 2) {}

    int lookup(Addr pc, Count timer)
    { 
        unsigned local_index = pc & index_mask;

        bool prediction = local_counters[local_index].predict();

        return prediction; 
    }

    void update(bool actual, Addr pc, Count timer)
    {
        unsigned local_index = pc & index_mask;

        if (actual)
        {
            // std::cout << "Correct! ";
            // std::cout << int(local_counters[local_index].val) << " -> ";
            local_counters[local_index].increment();
            // std::cout << int(local_counters[local_index].val) << "\n";
        }
        else
        {
            // std::cout << "Incorrect! "; 
            // std::cout << int(local_counters[local_index].val) << " -> ";
            local_counters[local_index].decrement();
            // std::cout << int(local_counters[local_index].val) << "\n";
        }
    }

  protected:
    static const unsigned NUM_ENTRIES = 4096;
    const unsigned index_mask;

    std::vector<Sat_Counter> local_counters;

};
}

#endif
