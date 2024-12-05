#ifndef DC_HPP
#define DC_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <future>
#include <mutex>

#include "serialize.hpp"
#include "../my_header.h"

class RustString {
    private:
    const char* str;

    public:
    RustString(const char*);
    RustString(RustString&&);
    ~RustString();
    RustString& operator=(RustString&&);
    const char* c_str() const;
    std::string cpp_str() const;

    RustString(const RustString&) = delete;
    RustString& operator=(const RustString&) = delete;
};

class Server {
    private:

    class Executable;
    struct data;

    std::string IPaddress;

    static std::mutex datamut;
    static std::unordered_map<std::string, Server::data>* servers;

    static std::pair<uint8_t*, size_t> readFile(const std::string&);
    Server::data& getData() const;
    Server::data& unsafe_getData() const;

    void cleanup();

    public:
    Server();
    Server(const std::string&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server();
    void sendExec(const std::string&);
    void removeExec(const std::string&);
    std::string runExec(const std::string& filename, const std::string& stdin_str = "", const std::vector<std::string>& args = {});
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
    size_t getNumJobs() const;
};

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
    std::mutex srvmut;
    std::unordered_map<std::string, Executable> executables; // filenames, executable handles
    size_t numJobs;
};

class Client {
    private:
    std::vector<Server> machines;

    Server& leastConnections();

    public:
    Client(const std::vector<Server>&);
    Client(std::initializer_list<Server>);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client();

    size_t numMachines() const;
    Server& getMachine(const size_t);
    template<typename ReturnType, typename... Args> std::future<ReturnType> roundRobinAsync(const std::string&, const Args&...);
};
#include "dc.tpp"

#endif