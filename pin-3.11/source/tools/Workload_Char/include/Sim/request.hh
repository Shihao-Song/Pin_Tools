#ifndef __REQUEST_HH__
#define __REQUEST_HH__

#include <cstdint> // Addr
#include <functional>
#include <list>
#include <memory>
#include <vector>

typedef uint64_t Addr;
typedef uint64_t Tick;

struct Request
{
  public:
    int core_id;

    Addr eip; // Advanced feature, the instruction that caused this memory request;

    Addr addr; // The address we are trying to read or write

    bool instr_loading = false; // Any instruction loading should not be in the critical path.

    // (Memory) request type
    enum class Request_Type : int
    {
        READ,
        WRITE,
        WRITE_BACK,
        MAX
    }req_type;

    /* Constructors */
    Request() : addr(0), req_type(Request_Type::MAX)
    {}

    Request(Addr _addr, Request_Type _type) :
        addr(_addr),
        req_type(_type)
    {}
};
#endif
