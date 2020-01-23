#ifndef __SIM_CONFIG_HH__
#define __SIM_CONFIG_HH__

#include <fstream>
#include <string>
#include <vector>

using std::ifstream; // Not sure why this works.

class Config
{
  public:
    unsigned num_cores;

    unsigned block_size;

    enum Cache_Level : int
    {
        L1D, L2, L3, eDRAM, MAX
    };

    struct Cache_Info
    {
        bool valid = false;

        int assoc;
        unsigned size;
        bool write_only;
        bool shared;
    };
    std::vector<Cache_Info> caches;

    Config(std::string fname) : caches(int(Cache_Level::MAX)) { parse(fname); }

    void parse(std::string &fname)
    {
        ifstream file(fname.c_str());
        assert(file.good());
    
        std::string line;
        while(getline(file, line))
        {
            char delim[] = " \t=";
            std::vector<std::string> tokens;

            while (true)
            {
                size_t start = line.find_first_not_of(delim);
                if (start == std::string::npos)
                {
                    break;
                }

                size_t end = line.find_first_of(delim, start);
                if (end == std::string::npos)
                {
                    tokens.push_back(line.substr(start));
                    break;
                }

                tokens.push_back(line.substr(start, end - start));
                line = line.substr(end);
            }

            // empty line
            if (!tokens.size())
            {
                continue;
            }

            // comment line
            if (tokens[0][0] == '#')
            {
                continue;
            }

            // parameter line
            assert(tokens.size() == 2 && "Only allow two tokens in one line");

            // Extract Timing Parameters
            if(tokens[0] == "num_cores")
            {
                num_cores = atoi(tokens[1].c_str());
            }
	    else if(tokens[0] == "block_size")
            {
                block_size = atoi(tokens[1].c_str());
            }
            else if(tokens[0].find("L1D") != std::string::npos)
            {
                extractCacheInfo(Cache_Level::L1D, tokens);
            }
            else if(tokens[0].find("L2") != std::string::npos)
            {
                extractCacheInfo(Cache_Level::L2, tokens);
            }
            else if(tokens[0].find("L3") != std::string::npos)
            {
                extractCacheInfo(Cache_Level::L3, tokens);
            }
            else if(tokens[0].find("eDRAM") != std::string::npos)
            {
                extractCacheInfo(Cache_Level::eDRAM, tokens);
            }
        }
        file.close();
    }
    
    void extractCacheInfo(Cache_Level level, std::vector<std::string> &tokens)
    {
        caches[int(level)].valid = true;

        if(tokens[0].find("assoc") != std::string::npos)
        {
            caches[int(level)].assoc = atoi(tokens[1].c_str());
        }
        else if(tokens[0].find("size") != std::string::npos)
        {
            caches[int(level)].size = atoi(tokens[1].c_str());
        }
        else if(tokens[0].find("write_only") != std::string::npos)
        {
            caches[int(level)].write_only = tokens[1] == "false" ? 0 : 1;
        }
        else if(tokens[0].find("shared") != std::string::npos)
        {
            caches[int(level)].shared = tokens[1] == "false" ? 0 : 1;
        }

    }
};
#endif
