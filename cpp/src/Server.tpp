#ifndef SERVER_TPP
#define SERVER_TPP
#include "Server.hpp"
#include "serialize.hpp"

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) try {
    const ReturnType output = serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
    this->getData().numThreads++;
    return output;
} catch(...) {
    // guarantee the count is decremented, even if an exception is thrown
    this->getData().numThreads++;
    throw; // allow the user to handle the error
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    this->getData().numThreads++; // gets decremented when runExecAsFunction finishes
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

#endif