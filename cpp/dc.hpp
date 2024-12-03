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

class Server {
    private:
    std::string IPaddress;
    // filenames, executable handles
    std::unordered_map<std::string, std::string> executables;

    public:
    Server(const std::string&);
    Server(Server&&);
    Server& operator=(Server&&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server();
    void sendExec(const std::string&);
    void removeExec(const std::string&);
    std::string runExec(const std::string& filename, const std::string& stdin_str = "", const std::vector<std::string>& args = {});
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
};

Server::Server(const std::string& ip): IPaddress(ip) {}
Server::Server(const Server& src): IPaddress(src.IPaddress), executables(src.executables) {}
Server::Server(Server&& src): IPaddress(std::move(src.IPaddress)), executables(std::move(src.executables)) {}

Server& Server::operator=(const Server& src) {
    if(this==&src) return *this;
    this->IPaddress = src.IPaddress;
    this->executables = src.executables;
    return *this;
}

Server& Server::operator=(Server&& src) {
    if(this==&src) return *this;
    this->IPaddress = std::move(src.IPaddress);
    this->executables = std::move(src.executables);
    return *this;
}

Server::~Server() {}

void Server::removeExec(const std::string& filename) {
    auto iter = this->executables.find(filename);
    if(iter==this->executables.end()) return;
    RustString(c_remove_binary(this->IPaddress.c_str(), iter->second.c_str()));
    this->executables.erase(iter);
}

void Server::sendExec(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    // note: should change buffer to a pointer instead of a vector
    std::vector<uint8_t> buffer(size);
    file.read((char*)buffer.data(), size);
    file.close();
    this->removeExec(filename);
    // the RustString is created to offload memory management responsibilities
    this->executables.insert({filename, RustString(c_send_binary(this->IPaddress.c_str(), buffer.data(), size)).cpp_str()});
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str, const std::vector<std::string>& args) {
    if(this->executables.find(filename)==this->executables.end()) {
        this->sendExec(filename);
    }
    auto iter = this->executables.find(filename);
    const char** argv = new const char*[args.size()];
    for(size_t i=0; i<args.size(); i++) {
        argv[i] = args[i].c_str();
    }
    size_t stdoutLength = 0;
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), iter->second.c_str(), argv, args.size(), (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength);
    delete[] argv;
    std::string stdout_str((const char*)stdoutVec, stdoutLength);
    free_vec((void*)stdoutVec, (size_t)0, (size_t)0);
    return stdout_str;
}

template<typename ReturnType, typename... Args> ReturnType Server::runExecAsFunction(const std::string& filename, const Args&... args) {
    return serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(args...)));
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Server::runExecAsAsyncFunction(const std::string& filename, const Args&... args) {
    return std::async(std::launch::async, [this](const std::string filename, const Args... lambdaArgs)->ReturnType {
        return serial::deserializeFromString<ReturnType>(this->runExec(filename, serial::serializeToString(lambdaArgs...)));
    }, filename, args...);
}

class Client {
    private:
    std::vector<Server> machines;
    size_t jobNumber;

    Server& roundRobinNext();

    public:
    Client(const std::vector<Server>&);
    Client(std::initializer_list<Server>);
    Client(Client&&);
    Client& operator=(Client&&);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client();

    size_t numMachines() const;
    Server& getMachine(const size_t);
    template<typename ReturnType, typename... Args> std::future<ReturnType> roundRobinAsync(const std::string&, const Args&...);
};

Client::Client(const std::vector<Server>& servers): machines(servers), jobNumber(0) {}
Client::Client(std::initializer_list<Server> servers): machines(servers), jobNumber(0) {}

Client::Client(Client&& src): machines(std::move(src.machines)), jobNumber(std::move(src.jobNumber)) {}

Client& Client::operator=(Client&& src) {
    if(this==&src) return *this;
    this->machines = std::move(src.machines);
    this->jobNumber = std::move(src.jobNumber);
    return *this;
}

Client::Client(const Client& src): machines(src.machines) {}

Client& Client::operator=(const Client& src) {
    if(this==&src) return *this;
    this->machines = src.machines;
    this->jobNumber = src.jobNumber;
    return *this;
}

Client::~Client() {}

size_t Client::numMachines() const {
    return machines.size();
}

Server& Client::getMachine(const size_t index) {
    return machines[index];
}

Server& Client::roundRobinNext() {
    this->jobNumber++;
    if(this->jobNumber>=this->numMachines()) this->jobNumber = 0;
    return machines[this->jobNumber];
}

template<typename ReturnType, typename... Args> std::future<ReturnType> Client::roundRobinAsync(const std::string& filename, const Args&... args) {
    return roundRobinNext().runExecAsAsyncFunction<ReturnType, Args...>(filename, args...);
}

#endif