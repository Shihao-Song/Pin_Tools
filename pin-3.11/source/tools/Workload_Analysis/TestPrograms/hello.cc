#include <iostream>
#include <vector>

void helloWorld(std::vector<int> &vec)
{
    // std::cout << "Hello World!\n";
    for (int i = 0; i < vec.size(); i++)
    {
        vec[i] += 1;
    }
}

int main()
{
    std::vector<int> vec;
    vec.resize(8192, 1);

    helloWorld(vec);
}
