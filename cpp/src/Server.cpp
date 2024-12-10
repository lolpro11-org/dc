#include "Server.hpp"

#include <fstream>
#include "../../my_header.h"
#include "RustString.hpp"


Server::Server() {
    Server::data& srvdata = this->getData();
    std::lock_guard lock(srvdata.mut);
    srvdata.users++;
}
Server::Server(const std::string& ip): IPaddress(ip) {
    Server::data& srvdata = this->getData();
    std::lock_guard lock(srvdata.mut);
    srvdata.users++;
}
Server::Server(const Server& src): IPaddress(src.IPaddress) {
    Server::data& srvdata = this->getData();
    std::lock_guard lock(srvdata.mut);
    srvdata.users++;
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
    {
        std::lock_guard lock(serverData.mut);
        if(--serverData.users!=0) return;
    }
    // wait for threads to finish
    while(serverData.numThreads!=0) {
        std::lock_guard lock(serverData.mut);
        if(serverData.numThreads!=0) continue;
        serverData.executables.clear();
        return;
    }
}

Server::~Server() noexcept {
    this->cleanup();
}


void Server::removeExec(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    serverData.executables.erase(filename);
}

void Server::sendExec(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    if(serverData.executables.find(filename)!=serverData.executables.cend()) return;
    serverData.executables.emplace(filename, std::move(Server::Executable(this->IPaddress, filename)));
}
void Server::sendExecOverwrite(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    serverData.executables[filename] = Server::Executable(this->IPaddress, filename);
}
bool Server::containsExecutable(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    return (serverData.executables.find(filename)!=serverData.executables.cend());
}
Server::Executable& Server::getExecutable(const std::string& filename) const {
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    return serverData.executables[filename];
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str) {
    this->sendExec(filename);
    return this->getExecutable(filename)(stdin_str);
}

size_t Server::getNumJobs() const {
    return this->getData().numThreads;
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








Server::Executable::Executable() {
    valid = false;
}
Server::Executable::Executable(const std::string& ip, const std::string& filename) {
    std::pair<const uint8_t*, size_t> fileBuffer;
    try {
        fileBuffer = Server::Executable::readFile(filename);
    } catch(...) {
        // the buffer is already deallocated when an exception is thrown
        throw std::runtime_error("Could not copy the file to an internal buffer, error from Server::Executable::readFile (Server::Executable::Executable)");
    }
    try {
        this->handle = RustString(c_send_binary(ip.c_str(), fileBuffer.first, fileBuffer.second)).cpp_str();
        this->IPaddress = ip;
        this->valid = true;
        delete[] fileBuffer.first;
    } catch(...) {
        delete[] fileBuffer.first;
        throw;
    }
}
Server::Executable::Executable(Server::Executable&& src) {
    this->IPaddress = std::move(src.IPaddress);
    this->handle = std::move(src.handle);
    this->valid = src.valid;
    src.valid = false;
}
Server::Executable& Server::Executable::operator=(Server::Executable&& src) {
    if(this==&src) return *this;
    this->~Executable();
    this->handle = std::move(src.handle);
    this->IPaddress = std::move(src.IPaddress);
    this->valid = src.valid;
    src.valid = false;
    return *this;
}
Server::Executable::~Executable() noexcept {
    if(!(this->valid)) return;
    this->valid = false;
    RustString(c_remove_binary(this->IPaddress.c_str(), this->handle.c_str()));
}
std::string Server::Executable::operator()(const std::string stdin_str) const {
    if(!(this->valid)) return "";
    size_t stdoutLength = 0;
    size_t stdoutCap = 0;
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), this->handle.c_str(), nullptr, 0, (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength, &stdoutCap);
    try {
        const std::string stdout_str((const char*)stdoutVec, stdoutLength);
        free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
        return stdout_str;
    } catch(...) {
        free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
        throw;
    }
}
Server::Executable::operator bool() const {
    return this->valid;
}

std::pair<const uint8_t*, size_t> Server::Executable::readFile(const std::string& filename) {
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