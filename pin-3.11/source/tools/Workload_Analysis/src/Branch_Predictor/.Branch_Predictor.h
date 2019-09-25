typedef struct Branch_Predictor
{
    #ifdef TOURNAMENT
    unsigned local_predictor_size;
    unsigned local_predictor_mask;
    Sat_Counter *local_counters;

    unsigned local_history_table_size;
    unsigned local_history_table_mask;
    unsigned *local_history_table;

    unsigned global_predictor_size;
    unsigned global_history_mask;
    Sat_Counter *global_counters;

    unsigned choice_predictor_size;
    unsigned choice_history_mask;
    Sat_Counter *choice_counters;

    uint64_t global_history;
    unsigned history_register_mask;
    #endif
}Branch_Predictor;

// Initialization function
Branch_Predictor *initBranchPredictor();

// Branch predictor functions
bool predict(Branch_Predictor *branch_predictor, Instruction *instr);

unsigned getIndex(uint64_t branch_addr, unsigned index_mask);
bool getPrediction(Sat_Counter *sat_counter);

// Utility
int checkPowerofTwo(unsigned x);

#endif
