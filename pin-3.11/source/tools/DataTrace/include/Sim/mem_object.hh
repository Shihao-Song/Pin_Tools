#ifndef __MEM_OBJECT_HH__
#define __MEM_OBJECT_HH__

#include <fstream>

#include "request.hh"
#include "stats.hh"

#include "../Sim/data.hh"

using std::ofstream;

class MemObject
{
  public:
    MemObject(){}
    virtual ~MemObject() {}

    virtual bool send(Request &req) = 0;

    virtual void setPrevLevel(MemObject *_prev_level) { prev_levels.push_back(_prev_level); } 

    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }

    virtual void inval(uint64_t _addr) {}

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void setStorageUnit(Data *_data) { data = _data; }

    virtual void traceOutput(ofstream *_out) { trace_out = _out; }

    virtual void registerStats(Stats &stats) {}

    virtual void reInitialize() {}

  protected:
    Data *data = nullptr;

    std::vector<MemObject*> prev_levels;
    MemObject *next_level = nullptr;

    int id = -1;

    ofstream *trace_out;    
};

#endif
