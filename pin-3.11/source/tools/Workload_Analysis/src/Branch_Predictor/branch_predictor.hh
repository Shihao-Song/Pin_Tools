#ifndef __BRANCH_PREDICTOR_HH__
#define __BRANCH_PREDICTOR_HH__

#include "branch_predictor_constants.hh"
#include "../Sim/instruction.hh"

namespace BP
{
class Branch_Predictor
{
  public:
    Branch_Predictor() : instShiftAmt(CONSTANTS::instShiftAmt),
                         num_correct_preds(0), 
                         num_incorrect_preds(0) 
    {}

    virtual ~Branch_Predictor() {}

    virtual void predict(Instruction &instr, Count timer) {}

    // The correctness of the branch prediction.
    float perf() { return float(num_correct_preds) / 
                 (float(num_correct_preds) + float(num_incorrect_preds)) * 100; }

  protected:
    class Sat_Counter
    {
      public:
        Sat_Counter(unsigned _counter_bits) : counter_bits(_counter_bits),
                                              max_val((1 << counter_bits) - 1),
                                              val(0)
        {}

        void increment() { if (val < max_val) { ++val; } }

        void decrement() { if (val > 0) { --val; } }

        bool predict() { return val >> (counter_bits - 1); } // MSB determines prediction

        // TODO, should be protected, I kept them public for debugging.
//      protected:
        const unsigned counter_bits; // Number of bits of a counter

        uint8_t max_val; // Max value of the counter
        uint8_t val; // Current value of the counter
    };

  protected:
    const unsigned instShiftAmt;

    Count num_correct_preds;
    Count num_incorrect_preds;

  public:
    static int checkPowerofTwo(unsigned x)
    {
        //checks whether a number is zero or not
        if (x == 0)
        {
            return 0;
        }

        //true till x is not equal to 1
        while( x != 1)
        {
            //checks whether a number is divisible by 2
            if(x % 2 != 0)
            {
                return 0;
            }
            x /= 2;
        }
        return 1;
    }
};
}
#endif
