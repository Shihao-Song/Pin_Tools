#ifndef __CACHE_REPLACEMENT_POLICY_HH__
#define __CACHE_REPLACEMENT_POLICY_HH__

#include "../../cache_blk.hh"

namespace CacheSimulator
{
template<class T>
class ReplacementPolicy
{
  public:
    ReplacementPolicy() {}

    virtual void upgrade(T *blk, Tick cur_clk = 0) {}
    virtual void downgrade(T *blk) {}
};	

class SetWayAssocReplacementPolicy
      : public ReplacementPolicy<SetWayBlk>
{
  public:
    SetWayAssocReplacementPolicy()
        : ReplacementPolicy()
    {}

    virtual std::pair<bool, SetWayBlk*> findVictim(const std::vector<SetWayBlk *>&set)
                                        = 0; 
};
}
#endif
