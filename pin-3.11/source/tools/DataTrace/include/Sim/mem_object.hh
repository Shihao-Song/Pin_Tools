#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include <fstream>

#include "request.hh"
#include "stats.hh"

class MemObject
{
  public:
    MemObject(){}
    ~MemObject()
    {}

    virtual bool send(Request &req) = 0;

    virtual void setPrevLevel(MemObject *_prev_level) { prev_level = _prev_level; } 

    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }

    virtual void loadBlock(uint64_t _addr, uint8_t *source, unsigned int size) {}

    virtual void modifyBlock(uint64_t _addr, uint8_t *data, unsigned int size) {}

    virtual void getBlock(uint64_t _addr, 
                          std::vector<uint8_t> &ori_data,
                          std::vector<uint8_t> &new_data) {}

    virtual void setBlock(uint64_t _addr, 
                          std::vector<uint8_t> &ori_data,
                          std::vector<uint8_t> &new_data) {}

    virtual void inval(uint64_t _addr) {}

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void registerStats(Stats &stats) {}

    virtual void reInitialize() {}

  protected:
    MemObject *prev_level = nullptr;
    MemObject *next_level = nullptr;

    int id = -1;
};

#endif
