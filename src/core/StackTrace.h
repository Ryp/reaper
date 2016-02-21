#ifndef STACKTRACE_INCLUDED
#define STACKTRACE_INCLUDED

void printStacktrace();

#include <vector>
#include <string>

/*
 * Base stacktrace class, could be used to make an uniform API between platforms
 */
class StackTrace
{
public:
    StackTrace();

public:
    void    pushFrame(unsigned int pc, const std::string& symbol, unsigned int offset);
    const std::string& operator[](std::size_t idx) const;

private:
    std::vector<std::string>   _trace;
};

#endif // STACKTRACE_INCLUDED
