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

        bool actual = instr.taken;
        if (final_prediction == actual) { ++num_correct_preds; }
        else { ++num_incorrect_preds; }
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
