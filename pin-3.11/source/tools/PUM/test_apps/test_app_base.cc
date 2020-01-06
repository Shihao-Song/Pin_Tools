#include <algorithm>
#include <random>

#include "../assembly_instructions/assm.h"

// typedef PUM::double double;

void regionOfInterest(double* src_1, double* src_2, double* dest, unsigned arr_size)
{
    for (int i = 0; i < arr_size; i++)
    {
        dest[i] = src_1[i] + src_2[i];
    }
}

int main()
{
    const unsigned ARR_SIZE = 1024;
    double* src_1 = (double *)malloc(ARR_SIZE * sizeof(double));
    double* src_2 = (double *)malloc(ARR_SIZE * sizeof(double));
    double* dest = (double *)malloc(ARR_SIZE * sizeof(double));
    // Initialize data
    static std::default_random_engine e{};
    static std::uniform_real_distribution<double> d{};
    for (int i = 0; i < ARR_SIZE; i++)
    {
        src_1[i] = d(e);
        src_2[i] = d(e);
    }

    regionOfInterest(src_1, src_2, dest, ARR_SIZE);
    for (int i = 0; i < ARR_SIZE - 1; i++)
    {
        std::cout << dest[i] << " ";
    }
    std::cout << dest[ARR_SIZE - 1] << "\n";

    free(src_1);
    free(src_2);
    free(dest);
}
