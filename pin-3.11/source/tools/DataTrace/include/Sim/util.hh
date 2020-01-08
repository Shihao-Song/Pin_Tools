#ifndef __UTIL_HH__
#define __UTIL_HH__

template<typename T>
static std::string to_string(T Number)
{
    std::ostringstream ss;
    ss << Number;
    return ss.str();
}

#endif
