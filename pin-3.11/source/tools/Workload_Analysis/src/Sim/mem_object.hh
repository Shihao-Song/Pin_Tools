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
    
    virtual void setNextLevel(MemObject *_next_level) { next_level = _next_level; }

    virtual void setId(int _id)
    {
        id = _id;
    }

    virtual void registerStats(Stats &stats) {}

    virtual void reInitialize() {}

  protected:
    MemObject *next_level;

    int id = -1;
};

#endif
