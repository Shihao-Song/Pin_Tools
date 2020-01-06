#include <algorithm>
#include <iostream>
#include <random>

#include "../hooks/hooks.h"

int main()
{
    const unsigned ARR_SIZE = 64;
    uint8_t* src_1 = (uint8_t *)malloc(ARR_SIZE * sizeof(uint8_t));
    uint8_t* src_2 = (uint8_t *)malloc(ARR_SIZE * sizeof(uint8_t));
    uint8_t* dest = (uint8_t *)malloc(ARR_SIZE * sizeof(uint8_t));
    // Initialize data
    static std::default_random_engine e{};
    static std::uniform_int_distribution<uint8_t> d{};
    for (int i = 0; i < ARR_SIZE; i++)
    {
        src_1[i] = d(e);
        src_2[i] = d(e);
    }

    roi_begin();
    for (int i = 0; i < ARR_SIZE; i++)
    {
        dest[i] = src_1[i] & src_2[i];
    }
    roi_end();

    std::cout << "\nSrc (1): \n"; 
    for (int i = 0; i < ARR_SIZE - 1; i++)
    {
        std::cout << int(src_1[i]) << " ";
    }
    std::cout << int(src_1[ARR_SIZE - 1]) << "\n";

    std::cout << "\nSrc (2): \n"; 
    for (int i = 0; i < ARR_SIZE - 1; i++)
    {
        std::cout << int(src_2[i]) << " ";
    }
    std::cout << int(src_2[ARR_SIZE - 1]) << "\n";

    std::cout << "\nResult: \n"; 
    for (int i = 0; i < ARR_SIZE - 1; i++)
    {
        std::cout << int(dest[i]) << " ";
    }
    std::cout << int(dest[ARR_SIZE - 1]) << "\n";

    free(src_1);
    free(src_2);
    free(dest);
}
