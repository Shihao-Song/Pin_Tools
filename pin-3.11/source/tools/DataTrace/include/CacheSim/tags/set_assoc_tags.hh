#ifndef __CACHE_SET_ASSOC_TAGS_HH__
#define __CACHE_SET_ASSOC_TAGS_HH__

#include <assert.h>

#include "../cache_blk.hh"

#include "cache_tags.hh"
#include "replacement_policies/set_way_lru.hh"

namespace CacheSimulator
{
class TagsWithSetWayBlk : public Tags<SetWayBlk>
{
  public:
    TagsWithSetWayBlk(int level, Config &cfg) :
        Tags(level, cfg) {}
};

template<typename P>
class SetWayAssocTags : public TagsWithSetWayBlk
{
  protected:
    const int assoc;

    const uint32_t num_sets;

    const int set_shift;

    const unsigned set_mask;

    const int tag_shift;

    std::vector<std::vector<SetWayBlk *>> sets;

    P policy;

  public:
    SetWayAssocTags(int level, Config &cfg)
        : TagsWithSetWayBlk(level, cfg),
          assoc(cfg.caches[level].assoc),
          num_sets(size / (block_size * assoc)),
          set_shift(log2(block_size)),
          set_mask(num_sets - 1),
          tag_shift(set_shift + log2(num_sets)),
          sets(num_sets)
    {
        for (uint32_t i = 0; i < num_sets; i++)
        {
            sets[i].resize(assoc);
        }

        tagsInit();
    }

    std::pair<bool, Addr> accessBlock(Addr addr, bool modify, Tick cur_clk = 0) override
    {
        bool hit = false;
        Addr blk_aligned_addr = blkAlign(addr);
        
        SetWayBlk *blk = findBlock(blk_aligned_addr);

        // If there is hit, upgrade
        if (blk != nullptr)
        {
            hit = true;
            policy.upgrade(blk, cur_clk);

            if (modify) { blk->setDirty(); }
        }
        return std::make_pair(hit, blk_aligned_addr);
    }

    std::pair<bool, Addr> insertBlock(Addr addr, bool modify, Tick cur_clk = 0) override
    {
        // Find a victim block 
        auto ret = findVictim(addr);

        bool wb_required = ret.wb_required;
        Addr victim_addr = ret.wb_addr;
        SetWayBlk *victim = ret.victim;

        if (modify) { victim->setDirty(); }
        victim->insert(extractTag(addr));
        policy.upgrade(victim, cur_clk);

        /*
        std::cout << level_str << "; ";
        std::cout << "Inserted: " << addr << "; ";
        if (wb_required)
        {
            std::cout << "Evicted: " << victim_addr << "\n";
        }
        else
        {
            std::cout << "\n";
        }
        */

        return std::make_pair(wb_required, victim_addr);
    }
   
    void inval(uint64_t _addr) override
    {
        // assert(_addr == blkAlign(_addr));
        SetWayBlk *blk = findBlock(_addr);
        if (blk != nullptr)
        {
            invalidate(blk);
            blk->ori_data.clear();
            blk->new_data.clear();
            assert(blk->ori_data.size() == 0);
            assert(blk->new_data.size() == 0);
        }
    }
    
    void modifyBlock(uint64_t _addr, uint8_t *data, unsigned int size) override
    {
        unsigned block_offset = _addr & block_mask;
	
        Addr blk_aligned_addr = blkAlign(_addr);
        SetWayBlk *blk = findBlock(blk_aligned_addr);
        // assert(blk != nullptr);
        if (blk == nullptr) { std::cerr << blk_aligned_addr << "\n"; exit(0); }

        assert(blk->ori_data.size() != 0);
        assert(blk->new_data.size() != 0);

        for (unsigned int i = 0; i < size; i++)
        {
            blk->new_data[block_offset + i] = data[i];
        }

        /*	
        std::cout << "New data for " << _addr <<  ", offset " << block_offset << ", size " << size << "\n";
        for (unsigned int i = 0; i < block_size - 1; i++)
        {
            std::cout << int(blk->new_data[i]) << " ";
        }
        std::cout << int(blk->new_data[block_size - 1]) << "\n";
        */
    }

    void loadBlock(uint64_t _addr, uint8_t *data, unsigned int size) override
    {
        Addr blk_aligned_addr = blkAlign(_addr);
        SetWayBlk *blk = findBlock(blk_aligned_addr);
        assert(blk != nullptr);

        assert(blk->ori_data.size() == 0);
        assert(blk->new_data.size() == 0);

        blk->ori_data.resize(size);
        blk->new_data.resize(size);
        for (unsigned int i = 0; i < size; i++)
        {
            blk->ori_data[i] = data[i];
            blk->new_data[i] = data[i];
        }

	/*
        std::cout << level_str << " data for " << blk_aligned_addr << "\n";
        std::cout << "Old data: \n";
        for (unsigned int i = 0; i < size - 1; i++)
        {
            std::cout << int(blk->ori_data[i]) << " ";
        }
        std::cout << int(blk->ori_data[size - 1]) << "\n";
	
        std::cout << "New data: \n";
        for (unsigned int i = 0; i < size - 1; i++)
        {
            std::cout << int(blk->new_data[i]) << " ";
        }
        std::cout << int(blk->new_data[size - 1]) << "\n";
        */
    }
   
    void getBlock(uint64_t _addr,
                  std::vector<uint8_t> &ori_data,
                  std::vector<uint8_t> &new_data) override
    {
        SetWayBlk *blk = findBlock(_addr);
        assert(blk != nullptr);

        assert(blk->ori_data.size() != 0);
        assert(blk->new_data.size() != 0);
        assert(blk->ori_data.size() == blk->new_data.size());

        ori_data.resize(blk->ori_data.size());
        new_data.resize(blk->new_data.size());

        for (unsigned int i = 0; i < blk->ori_data.size(); i++)
        {
            ori_data[i] = blk->ori_data[i];
            new_data[i] = blk->new_data[i];
        }
    }

    void setBlock(uint64_t _addr,
                  std::vector<uint8_t> &ori_data,
                  std::vector<uint8_t> &new_data) override
    {
        SetWayBlk *blk = findBlock(_addr);
        assert(blk != nullptr);

        assert(blk->ori_data.size() == 0);
        assert(blk->new_data.size() == 0);

        blk->ori_data.resize(ori_data.size());
        blk->new_data.resize(new_data.size());

        for (unsigned int i = 0; i < ori_data.size(); i++)
        {
            blk->ori_data[i] = ori_data[i];
            blk->new_data[i] = new_data[i];
        }
    }

    void clearData(uint64_t _addr) override
    {
        SetWayBlk *blk = findBlock(_addr);
        assert(blk != nullptr);

        blk->ori_data.clear();
        blk->new_data.clear();
        assert(blk->ori_data.size() == 0);
        assert(blk->new_data.size() == 0);
    }

    void printTagInfo() override
    {
        std::cout << "Assoc: " << assoc << "\n";
        std::cout << "Number of sets: " << num_sets << "\n";
    }

  protected:
    void tagsInit() override
    {
        for (unsigned i = 0; i < num_blocks; i++)
        {
            SetWayBlk *blk = &blks[i];
            uint32_t set = i / assoc;
            uint32_t way = i % assoc;

            sets[set][way] = blk;
            blk->setPosition(set, way);
        }
    }

    Addr extractSet(Addr addr) const
    {
        return (addr >> set_shift) & set_mask;
    }

    Addr extractTag(Addr addr) const override
    {
        return (addr >> tag_shift);
    }

    Addr regenerateAddr(SetWayBlk *blk) const override
    {
        return (blk->tag << tag_shift) | (blk->getSet() << set_shift);
    }

    SetWayBlk* findBlock(Addr addr) const override
    {
        // Extract block tag
        Addr tag = extractTag(addr);

        // Extract the set
        const std::vector<SetWayBlk *> set = sets[extractSet(addr)];

        for (const auto& way : set)
        {
            if (way->tag == tag && way->isValid())
            {
                return way;
            }
        }
        return nullptr;
    }

    victimRet findVictim(Addr addr) override
    {
        // Extract the set
        const std::vector<SetWayBlk *> set = sets[extractSet(addr)];

        // Get the victim block based on replacement policy
        auto ret = policy.findVictim(set);

	bool wb_required = ret.first;
        SetWayBlk *victim = ret.second;
        assert(victim != nullptr);

        // Addr victim_addr = MaxAddr;
        if (wb_required)
        {
            assert(victim->isDirty());
        }
        Addr victim_addr = regenerateAddr(victim);

        if (victim->isValid())
        {
            invalidate(victim);
        }

        return victimRet{wb_required, victim_addr, victim};
    }

    void invalidate(SetWayBlk* victim) override
    {
        victim->invalidate();
        victim->clearDirty();
        policy.downgrade(victim);
        assert(!victim->isValid());
    }
};
typedef SetWayAssocTags<SetWayAssocLRU> LRUSetWayAssocTags;
}

#endif
