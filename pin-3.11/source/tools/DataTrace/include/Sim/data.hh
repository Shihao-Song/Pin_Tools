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
        // The address must be block(cache-line) aligned.
        assert(aligned_addr == (aligned_addr & ~((uint64_t)block_size - (uint64_t)1)));
        // Loading must be the block(cache-line) size.
        assert(size == block_size);

        auto d_iter = data_storage.find(aligned_addr);
        // We only load data if there is miss from all the caches.
        assert(d_iter == data_storage.end());

        DUnit storage;
        for (unsigned i = 0; i < size; i++)
        {
            storage.ori_data.push_back(data[i]);
            storage.new_data.push_back(data[i]);
        }

        // Insert the data.
        data_storage.insert({aligned_addr, storage});
    }

    void getData(uint64_t aligned_addr,
                 std::vector<uint8_t> &ori_data,
                 std::vector<uint8_t> &new_data)
    {
        // The loading address must be block(cache-line) aligned.
        assert(aligned_addr == (aligned_addr & ~((uint64_t)block_size - (uint64_t)1)));
        
	auto d_iter = data_storage.find(aligned_addr);
        // The data has to be there.
        assert(d_iter != data_storage.end());

        for (unsigned i = 0; i < block_size; i++)
        {
            ori_data.push_back((d_iter->second).ori_data[i]);
            new_data.push_back((d_iter->second).new_data[i]);
        }
    }

    void deleteData(uint64_t aligned_addr)
    {
        // The address must be block(cache-line) aligned.
        assert(aligned_addr == (aligned_addr & ~((uint64_t)block_size - (uint64_t)1)));

        auto d_iter = data_storage.find(aligned_addr);
        // The data has to be there.
        assert(d_iter != data_storage.end());

        // Erase.
        data_storage.erase(aligned_addr);
        d_iter = data_storage.find(aligned_addr);
        assert(d_iter == data_storage.end());
    }

    void modifyData(uint64_t addr, uint8_t *data, unsigned size)
    {
        unsigned offset = (addr & ((uint64_t)block_size - (uint64_t)1));
        uint64_t aligned_addr = (addr & ~((uint64_t)block_size - (uint64_t)1));

        auto d_iter = data_storage.find(aligned_addr);
        // Still, the data has to be there.
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
