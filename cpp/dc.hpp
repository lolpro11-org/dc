#ifndef DC_HPP
#define DC_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <functional>
#include <tuple>
#include <fstream>
#include <future>
#include <mutex>

#include <unistd.h>
#include <limits.h>
#include <iostream>

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
RustString::RustString(const char* rstr) {
    this->str = rstr;
}
RustString::RustString(RustString&& ruststr) {
    this->str = ruststr.str;
    ruststr.str = nullptr;
}
RustString& RustString::operator=(RustString&& ruststr) {
    if(this==&ruststr) return *this;
    this->str = ruststr.str;
    ruststr.str = nullptr;
    return *this;
}
RustString::~RustString() {
    if(this->str == nullptr) return;
    free_string(this->str);
    this->str = nullptr;
}
const char* RustString::c_str() const {
    return this->str;
}
std::string RustString::cpp_str() const {
    return std::string(this->str);
}

class Executable {
    private:
    std::string IPaddress;
    void cleanup();
    public:
    std::string handle;
    bool valid;
    Executable();
    Executable(const std::string&, const std::string&);
    Executable(Executable&&);
    Executable& operator=(Executable&&);
    ~Executable();

    Executable(const Executable&) = delete;
    Executable& operator=(const Executable&) = delete;
};

Executable::Executable(): valid(false) {}
Executable::Executable(const std::string& ip, const std::string& src): IPaddress(ip), handle(src), valid(true) {}
Executable::Executable(Executable&& src): IPaddress(std::move(src.IPaddress)), handle(std::move(src.handle)), valid(src.valid) {
    src.valid = false;
}
Executable& Executable::operator=(Executable&& src) {
    if(this==&src) return *this;
    this->cleanup();
    this->handle = std::move(src.handle);
    this->IPaddress = std::move(src.IPaddress);
    this->valid = src.valid;
    src.valid = false;
    return *this;
}
Executable::~Executable() {
    this->cleanup();
}
void Executable::cleanup() {
    if(!(this->valid)) return;
    this->valid = false;
    RustString(c_remove_binary(this->IPaddress.c_str(), this->handle.c_str()));
}

class Server {
    private:

    struct data {
        size_t users;
        std::mutex srvmut;
        std::unordered_map<std::string, Executable> executables; // filenames, executable handles
        std::unordered_map<std::string, size_t> jobs; // filenames, number of jobs with that filename
    };

    std::string IPaddress;

    static std::mutex datamut;
    static std::unordered_map<std::string, Server::data> servers;

    static std::pair<uint8_t*, size_t> readFile(const std::string&);

    Server::data& getData() const;

    void cleanup();

    public:
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
std::mutex Server::datamut;
std::unordered_map<std::string, Server::data> Server::servers;

Server::Server(const std::string& ip): IPaddress(ip) {
    this->getData().users++;
}
Server::Server(const Server& src): IPaddress(src.IPaddress) {
    this->getData().users++;
}

// treat src as if a move happened
Server& Server::operator=(const Server& src) {
    if(this==&src) return *this;
    src.getData().users++;
    this->cleanup();
    this->IPaddress = src.IPaddress;
    
    return *this;
}

void Server::cleanup() {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.srvmut);
    serverData.users--;
    if(serverData.users!=0) return;
    serverData.executables.clear();
}

Server::~Server() {
    this->cleanup();
}

std::pair<uint8_t*, size_t> Server::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    uint8_t*const buffer = new uint8_t[size];
    file.read((char*)buffer, size);
    file.close();
    return std::pair<uint8_t*, size_t>(buffer, size);
}

void Server::removeExec(const std::string& filename) {
    Server::data& serverData = this->getData();

    std::lock_guard lock(serverData.srvmut);

    auto iter = serverData.executables.find(filename);
    if(iter==serverData.executables.end()) return;
    serverData.executables.erase(iter);
}

void Server::sendExec(const std::string& filename) {
    std::pair<uint8_t*, size_t> buffer = Server::readFile(filename);

    {
        Server::data& serverData = this->getData();
        std::lock_guard lock(serverData.srvmut);
    
        auto iter = serverData.executables.find(filename);
        if(iter!=serverData.executables.end()) {
            serverData.executables.erase(iter);
        }
        // the RustString is created to offload memory management responsibilities
        serverData.executables.emplace(filename, std::move(Executable(this->IPaddress, RustString(c_send_binary(this->IPaddress.c_str(), buffer.first, buffer.second)).cpp_str())));
    }

    delete[] buffer.first;
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str, const std::vector<std::string>& args) {
    Server::data& serverData = this->getData();

    std::unordered_map<std::string, Executable>::iterator iter;
    {
        std::lock_guard lock(serverData.srvmut);
        iter = serverData.executables.find(filename);
        if(iter==serverData.executables.end()) {
            std::pair<uint8_t*, size_t> buffer = Server::readFile(filename);

            // the RustString is created to offload memory management responsibilities
            serverData.executables.emplace(filename, std::move(Executable(this->IPaddress, RustString(c_send_binary(this->IPaddress.c_str(), buffer.first, buffer.second)).cpp_str())));

            delete[] buffer.first;

            iter = serverData.executables.find(filename);
        }
    }
    Executable& execHandle = iter->second;

    size_t stdoutLength = 0;
    const char** argv = new const char*[args.size()];
    for(size_t i=0; i<args.size(); i++) {
        argv[i] = args[i].c_str();
    }

    {
        std::lock_guard lock(serverData.srvmut);
        serverData.jobs[filename]++;
    }
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), execHandle.handle.c_str(), argv, args.size(), (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength);
    {
        std::lock_guard lock(serverData.srvmut);
        if(serverData.jobs[filename]>0) serverData.jobs[filename]--;
    }
    delete[] argv;
    std::string stdout_str((const char*)stdoutVec, stdoutLength);
    free_vec((void*)stdoutVec, (size_t)0, (size_t)0);
    return stdout_str;
}

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    return serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    return std::async(std::launch::async, &Server::runExecAsFunction<ReturnType, Args...>, *this, filename, args...);
}

size_t Server::getNumJobs() const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(Server::datamut);
    size_t total = 0;
    for(const std::pair<const std::string, size_t>& executableNames : serverData.jobs) {
        total += executableNames.second;
    }
    return total;
}

Server::data& Server::getData() const {
    std::lock_guard lock(Server::datamut);
    return Server::servers[this->IPaddress];
}

class Client {
    private:
    std::vector<Server> machines;
    std::vector<size_t> weights;
    size_t counts;
    size_t machineIndex;

    Server& roundRobinNext();

    public:
    Client(const std::vector<Server>&);
    Client(const std::vector<Server>&, const std::vector<size_t>&);
    Client(std::initializer_list<Server>);
    Client(std::initializer_list<Server>, std::initializer_list<size_t>);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client();

    size_t numMachines() const;
    Server& getMachine(const size_t);
    template<typename ReturnType, typename... Args> std::future<ReturnType> roundRobinAsync(const std::string&, const Args&...);
};

Client::Client(const std::vector<Server>& servers): machines(servers), weights(servers.size(), 1), counts(0), machineIndex(0) {}
Client::Client(const std::vector<Server>& servers, const std::vector<size_t>& weight): machines(servers), weights(weight), counts(0), machineIndex(0) {}
Client::Client(std::initializer_list<Server> servers): machines(servers), weights(servers.size(), 1), counts(0), machineIndex(0) {}
Client::Client(std::initializer_list<Server> servers, std::initializer_list<size_t> weight): machines(servers), weights(weight), counts(0), machineIndex(0) {}

Client::Client(const Client& src): machines(src.machines), weights(src.weights), counts(src.counts), machineIndex(src.machineIndex) {}

Client& Client::operator=(const Client& src) {
    if(this==&src) return *this;
    this->machines = src.machines;
    this->machineIndex = src.machineIndex;
    this->weights = src.weights;
    this->counts = src.counts;
    return *this;
}

Client::~Client() {}

size_t Client::numMachines() const {
    return this->machines.size();
}

Server& Client::getMachine(const size_t index) {
    return this->machines[index];
}

Server& Client::roundRobinNext() {
    if(this->machineIndex>=this->numMachines()) {
        this->machineIndex = 0;
        this->counts = 0;
    }
    const size_t machine = this->machineIndex;

    this->counts++;
    if(this->counts >= this->weights[this->machineIndex]) {
        this->counts = 0;
        this->machineIndex++;
    }
    return this->machines[machine];
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::roundRobinAsync(const std::string& filename, const Args&... args) {
    return roundRobinNext().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif