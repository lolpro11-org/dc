#ifndef DC_TPP
#define DC_TPP
#include "dc.hpp"

#include "serialize.hpp"

class Server::Executable {
    friend class Server;
    private:
    std::string IPaddress;
    std::string handle;
    bool valid;

    static std::pair<const uint8_t*, size_t> readFile(const std::string&);

    public:

    std::string operator()(const std::string) const;
    operator bool() const;
    
    Executable();
    // ip address, filename
    Executable(const std::string&, const std::string&);
    Executable(Executable&&);
    Executable& operator=(Executable&&);
    ~Executable() noexcept;

    Executable(const Executable&) = delete;
    Executable& operator=(const Executable&) = delete;
};

struct Server::data {
    size_t users;
    size_t numThreads;
    std::mutex mut;
    std::unordered_map<std::string, Server::Executable> executables; // filenames, executable handles
};

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    Server::data& srvdata = this->getData();
    try {
        ReturnType output = serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
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
    Server::data& srvdata = this->getData();
    {
        std::lock_guard lock(srvdata.mut);
        srvdata.numThreads++; // gets decremented when runExecAsFunction finishes
    }
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::distributeAndRun(const std::string& filename, const Args&... args) {
    return leastConnections().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif