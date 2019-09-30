#ifndef __SIM_STATS_HH__
#define __SIM_STATS_HH__

#include <string>
#include <vector>

class Stats
{
  protected:
    std::vector<std::string> printables;

  public:
    Stats(){}

    void registerStats(std::string printable)
    {
        printables.push_back(printable + "\n");
    }

    void outputStats(std::string output)
    {
        for (auto entry : printables)
        {
            std::cout << entry;
        }
    }
};

#endif
