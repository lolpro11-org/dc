#include "Server.hpp"

#include <fstream>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "../../my_header.h"
#include "RustString.hpp"

#include "ScopeCounter.tpp"

// may throw std::bad_alloc
Server::Server(const std::string& ip): dataptr(std::make_shared<Server::data>(ip)) {}
// copy construction of std::shared_ptr will never throw
Server::Server(const Server& src) noexcept: dataptr(src.dataptr) {}

Server& Server::operator=(const Server& src) noexcept {
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

Server::operator bool() const noexcept {
    return (this->dataptr != nullptr);
}

void Server::removeExec(const std::string& filename) const noexcept {
    if(this->dataptr == nullptr) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    serverData.executables.erase(filename);
}
// may throw std::bad_alloc and std::runtime_error
void Server::sendExec(const std::string& filename) const {
    if(this->dataptr == nullptr) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    try {
        if(serverData.executables.find(filename)!=serverData.executables.cend()) return;
        serverData.executables.emplace(filename, std::move(Executable(serverData.IPaddress, filename)));
    } catch(const std::bad_alloc& except) {
        throw;
    } catch(const std::runtime_error& except) {
        throw;
    } catch(...) {
        throw std::runtime_error("unknown exception thrown by std::unordered_map::find or std::unordered_map::emplace (Server::sendExec)");
    }
}
// may throw std::bad_alloc and std::runtime_error
void Server::sendExecOverwrite(const std::string& filename) const {
    if(this->dataptr == nullptr) return;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    try {
        serverData.executables.insert_or_assign(filename, std::move(Executable(serverData.IPaddress, filename)));
    } catch(const std::bad_alloc& except) {
        throw;
    } catch(const std::runtime_error& except) {
        throw;
    } catch(...) {
        throw std::runtime_error("unknown exception thrown by std::unordered_map::insert_or_assign (Server::sendExecOverwrite)");
    }
}
// may throw std::runtime_error
bool Server::containsExecutable(const std::string& filename) const {
    if(this->dataptr == nullptr) return false;
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    // const reference is to force it to use the const qualified version of std::unordered_map::find (might improve performance?)
    const std::unordered_map<std::string, Executable>& execs = serverData.executables;
    try {
        return (execs.find(filename)!=execs.cend());
    } catch(...) {
        throw std::runtime_error("unknown exception thrown by std::unordered_map::find (Server::containsExecutable)");
    }
}
// may throw std::runtime_error
Executable& Server::getExecutable(const std::string& filename) const {
    if(this->dataptr == nullptr) throw std::runtime_error("method called on an invalid Server object (Server::getExecutable)");
    Server::data& serverData = this->getData();
    std::lock_guard lock(serverData.mut);
    std::unordered_map<std::string, Executable>::iterator iter;
    try {
        iter = serverData.executables.find(filename);
    } catch(...) {
        throw std::runtime_error("unknown exception thrown by std::unordered_map::find (Server::getExecutable)");
    }
    if(iter==serverData.executables.cend()) throw std::runtime_error("executable not found in Server (Server::getExecutable)");
    return iter->second;
}

// could throw std::bad_alloc, std::runtime_error
std::string Server::runExec(const std::string& filename, const std::string& stdin_str) {
    if(this->dataptr == nullptr) throw std::runtime_error("method called on an invalid Server object (Server::runExec)");
    ScopeCounter count(this->getData().numThreads); // useful for try-catch blocks
    this->sendExec(filename);
    return this->getExecutable(filename)(stdin_str);
}

std::size_t Server::getNumJobs() const noexcept {
    return (this->dataptr == nullptr) ? 0 : this->getData().numThreads.load();
}

Server::data& Server::getData() const noexcept {
    return *(this->dataptr);
}

// may throw: std::bad_alloc, and std::system_error
// calling get on the future may throw: std::bad_alloc, std::runtime_error
std::future<std::string> Server::runExecAsync(const std::string& filename, const std::string& stdin_str) {
    if(this->dataptr == nullptr) throw std::runtime_error("method called on an invalid Server object (Server::runExecAsync)");
    else return std::async(std::launch::async, &Server::runExec, *this, filename, stdin_str);
}







// may throw std::bad_alloc
Executable::Executable() noexcept: valid(false), IPaddress(), handle() {}
// may throw std::bad_alloc and std::runtime_error (note that std::bad_alloc can be thrown by the string copy constructors)
Executable::Executable(const std::string& ip, const std::string& filename): valid(false), IPaddress(ip), handle() {
    // Open the file
    const int fd = open(filename.c_str(), O_RDONLY);
    if(fd == -1) {
        throw std::runtime_error("failed to open file (Executable::Executable)");
    }

    struct stat sb;
    // Get the file size
    if(fstat(fd, &sb) == -1) {
        close(fd);
        throw std::runtime_error("failed to read file size (Executable::Executable)");
    }

    // Map the file into memory
    void*const addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(addr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("failed to map file to memory (Executable::Executable)");
    }

    // access contents
    // RustString construction won't throw
    this->handle = std::move(RustString(c_send_binary(ip.c_str(), (const uint8_t*)addr, sb.st_size)));
    this->valid = true;
    
    // Unmap the file
    if(munmap(addr, sb.st_size) == -1) {
        close(fd);
        this->~Executable(); // its in a valid state to be destructed as needed (throw won't destruct unless the constructor finishes)
        throw std::runtime_error("failed to unmap file from memory (Executable::Executable)");
    }

    // Close the file
    close(fd);
}
// move construction of string members should never throw
Executable::Executable(Executable&& src) noexcept: valid(src.valid), IPaddress(std::move(src.IPaddress)), handle(std::move(src.handle)) {
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
// may throw std::bad_alloc, std::runtime_error
std::string Executable::operator()(const std::string stdin_str) const {
    if(!(this->valid)) return std::string();
    std::size_t stdoutLength = 0;
    std::size_t stdoutCap = 0;
    const uint8_t* stdoutVec = c_execute_binary(this->IPaddress.c_str(), this->handle.c_str(), nullptr, 0, (const uint8_t*)(stdin_str.c_str()), stdin_str.length(), &stdoutLength, &stdoutCap);
    try {
        const std::string stdout_str((const char*)stdoutVec, stdoutLength);
        free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
        return stdout_str;
    } catch(const std::length_error& except) {
        free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
        throw std::runtime_error("stdout string too large to contain within std::string (Executable::operator())");
    } catch(...) {
        free_vec((void*)stdoutVec, stdoutLength, stdoutCap);
        throw;
    }
}

Executable::operator bool() const noexcept {
    return this->valid;
}






// may throw bad_alloc
Server::data::data(const std::string& ip):
    IPaddress(ip),
    numThreads(0),
    mut(),
    executables()
{}