#ifndef __PENTIUM_M_HH__
#define __PENTIUM_M_HH__

#include "pentium_m_bimodal.hh"
#include "pentium_m_btb.hh"
#include "pentium_m_loop_branch_predictor.hh"
#include "pentium_m_global_predictor.hh"

namespace BP
{
class PentiumM : public Branch_Predictor
{
  public:
    PentiumM() {}

    void predict(Instruction &instr, Count timer) override
    {
        Addr branch_addr = instr.PC;

        // -1 means not found
        // 0/1 means the real prediction
        int global_out = global_predictor.lookup(branch_addr, pir, timer);
        int lp_out = loop_predictor.lookup(branch_addr, timer);
        int bimodal_out = bimodal.lookup(branch_addr, timer);
        int btb_out = btb.lookup(branch_addr, timer);

        bool final_prediction;
        if (global_out != -1) {  final_prediction = global_out; }
        else if (lp_out != -1 && btb_out != -1) { final_prediction = lp_out; }
        else { final_prediction = bimodal_out; }

        // Update counters
        bool actual = instr.taken;
        if (final_prediction == actual) { ++num_correct_preds; }
        else { ++num_incorrect_preds; }

        // Update tables
        btb.update(actual, branch_addr, timer);
        loop_predictor.update(actual, branch_addr, timer);

        // Update bimodal predictor only when global and loop predictors missed
        if (global_out == -1 && lp_out == -1)
        {
            bimodal.update(actual, branch_addr, timer);
        }

        // Global should only allocate when no loop predictor hit and bimodal was wrong
        bool alloc_global = (lp_out == -1) && (bimodal_out != actual);

        if (global_out != -1)
        {
            if (final_prediction != actual && !alloc_global) { global_predictor.evict(branch_addr, pir); }
            else { global_predictor.update(actual, branch_addr, pir, timer); }
        }
        else if (final_prediction != actual && alloc_global)
        {
            global_predictor.update(actual, branch_addr, pir, timer);
        }

        // update pir
        Addr rhs = 0;
        if (actual) { rhs = branch_addr >> 4; }
        pir = ((pir << 2) ^ rhs) & 0x7fff;
    }

  protected:
    Addr pir = 0;

    // Need (1): A global predictor
    Pentium_M_Global_Predictor global_predictor;
    // Need (2): A branch target buffer
    Pentium_M_BTB btb;
    // Need (3): A bimodal table
    Pentium_M_Bimodal bimodal;
    // Need (4): A loop branch predictor
    Pentium_M_Loop_Branch_Predictor loop_predictor;
};
}

#endif
