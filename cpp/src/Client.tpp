#ifndef CLIENT_TPP
#define CLIENT_TPP
#include "Client.hpp"

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::distributeAndRun(const std::string& filename, const Args&... args) {
    return leastConnections().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif