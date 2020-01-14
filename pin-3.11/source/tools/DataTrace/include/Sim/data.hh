#ifndef __DATA_HH__
#define __DATA_HH__

#include <unordered_map>
#include <vector>

class Data
{
  public:
    Data(unsigned _block_size) : block_size(_block_size) {}

    void loadData(uint64_t aligned_addr, uint8_t *data, unsigned size)
    {
        assert(aligned_addr == (aligned_addr & ~((uint64_t)block_size - (uint64_t)1)));
        assert(size == block_size);

        DUnit storage;
        for (unsigned i = 0; i < size; i++)
        {
            storage.ori_data.push_back(data[i]);
            storage.new_data.push_back(data[i]);
        }

        data_storage.insert({aligned_addr, storage});
    }

    void getData(uint64_t aligned_addr,
                 std::vector<uint8_t> &ori_data,
                 std::vector<uint8_t> &new_data)
    {
        assert(aligned_addr == (aligned_addr & ~((uint64_t)block_size - (uint64_t)1)));
        
	auto d_iter = data_storage.find(aligned_addr);
        assert(d_iter != data_storage.end());

        for (unsigned i = 0; i < block_size; i++)
        {
            ori_data.push_back((d_iter->second).ori_data[i]);
            new_data.push_back((d_iter->second).new_data[i]);
        }

        data_storage.erase(aligned_addr);
        d_iter = data_storage.find(aligned_addr);
        assert(d_iter == data_storage.end());
    }

    void modifyData(uint64_t addr, uint8_t *data, unsigned size)
    {
        unsigned offset = (addr & ((uint64_t)block_size - (uint64_t)1));
        uint64_t aligned_addr = (addr & ~((uint64_t)block_size - (uint64_t)1));

        auto d_iter = data_storage.find(aligned_addr);
        assert(d_iter != data_storage.end());

        for (unsigned i = 0; i < size; i++)
        {
            assert(i + offset < block_size);
            (d_iter->second).new_data[i + offset] = data[i];
        }
    }

  protected:
    unsigned block_size;

    struct DUnit
    {
        std::vector<uint8_t> ori_data;
        std::vector<uint8_t> new_data;
    };

    std::unordered_map<uint64_t, DUnit> data_storage;
 
};
  
#endif
