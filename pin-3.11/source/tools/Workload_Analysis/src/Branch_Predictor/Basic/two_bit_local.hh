#ifndef __TWO_BIT_LOCAL_HH__
#define __TWO_BIT_LOCAL_HH__

#include "../branch_predictor.hh"

#include <vector>

namespace BP
{
class Two_Bit_Local : public Branch_Predictor
{
  public:
    Two_Bit_Local(): local_predictor_size(CONSTANTS::localPredictorSize),
                     index_mask(local_predictor_size - 1),
                     local_counters(local_predictor_size, CONSTANTS::localCounterBits)
    {
        assert(checkPowerofTwo(local_predictor_size));
    }

    void predict(Instruction &instr, Count timer) override
    {
        Addr branch_addr = instr.PC;

        // Step one, get prediction
        unsigned local_index = (branch_addr >> instShiftAmt) & index_mask;

        bool prediction = local_counters[local_index].predict();
        if (prediction == instr.taken) { ++num_correct_preds; }
        else { ++num_incorrect_preds; }

        // Step two, update counter
        if (instr.taken)
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
    const unsigned local_predictor_size; // Number of entries in a local predictor
    const unsigned index_mask;

    // It seems that PIN does not support C++11. :( 
    std::vector<Sat_Counter> local_counters;
};
}

#endif
