#ifndef __PENTIUM_M_HH__
#define __PENTIUM_M_HH__

#include "pentium_m_global_predictor.hh"
#include "pentium_m_loop_branch_predictor.hh"

namespace BP
{
class PentiumM : public Branch_Predictor
{
  public:
    PentiumM() {}

    void predict(Instruction &instr, Count timer) override
    {
    //    global_predictor.lookup(instr.PC);
    }

  protected:
    // Need (1): A global predictor
    Pentium_M_Global_Predictor global_predictor;
    // Need (2): A branch target buffer
    // Need (3): A bimodal table
    // Need (4): A loop branch predictor
    Pentium_M_Loop_Branch_Predictor loop_predictor;
};
}

#endif
