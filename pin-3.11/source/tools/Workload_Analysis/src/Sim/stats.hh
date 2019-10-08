#ifndef __SIM_STATS_HH__
#define __SIM_STATS_HH__

#include <fstream>
#include <string>
#include <vector>

using std::ofstream;

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
        ofstream out;
        out.open(output.c_str());

        for (auto entry : printables)
        {
            out << entry;
        }
        out << std::flush;
    }
};

#endif
