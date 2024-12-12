#include "Server.hpp"

#include <fstream>
#include "../../my_header.h"
#include "RustString.hpp"

Server::Server(const std::string& ip): dataptr(std::make_shared<Server::data>(ip)) {}
Server::Server(const Server& src): dataptr(src.dataptr) {}

Server& Server::operator=(const Server& src) {
    if(this==&src) return *this;
    this->dataptr = src.dataptr;
    return *this;
}

bool Server::operator==(const Server& rhs) const noexcept {
    return (this->dataptr == rhs.dataptr);
}

bool Server::operator!=(const Server& rhs) const noexcept {
    return !(*this == rhs);
}


void Server::removeExec(const std::string& filename) const noexcept {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) return;
    std::lock_guard lock(serverData.mut);
    serverData.executables.erase(filename);
}
void Server::sendExec(const std::string& filename) const {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) return;
    std::lock_guard lock(serverData.mut);
    if(serverData.executables.find(filename)!=serverData.executables.cend()) return;
    serverData.executables.emplace(filename, std::move(Executable(serverData.IPaddress, filename)));
}
void Server::sendExecOverwrite(const std::string& filename) const {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) return;
    std::lock_guard lock(serverData.mut);
    serverData.executables[filename] = Executable(serverData.IPaddress, filename);
}
bool Server::containsExecutable(const std::string& filename) const {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) return false;
    std::lock_guard lock(serverData.mut);
    return (serverData.executables.find(filename)!=serverData.executables.cend());
}
Executable& Server::getExecutable(const std::string& filename) const {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::getExecutable)");
    std::lock_guard lock(serverData.mut);
    return serverData.executables[filename];
}

std::string Server::runExec(const std::string& filename, const std::string& stdin_str) {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::runExec)");
    this->sendExec(filename);
    const std::string stdout_str = this->getExecutable(filename)(stdin_str);
    serverData.numThreads--;
    return stdout_str;
}

std::size_t Server::getNumJobs() const noexcept {
    Server::data& serverData = this->getData();
    return serverData.IPaddress.empty() ? 0 : serverData.numThreads.load();
}

Server::data& Server::getData() const noexcept {
    return *(this->dataptr);
}

std::future<std::string> Server::runExecAsync(const std::string& filename, const std::string& stdin_str) {
    Server::data& serverData = this->getData();
    if(serverData.IPaddress.empty()) throw std::invalid_argument("method called on an invalid Server object (Server::runExecAsync)");
    serverData.numThreads++; // gets decremented when runExecAsFunction finishes
    return std::async(std::launch::async, &Server::runExec, *this, filename, stdin_str);
}








Executable::Executable() noexcept: valid(false) {}
Executable::Executable(const std::string& ip, const std::string& filename) {
    // readFile may throw std::runtime_error, and only std::runtime_error
    const std::pair<const uint8_t*, std::size_t> fileBuffer = Executable::readFile(filename);
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
Executable::Executable(Executable&& src) noexcept {
    this->IPaddress = std::move(src.IPaddress);
    this->handle = std::move(src.handle);
    this->valid = src.valid;
    src.valid = false;
}
Executable& Executable::operator=(Executable&& src) noexcept {
    if(this==&src) return *this;
    this->cleanup();
    this->handle = std::move(src.handle);
    this->IPaddress = std::move(src.IPaddress);
    this->valid = src.valid;
    src.valid = false;
    return *this;
}
void Executable::cleanup() noexcept {
    if(!(this->valid)) return;
    this->valid = false;
    RustString(c_remove_binary(this->IPaddress.c_str(), this->handle.c_str()));
}
Executable::~Executable() noexcept {
    this->cleanup();
}
std::string Executable::operator()(const std::string stdin_str) const {
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
Executable::operator bool() const noexcept {
    return this->valid;
}

std::pair<const uint8_t*, std::size_t> Executable::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file.is_open()) {
        throw std::runtime_error("could not open file (Executable::readFile)");
    }
    std::size_t size;
    try {
        std::streampos endpos = file.tellg();
        if(endpos<0) throw std::runtime_error("error calculating file size (Executable::readFile)");
        size = (std::size_t)endpos;
        file.seekg(0, std::ios::beg);
    } catch(...) {
        throw std::runtime_error("could not calculate file size (Executable::readFile)");
    }
    uint8_t* buffer = nullptr;
    try {
        buffer = new uint8_t[size];
    } catch(const std::bad_alloc& except) {
        throw std::runtime_error("could not read file, new threw std::bad_alloc (Executable::readFile)");
    } catch(...) {
        throw std::runtime_error("new threw an unknown exception (Executable::readFile)");
    }
    try {
        file.read((char*)buffer, size);
        // the file is closed when the destructor is called, and it avoids exceptions
        return std::pair<const uint8_t*, std::size_t>((const uint8_t*)buffer, size);
    } catch(...) {
        delete[] buffer;
        throw std::runtime_error("error reading file (Executable::readFile)");
    }
}







Server::data::data(const std::string& ip): IPaddress(ip) {}

