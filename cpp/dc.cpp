#include <fstream>
#include <unordered_map>
#include <mutex>
#include <exception>
#include <stdexcept>

#include "dc.hpp"
#include "../my_header.h"

class RustString {
    private:
    const char* str;

    public:
    bool valid() const noexcept;
    constexpr RustString() noexcept ;
    RustString(const char*) noexcept;
    RustString(RustString&&) noexcept;
    ~RustString() noexcept;
    RustString& operator=(RustString&&) noexcept;
    const char* c_str() const noexcept;
    std::string cpp_str() const;

    RustString(const RustString&) = delete;
    RustString& operator=(const RustString&) = delete;
};

constexpr RustString::RustString() noexcept: str(nullptr) {}

RustString::RustString(const char* rstr) noexcept: str(rstr) {}
RustString::RustString(RustString&& ruststr) noexcept: str(ruststr.str) {
    ruststr.str = nullptr; // transfers ownership to this string
}
bool RustString::valid() const noexcept {
    return this->str!=nullptr;
}
RustString& RustString::operator=(RustString&& ruststr) noexcept {
    if(this==&ruststr) return *this;
    this->str = ruststr.str;
    ruststr.str = nullptr;
    return *this;
}
RustString::~RustString() noexcept {
    if(this->str == nullptr) return;
    free_string(this->str);
    this->str = nullptr;
}
const char* RustString::c_str() const noexcept {
    return this->str;
}
// may throw as it calls the C++ string constructor
std::string RustString::cpp_str() const {
    return std::string(this->valid() ? this->str : "");
}

Server::Executable::Executable() noexcept: valid(false) {}
Server::Executable::Executable(const std::string& ip, const std::string& src) noexcept: IPaddress(ip), handle(src), valid(true) {}
Server::Executable::Executable(Server::Executable&& src) noexcept: IPaddress(std::move(src.IPaddress)), handle(std::move(src.handle)), valid(src.valid) {
    src.valid = false;
}
Server::Executable& Server::Executable::operator=(Server::Executable&& src) noexcept {
    if(this==&src) return *this;
    this->cleanup();
    this->handle = std::move(src.handle);
    this->IPaddress = std::move(src.IPaddress);
    this->valid = src.valid;
    src.valid = false;
    return *this;
}
Server::Executable::~Executable() noexcept {
    this->cleanup();
}
void Server::Executable::cleanup() noexcept {
    if(!(this->valid)) return;
    this->valid = false;
    RustString(c_remove_binary(this->IPaddress.c_str(), this->handle.c_str()));
}
const char* Server::Executable::c_str() const noexcept {
    return this->handle.c_str();
}

Server::Server() noexcept {
    Server::data& srvdata = this->getData();
    std::lock_guard(srvdata.srvmut);
    srvdata.users++;
}
Server::Server(const std::string& ip) noexcept: IPaddress(ip) {
    Server::data& srvdata = this->getData();
    std::lock_guard(srvdata.srvmut);
    srvdata.users++;
}
Server::Server(const Server& src) noexcept: IPaddress(src.IPaddress) {
    Server::data& srvdata = this->getData();
    std::lock_guard(srvdata.srvmut);
    srvdata.users++;
}

Server& Server::operator=(const Server& src) noexcept {
    if(this==&src) return *this;
    src.getData().users++;
    this->cleanup();
    this->IPaddress = src.IPaddress;
    return *this;
}

void Server::cleanup() noexcept {
    Server::data& serverData = this->getData();
    {
        std::lock_guard lock(serverData.srvmut);
        if(--serverData.users!=0) return;
    }
    // wait for threads to finish
    while(serverData.numThreads!=0) {
        std::lock_guard lock(serverData.srvmut);
        if(serverData.numThreads!=0) continue;
        break;
    }
    serverData.executables.clear();
}

Server::~Server() noexcept {
    this->cleanup();
}

std::pair<const uint8_t*, size_t> Server::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file.is_open()) {
        throw std::runtime_error("could not open file (Server::readFile)");
    }
    std::streamsize size;
    try {
        size = file.tellg();
        file.seekg(0, std::ios::beg);
    } catch(...) {
        throw std::runtime_error("could not calculate file size (Server::readFile)");
    }
    uint8_t* buffer = nullptr;
    try {
        buffer = new uint8_t[size];
    } catch(const std::bad_alloc& except) {
        throw std::runtime_error("could not read file, new threw std::bad_alloc (Server::readFile)");
    }
    try {
        file.read((char*)buffer, size);
        // the file is closed when the destructor is called, and it avoids exceptions
        return std::pair<const uint8_t*, size_t>((const uint8_t*)buffer, size);
    } catch(...) {
        delete[] buffer;
        throw std::runtime_error("error reading file (Server::readFile)");
    }
}

void Server::removeExec(const std::string& filename) const noexcept {
    Server::data& serverData = this->getData();

    std::lock_guard lock(serverData.srvmut);
    const std::unordered_map<std::string, Server::Executable>::iterator iter = serverData.executables.find(filename);
    if(iter==serverData.executables.cend()) return;
    serverData.executables.erase(iter);
}

// note, the buffer is created before the mutex is locked, which may lead to increased memory usage
// a fix would be to put it after the lock, but that would mean the file reading is no longer done in parallel
void Server::sendExec(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::pair<const uint8_t*, size_t> buffer;
    try {
        buffer = Server::readFile(filename);
    } catch(...) {
        throw std::runtime_error("Could not copy the file to an internal buffer, error from Server::readFile (Server::sendExec)");
    }
    {
        std::lock_guard lock(serverData.srvmut);
    
        const std::unordered_map<std::string, Server::Executable>::iterator iter = serverData.executables.find(filename);
        if(iter!=serverData.executables.cend()) {
            serverData.executables.erase(iter);
        }
        // the RustString is created to offload memory management responsibilities
        serverData.executables.emplace(filename, std::move(Executable(this->IPaddress, RustString(c_send_binary(this->IPaddress.c_str(), buffer.first, buffer.second)).cpp_str())));
    }

    delete[] buffer.first;
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str) {
    Server::data& serverData = this->getData();

    std::unordered_map<std::string, Executable>::iterator iter;
    {
        std::lock_guard lock(serverData.srvmut);
        iter = serverData.executables.find(filename);
        if(iter==serverData.executables.cend()) {
            std::pair<const uint8_t*, size_t> buffer;
            try {
                buffer = Server::readFile(filename);
            } catch(...) {
                // the buffer is already deallocated when an exception is thrown
                throw std::runtime_error("Could not copy the file to an internal buffer, error from Server::readFile (Server::runExec)");
            }

            // the RustString is created to offload memory management responsibilities
            serverData.executables.emplace(filename, std::move(Executable(this->IPaddress, RustString(c_send_binary(this->IPaddress.c_str(), buffer.first, buffer.second)).cpp_str())));

            delete[] buffer.first;

            iter = serverData.executables.find(filename);
        }
    }
    const Server::Executable& execHandle = iter->second;

    size_t stdoutLength = 0;
    size_t stdoutCap = 0;
    {
        std::lock_guard lock(serverData.srvmut);
        serverData.numJobs++;
    }
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), execHandle.c_str(), nullptr, 0, (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength, &stdoutCap);
    {
        std::lock_guard lock(serverData.srvmut);
        if(serverData.numJobs>0) serverData.numJobs--;
    }
    const std::string stdout_str((const char*)stdoutVec, stdoutLength);
    free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
    return stdout_str;
}

size_t Server::getNumJobs() const {
    return this->getData().numJobs;
}

Server::data& Server::getData() const noexcept {
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

Client::Client() {}
Client::Client(const std::vector<Server>& servers): machines(servers) {}
Client::Client(std::initializer_list<Server> servers): machines(servers) {}
Client::Client(std::initializer_list<std::string> IPaddresses) {
    for(const std::string& IPaddress : IPaddresses) {
        machines.emplace_back(IPaddress);
    }
}

Client::Client(const Client& src): machines(src.machines) {}

Client& Client::operator=(const Client& src) {
    if(this==&src) return *this;
    this->machines = src.machines;
    return *this;
}

Client::~Client() noexcept {}

size_t Client::numMachines() const noexcept {
    return this->machines.size();
}

Server& Client::getMachine(const size_t index) {
    // machines.at(index) is used instead of machines[index] for safety reasons
    return this->machines.at(index);
}

Server& Client::leastConnections() {
    if(machines.empty()) {
        throw std::out_of_range("Cannot obtain a reference to a server object, client has none stored");
    }
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