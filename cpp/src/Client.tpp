#ifndef CLIENT_TPP
#define CLIENT_TPP
#include "Client.hpp"

#include "serialize.hpp"

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    Server::data& srvdata = this->getData();
    try {
        const ReturnType output = serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
        {
            std::lock_guard lock(srvdata.mut);
            srvdata.numThreads--;
        }
        return output;
    } catch(...) {
        // guarantee the count is decremented, even if an exception is thrown
        std::lock_guard lock(srvdata.mut);
        srvdata.numThreads--;
        throw; // allow the user to handle the error
    }
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    {
        Server::data& srvdata = this->getData();
        std::lock_guard lock(srvdata.mut);
        srvdata.numThreads++; // gets decremented when runExecAsFunction finishes
    }
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::distributeAndRun(const std::string& filename, const Args&... args) {
    return leastConnections().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif