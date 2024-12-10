#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <future>
#include <unordered_map>


class Server {
    private:

    class Executable;
    struct data;
    
    std::string IPaddress;

    Server::data& getData() const;
    bool containsExecutable(const std::string&) const;
    Server::Executable& getExecutable(const std::string&) const;

    public:
    Server();
    Server(const std::string&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server() noexcept;
    Server::Executable& sendExec(const std::string&) const;
    Server::Executable& sendExecOverwrite(const std::string&) const;
    void removeExec(const std::string&) const;
    std::string runExec(const std::string& filename, const std::string& stdin_str = "");
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
    size_t getNumJobs() const;
};


class Server::Executable {
    friend class Server;
    private:
    std::string IPaddress;
    std::string handle;
    bool valid;

    static std::pair<const uint8_t*, size_t> readFile(const std::string&);

    public:

    std::string operator()(const std::string) const;
    operator bool() const noexcept;
    
    Executable() noexcept;
    // ip address, filename
    Executable(const std::string&, const std::string&);
    Executable(Executable&&);
    Executable& operator=(Executable&&);
    ~Executable() noexcept;

    Executable(const Executable&) = delete;
    Executable& operator=(const Executable&) = delete;
};

struct Server::data {
    size_t users;
    size_t numThreads;
    std::mutex mut;
    std::unordered_map<std::string, Server::Executable> executables; // filenames, executable handles
};

#endif