#include "Server.hpp"

#include <fstream>
#include "../../my_header.h"
#include "RustString.hpp"


Server::Server(): IPaddress() {}
Server::Server(const std::string& ip): IPaddress(ip), dataptr(std::make_shared<Server::data>()) {
    if(this->IPaddress.empty()) return;
    this->getData().users++;
}
Server::Server(const Server& src): IPaddress(src.IPaddress) {
    if(this->IPaddress.empty()) return;
    this->dataptr = src.dataptr;
    this->getData().users++;
}

Server& Server::operator=(const Server& src) {
    if(this==&src) return *this;
    if(*this == src) return *this; // optimization to prevent locking the mutex
    if(src.IPaddress.empty()) {
        this->~Server();
        this->IPaddress.clear();
        this->dataptr = nullptr;
        return *this;
    }
    src.dataptr->users++;
    this->~Server();
    this->IPaddress = src.IPaddress;
    this->dataptr = src.dataptr;
    return *this;
}

Server::~Server() noexcept {
    if(this->IPaddress.empty()) return;
    Server::data& serverData = this->getData();
    if(--serverData.users!=0) return;
    {
        std::lock_guard lock(serverData.mut);
        serverData.executables.clear();
    }
}

bool Server::operator==(const Server& rhs) const noexcept {
    return (this == &rhs)?true:(this->IPaddress==rhs.IPaddress);
}

bool Server::operator!=(const Server& rhs) const noexcept {
    return !(*this == rhs);
}


void Server::removeExec(const std::string& filename) const noexcept {
    if(this->IPaddress.empty()) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    serverData.executables.erase(filename);
}
void Server::sendExec(const std::string& filename) const {
    if(this->IPaddress.empty()) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    if(serverData.executables.find(filename)!=serverData.executables.cend()) return;
    serverData.executables.emplace(filename, std::move(Server::Executable(this->IPaddress, filename)));
}
void Server::sendExecOverwrite(const std::string& filename) const {
    if(this->IPaddress.empty()) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    serverData.executables[filename] = Server::Executable(this->IPaddress, filename);
}
bool Server::containsExecutable(const std::string& filename) const {
    if(this->IPaddress.empty()) return false;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    return (serverData.executables.find(filename)!=serverData.executables.cend());
}
Server::Executable& Server::getExecutable(const std::string& filename) const {
    if(this->IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::getExecutable)");
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    return serverData.executables[filename];
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str) {
    if(this->IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::runExec)");
    this->sendExec(filename);
    const std::string stdout_str = this->getExecutable(filename)(stdin_str);
    this->getData().numThreads--;
    return stdout_str;
}

std::size_t Server::getNumJobs() const noexcept {
    return this->IPaddress.empty() ? 0 : this->getData().numThreads.load();
}

Server::data& Server::getData() const noexcept {
    return *(this->dataptr);
}

std::future<std::string> Server::runExecAsync(const std::string& filename, const std::string& stdin_str) {
    if(this->IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::runExecAsync)");
    this->getData().numThreads++; // gets decremented when runExecAsFunction finishes
    return std::async(std::launch::async, &Server::runExec, *this, filename, stdin_str);
}








Server::Executable::Executable() noexcept: valid(false) {}
Server::Executable::Executable(const std::string& ip, const std::string& filename) {
    // readFile may throw std::runtime_error, and only std::runtime_error
    const std::pair<const uint8_t*, std::size_t> fileBuffer = Server::Executable::readFile(filename);
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
Server::Executable::Executable(Server::Executable&& src) noexcept {
    this->IPaddress = std::move(src.IPaddress);
    this->handle = std::move(src.handle);
    this->valid = src.valid;
    src.valid = false;
}
Server::Executable& Server::Executable::operator=(Server::Executable&& src) noexcept {
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
    std::size_t stdoutLength = 0;
    std::size_t stdoutCap = 0;
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
Server::Executable::operator bool() const noexcept {
    return this->valid;
}

std::pair<const uint8_t*, std::size_t> Server::Executable::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file.is_open()) {
        throw std::runtime_error("could not open file (Server::readFile)");
    }
    std::size_t size;
    try {
        std::streampos endpos = file.tellg();
        if(endpos<0) throw std::runtime_error("error calculating file size (Server::readFile)");
        size = (std::size_t)endpos;
        file.seekg(0, std::ios::beg);
    } catch(...) {
        throw std::runtime_error("could not calculate file size (Server::readFile)");
    }
    uint8_t* buffer = nullptr;
    try {
        buffer = new uint8_t[size];
    } catch(const std::bad_alloc& except) {
        throw std::runtime_error("could not read file, new threw std::bad_alloc (Server::readFile)");
    } catch(...) {
        throw std::runtime_error("new threw an unknown exception (Server::readFile)");
    }
    try {
        file.read((char*)buffer, size);
        // the file is closed when the destructor is called, and it avoids exceptions
        return std::pair<const uint8_t*, std::size_t>((const uint8_t*)buffer, size);
    } catch(...) {
        delete[] buffer;
        throw std::runtime_error("error reading file (Server::readFile)");
    }
}