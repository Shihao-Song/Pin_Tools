#ifndef __MICRO_OP_HH__
#define __MICRO_OP_HH__

typedef uint64_t Addr;

// Instruction Format
struct MICRO_OP
{
    Addr PC; // Program Counter of the instruction

    enum Instruction_Type : int {EXE, BRANCH, LOAD, STORE, MAX};
    Instruction_Type instr_type = Instruction_Type::MAX; // Instruction type

    Addr memory_addr; // load or store address (for LOAD and STORE instructions)
    unsigned payload_size; // how much data (in bytes) to be loaded/stored.

    bool taken = false; // If the instruction is a branch, what is the real direction (not the predicted).
                        // You should reply on this field to determine the correctness of your predictions.
    Addr branch_target_addr; // If the instruction is a branch, what is the branch target address. 

    /* Member Functions */
    void setPC(Addr _PC) { PC = _PC; }
    Addr getPC() { return PC; }

    void setEXE() { instr_type = Instruction_Type::EXE; }
    bool isEXE() { return instr_type == Instruction_Type::EXE; }

    void setBRANCH() { instr_type = Instruction_Type::BRANCH; }
    bool isBRANCH() { return instr_type == Instruction_Type::BRANCH; }

    void setLOAD() { instr_type = Instruction_Type::LOAD; }
    bool isLOAD() { return instr_type == Instruction_Type::LOAD; }

    void setStore() { instr_type = Instruction_Type::STORE; }
    bool isStore() { return instr_type == Instruction_Type::STORE; }

    void setMemAddr(Addr _addr) { memory_addr = _addr; }
    Addr getMemAddr() { return memory_addr; }

    void setTaken() { taken = true; }
    bool isTaken() { return taken; }
};

#endif
