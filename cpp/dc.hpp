#ifndef DC_HPP
#define DC_HPP

#include <vector>
#include <string>
#include <future>

class Server {
    private:

    class Executable;
    struct data;

    std::string IPaddress;

    static std::pair<const uint8_t*, size_t> readFile(const std::string&);
    Server::data& getData() const noexcept;

    void cleanup() noexcept;

    public:
    Server() noexcept;
    Server(const std::string&) noexcept;
    Server(const Server&) noexcept;
    Server& operator=(const Server&) noexcept;
    ~Server() noexcept;
    void sendExec(const std::string&) const;
    void removeExec(const std::string&) const noexcept;
    std::string runExec(const std::string& filename, const std::string& stdin_str = "");
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
    size_t getNumJobs() const;
};

class Client {
    private:
    std::vector<Server> machines;

    Server& leastConnections();

    public:
    Client();
    Client(const std::vector<Server>&);
    Client(std::initializer_list<Server>);
    Client(std::initializer_list<std::string>);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client() noexcept;

    size_t numMachines() const noexcept;
    Server& getMachine(const size_t);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    template<typename ReturnType, typename... Args> std::future<ReturnType> distributeAndRun(const std::string&, const Args&...);
};
#include "dc.tpp"

#endif