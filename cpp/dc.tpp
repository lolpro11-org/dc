#ifndef DC_TPP
#define DC_TPP
#include "dc.hpp"

#include "serialize.hpp"

class Server::Executable {
    private:
    std::string IPaddress;
    std::string handle;
    bool valid;

    void cleanup();

    public:
    const char* c_str() const;
    
    Executable();
    Executable(const std::string&, const std::string&);
    Executable(Executable&&);
    Executable& operator=(Executable&&);
    ~Executable();

    Executable(const Executable&) = delete;
    Executable& operator=(const Executable&) = delete;
};

struct Server::data {
    size_t users;
    size_t numJobs;
    size_t numThreads;
    std::mutex srvmut;
    std::unordered_map<std::string, Executable> executables; // filenames, executable handles
};

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    ReturnType output = serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
    Server::data& srvdata = this->getData();
    {
        std::lock_guard lock(srvdata.srvmut);
        srvdata.numThreads--;
    }
    return output;
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    Server::data& srvdata = this->getData();
    {
        std::lock_guard lock(srvdata.srvmut);
        srvdata.numThreads++; // gets decremented when runExecAsFunction finishes
    }
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::distributeAndRun(const std::string& filename, const Args&... args) {
    return leastConnections().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif