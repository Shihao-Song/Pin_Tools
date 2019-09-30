#ifndef __CACHE_BLK_HH__
#define __CACHE_BLK_HH__

#include <cassert>
#include <cstdint>

#include "request.hh"

namespace CacheSimulator
{
class Blk
{
  public:
    // Initially, all blks should not be valid
    Blk() : valid(0), dirty(0), when_touched(0) {}

    void insert(const Addr _tag)
    {
        assert(valid == 0); // Should never insert to a valid block
        this->tag = _tag;
        valid = 1;
    }
    
    void invalidate()
    {
        valid = 0;
    }

    bool isValid()
    {
        return valid;
    }

    void setDirty() { dirty = 1; }
    void clearDirty() { dirty = 0; }

    bool isDirty() { return dirty; }

    Addr tag;

    bool valid;

    bool dirty; // Has the brought in cache-line been modified yet?

    Tick when_touched; // Last clock tick the Block is touched.

    // Advanced features, record instruction (EIP) that brings this block
    int core_id = -1;
    Addr eip;
};

class SetWayBlk : public Blk
{
  public:
    SetWayBlk() : Blk() {}

    void setPosition(const uint32_t _set, const uint32_t _way) 
    {
        set = _set;
        way = _way;
    }

    uint32_t getSet() const { return set; }

    uint32_t set; // Which set this entry belongs
    uint32_t way; // Which way (within set) this entry belongs
};

class FABlk : public Blk
{
  public:
    FABlk() : Blk(), prev(nullptr), next(nullptr) {}

    /*
     * prev and next are determined by the replacement policy. For example,
     * when LRU is used, prev means the previous block in LRU order.
     *
     * prev (recently used), block, next (least recently used)
     * */

    FABlk *prev;

    FABlk *next;
};
}

#endif
