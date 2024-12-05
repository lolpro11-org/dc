#ifndef DC_TPP
#define DC_TPP
#include "dc.hpp"

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    return serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::roundRobinAsync(const std::string& filename, const Args&... args) {
    return leastConnections().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif