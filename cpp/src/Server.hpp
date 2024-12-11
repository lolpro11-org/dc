#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <future>
#include <unordered_map>


class Server {
    private:

    class Executable;
    friend class Server::Executable;
    
    struct data;
    
    std::string IPaddress;

    Server::data& getData() const noexcept;
    bool containsExecutable(const std::string&) const;
    Server::Executable& getExecutable(const std::string&) const;

    public:

    Server();
    Server(const std::string&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server() noexcept;
    std::size_t getNumJobs() const noexcept;
    Server::Executable& sendExec(const std::string&) const;
    Server::Executable& sendExecOverwrite(const std::string&) const;
    void removeExec(const std::string&) const noexcept;
    std::string runExec(const std::string& filename, const std::string& stdin_str = "");
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
    
    bool operator==(const Server&) const noexcept;
    bool operator!=(const Server&) const noexcept;
};


class Server::Executable {
    friend class Server;
    private:
    std::string IPaddress;
    std::string handle;
    bool valid;

    static std::pair<const uint8_t*, std::size_t> readFile(const std::string&);

    public:

    std::string operator()(const std::string) const;
    operator bool() const noexcept;
    
    Executable() noexcept;
    // ip address, filename
    Executable(const std::string&, const std::string&);
    Executable(Executable&&) noexcept;
    Executable& operator=(Executable&&) noexcept;
    ~Executable() noexcept;

    Executable(const Executable&) = delete;
    Executable& operator=(const Executable&) = delete;
};

struct Server::data {
    std::atomic<std::size_t> users;
    std::atomic<std::size_t> numThreads;
    std::mutex mut;
    std::unordered_map<std::string, Server::Executable> executables; // filenames, executable handles
};

#include "Server.tpp"

#endif