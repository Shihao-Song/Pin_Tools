#ifndef __MMU_HH__
#define __MMU_HH__

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "random.hh"
#include "../Sim/request.hh"

using std::ofstream;

namespace System
{
class Mapper
{
  protected:
    int core_id;

    uint8_t m_address_randomization_table[256];

  public:
    static const uint64_t pa_core_shift = 48;
    static const uint64_t pa_core_size = 16;
    static const uint64_t pa_va_mask = ~(((uint64_t(1) << pa_core_size) - 1) << pa_core_shift);

    static const unsigned page_size = 4096; // 4kB    
    static const uint64_t va_page_shift = 12;
    static const uint64_t va_page_mask = (uint64_t(1) << va_page_shift) - 1;

  public:
    Mapper(int _core_id) : core_id(_core_id)
    {
        // Using core_id as a seed
        uint64_t state = rng_seed(core_id);
        m_address_randomization_table[0] = 0;
        for(unsigned int i = 1; i < 256; ++i)
        {
            uint8_t j = rng_next(state) % (i + 1);
            m_address_randomization_table[i] = m_address_randomization_table[j];
            m_address_randomization_table[j] = i;
        }
    }

    uint64_t va2pa(uint64_t va)
    {
        uint64_t va_page = va >> va_page_shift;

        // Randomly remapping the lower 32 bits of va_page.
        uint8_t *array = (uint8_t *)&va_page;
        array[0] = m_address_randomization_table[array[0]];
        array[1] = m_address_randomization_table[array[1]];
        array[2] = m_address_randomization_table[array[2]];
        array[3] = m_address_randomization_table[array[3]];

        // Re-organize
        uint64_t pa = (va_page << va_page_shift) |
                      (va & va_page_mask);

        return pa;
    }
};

class MMU
{
  protected:
    std::vector<Mapper> mappers;

  public:

    MMU(int num_cores)
    {
        for (int i = 0; i < num_cores; i++)
        {
            mappers.emplace_back(i);
        }
    }

    virtual void va2pa(Request &req)
    {
        Addr pa = mappers[req.core_id].va2pa(req.addr);
        req.addr = pa;
    }
};

#include <algorithm>
class SingleNode : public MMU
{
  protected:
    // All the touched pages for each core.
    std::vector<std::unordered_map<Addr,Addr>> pages_by_cores;

    // A pool of free physical pages
    std::vector<Addr> free_frame_pool;

    // A pool of used physical pages.
    std::unordered_map<Addr,bool> used_frame_pool;

    // TODO, hard-coded so far.
    const unsigned memory_size_gb = 128;
  public:
    SingleNode(int num_of_cores)
        : MMU(num_of_cores)
    {
        pages_by_cores.resize(num_of_cores);

        for (unsigned int i = 0; i < memory_size_gb * 1024 * 1024 / 4; i++)
        {
            // All available pages
            free_frame_pool.push_back(i);
        }
        std::random_shuffle(free_frame_pool.begin(), free_frame_pool.end());
    }

    void va2pa(Request &req) override
    {
        int core_id = req.core_id;

        Addr va = req.addr;
        Addr virtual_page_id = va >> Mapper::va_page_shift;

        auto &pages = pages_by_cores[core_id];
        
        auto p_iter = pages.find(virtual_page_id);
        if (p_iter != pages.end())
        {
            Addr page_id = p_iter->second;
            req.addr = (page_id << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);
        }
        else
        {
            auto &free_frames = free_frame_pool;

            auto &used_frames = used_frame_pool;

            // Choose a free frame
            Addr free_frame = *(free_frames.begin());

            free_frames.erase(free_frames.begin());
            used_frames.insert({free_frame, true});

            req.addr = (free_frame << Mapper::va_page_shift) |
                       (va & Mapper::va_page_mask);
            // Insert the page
            pages.insert({virtual_page_id, free_frame});
        }
    }
};
}

#endif
