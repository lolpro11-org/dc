#include <fstream>
#include <unordered_map>
#include <mutex>

#include "dc.hpp"
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

Server::Executable::Executable(): valid(false) {}
Server::Executable::Executable(const std::string& ip, const std::string& src): IPaddress(ip), handle(src), valid(true) {}
Server::Executable::Executable(Server::Executable&& src): IPaddress(std::move(src.IPaddress)), handle(std::move(src.handle)), valid(src.valid) {
    src.valid = false;
}
Server::Executable& Server::Executable::operator=(Server::Executable&& src) {
    if(this==&src) return *this;
    this->cleanup();
    this->handle = std::move(src.handle);
    this->IPaddress = std::move(src.IPaddress);
    this->valid = src.valid;
    src.valid = false;
    return *this;
}
Server::Executable::~Executable() {
    this->cleanup();
}
void Server::Executable::cleanup() {
    if(!(this->valid)) return;
    this->valid = false;
    RustString(c_remove_binary(this->IPaddress.c_str(), this->handle.c_str()));
}
const char* Server::Executable::c_str() const {
    return this->handle.c_str();
}

struct Server::data {
    size_t users;
    size_t numJobs;
    std::mutex srvmut;
    std::unordered_map<std::string, Executable> executables; // filenames, executable handles
};

Server::Server() {
    this->getData().users++;
}
Server::Server(const std::string& ip): IPaddress(ip) {
    this->getData().users++;
}
Server::Server(const Server& src): IPaddress(src.IPaddress) {
    this->getData().users++;
}

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
    if(--serverData.users!=0) return;
    serverData.executables.clear();
}

Server::~Server() {
    this->cleanup();
}

std::pair<uint8_t*, size_t> Server::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    uint8_t*const buffer = new uint8_t[size];
    file.read((char*)buffer, size);
    file.close();
    return std::pair<uint8_t*, size_t>(buffer, size);
}

void Server::removeExec(const std::string& filename) const {
    Server::data& serverData = this->getData();

    std::lock_guard lock(serverData.srvmut);

    std::unordered_map<std::string, Server::Executable>::iterator iter = serverData.executables.find(filename);
    if(iter==serverData.executables.cend()) return;
    serverData.executables.erase(iter);
}

// note, the buffer is created before the mutex is locked, which may lead to increased memory usage
// a fix would be to put it after the lock, but that would mean the file reading is no longer done in parallel
void Server::sendExec(const std::string& filename) const {
    Server::data& serverData = this->getData();

    std::pair<uint8_t*, size_t> buffer = Server::readFile(filename);
    {
        std::lock_guard lock(serverData.srvmut);
    
        std::unordered_map<std::string, Server::Executable>::iterator iter = serverData.executables.find(filename);
        if(iter!=serverData.executables.cend()) {
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
        if(iter==serverData.executables.cend()) {
            std::pair<uint8_t*, size_t> buffer = Server::readFile(filename);

            // the RustString is created to offload memory management responsibilities
            serverData.executables.emplace(filename, std::move(Executable(this->IPaddress, RustString(c_send_binary(this->IPaddress.c_str(), buffer.first, buffer.second)).cpp_str())));

            delete[] buffer.first;

            iter = serverData.executables.find(filename);
        }
    }
    Server::Executable& execHandle = iter->second;

    size_t stdoutLength = 0;
    const char** argv = new const char*[args.size()];
    for(size_t i=0; i<args.size(); i++) {
        argv[i] = args[i].c_str();
    }

    {
        std::lock_guard lock(serverData.srvmut);
        serverData.numJobs++;
    }
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), execHandle.c_str(), argv, args.size(), (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength);
    {
        std::lock_guard lock(serverData.srvmut);
        if(serverData.numJobs>0) serverData.numJobs--;
    }
    delete[] argv;
    std::string stdout_str((const char*)stdoutVec, stdoutLength);
    free_vec((void*)stdoutVec, (size_t)0, (size_t)0);
    return stdout_str;
}

size_t Server::getNumJobs() const {
    return this->getData().numJobs;
}
 
Server::data& Server::getData() const {
    static std::mutex datamut; // initalization is thread safe

    // servers was initially a static member of the class Server,
    // but it was the victim of the Static Initialization Order Fiasco.
    // to fix this, its a static variable only accessible by this function.
    // so now it is only initalized when it is first required
    // long story short, this prevents a Floating Point Exception error.

    static std::unordered_map<std::string, Server::data> servers;

    std::lock_guard lock(datamut);
    return servers[this->IPaddress];
}

Client::Client(const std::vector<Server>& servers): machines(servers) {}
Client::Client(std::initializer_list<Server> servers): machines(servers) {}

Client::Client(const Client& src): machines(src.machines) {}

Client& Client::operator=(const Client& src) {
    if(this==&src) return *this;
    this->machines = src.machines;
    return *this;
}

Client::~Client() {}

size_t Client::numMachines() const {
    return this->machines.size();
}

Server& Client::getMachine(const size_t index) {
    return this->machines[index];
}

Server& Client::leastConnections() {
    size_t minJobs = machines[0].getNumJobs();
    size_t minIter = 0;
    for(size_t i=1; i<machines.size(); i++) {
        const size_t numJobs = machines[i].getNumJobs();
        if(minJobs <= numJobs) continue;
        minJobs = numJobs;
        minIter = i;
    }
    return machines[minIter];
}