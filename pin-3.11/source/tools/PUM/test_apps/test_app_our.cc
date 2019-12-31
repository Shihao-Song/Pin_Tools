#include <algorithm>
#include <random>

#include "../assembly_instructions/assm.h"

typedef PUM::UINT8 UINT8;

int main()
{
    const unsigned ARR_SIZE = 1024;
    UINT8* src_1 = (UINT8 *)malloc(ARR_SIZE * sizeof(UINT8));
    UINT8* src_2 = (UINT8 *)malloc(ARR_SIZE * sizeof(UINT8));
    UINT8* dest = (UINT8 *)malloc(ARR_SIZE * sizeof(UINT8));
    // Initialize data
    static std::default_random_engine e{};
    static std::uniform_int_distribution<UINT8> d{};
    for (int i = 0; i < ARR_SIZE; i++)
    {
        src_1[i] = d(e);
        src_2[i] = d(e);
    }

    for (int i = 0; i < ARR_SIZE; i += 256)
    {
        PUM::ROWAND_256(&(src_1[i]), &(src_2[i]), &(dest[i]));
    }
    
    free(src_1);
    free(src_2);
    free(dest);
}
