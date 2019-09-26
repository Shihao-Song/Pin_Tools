#ifndef __TOURNAMENT_HH__
#define __TOURNAMENT_HH__

#include "branch_predictor.hh"

#include <vector>

namespace BP
{
class Tournament : public Branch_Predictor
{
  public:
    Tournament() : local_predictor_size(CONSTANTS::localPredictorSize),
                   local_predictor_mask(local_predictor_size - 1),
                   local_counters(local_predictor_size, CONSTANTS::localCounterBits),
                   
                   local_history_table_size(CONSTANTS::localHistoryTableSize),
                   local_history_table_mask(local_history_table_size - 1),
                   local_history_table(local_history_table_size, 0),

                   global_predictor_size(CONSTANTS::globalPredictorSize),
                   global_history_mask(global_predictor_size - 1),
                   global_counters(global_predictor_size, CONSTANTS::globalCounterBits),

                   choice_predictor_size(CONSTANTS::choicePredictorSize),
                   choice_history_mask(choice_predictor_size - 1),
                   choice_counters(choice_predictor_size, CONSTANTS::choiceCounterBits),

                   global_history(0),
                   history_register_mask(choice_predictor_size - 1)
    {
        assert(checkPowerofTwo(local_predictor_size));
        assert(checkPowerofTwo(local_history_table_size));
        assert(checkPowerofTwo(global_predictor_size));
        assert(checkPowerofTwo(choice_predictor_size));
        assert(global_predictor_size == choice_predictor_size); //TODO, limitation
    }

    void predict(Instruction &instr) override
    {
        Addr branch_addr = instr.PC;

        // Step one, get local prediction.
        unsigned local_history_table_index = 
            (branch_addr >> instShiftAmt) & local_history_table_mask;

        unsigned local_predictor_index = local_history_table[local_history_table_index] & 
            local_predictor_mask;

        bool local_prediction = local_counters[local_predictor_index].predict();

        // Step two, get global prediction.
        unsigned global_predictor_index = global_history & global_history_mask;

        bool global_prediction = global_counters[global_predictor_index].predict();

        // Step three, get choice prediction.
        unsigned choice_predictor_index = global_history & choice_history_mask;

        bool choice_prediction = choice_counters[choice_predictor_index].predict();

        // Step four, final prediction.
        bool final_prediction;
        if (choice_prediction)
        {
            final_prediction = global_prediction;
        }
        else
        {
            final_prediction = local_prediction;
        }

        if (final_prediction == instr.taken) { ++num_correct_preds; }
        else { ++num_incorrect_preds; }

        // Step five, update counters
        if (local_prediction != global_prediction)
        {
            if (local_prediction == instr.taken)
            {
                // Should be more favorable towards local predictor.
                choice_counters[choice_predictor_index].decrement();
            }
            else if (global_prediction == instr.taken)
            {
                // Should be more favorable towards global predictor.
                choice_counters[choice_predictor_index].increment();
            }
        }

        if (instr.taken)
        {
            global_counters[global_predictor_index].increment();
            local_counters[local_predictor_index].increment();
        }
        else
        {
            global_counters[global_predictor_index].decrement();
            local_counters[local_predictor_index].decrement(); 
        } 

        // Step six, update global history register
        global_history = global_history << 1 | instr.taken;
    }

  protected:
    unsigned local_predictor_size;
    unsigned local_predictor_mask;
    std::vector<Sat_Counter> local_counters;

    unsigned local_history_table_size;
    unsigned local_history_table_mask;
    std::vector<unsigned> local_history_table;

    unsigned global_predictor_size;
    unsigned global_history_mask;
    std::vector<Sat_Counter> global_counters;

    unsigned choice_predictor_size;
    unsigned choice_history_mask;
    std::vector<Sat_Counter> choice_counters;

    uint64_t global_history;
    unsigned history_register_mask;
};
}

#endif
