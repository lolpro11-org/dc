#ifndef DC_HPP
#define DC_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <future>
#include <mutex>

class Server {
    private:

    class Executable;
    struct data;

    std::string IPaddress;

    static std::pair<uint8_t*, size_t> readFile(const std::string&);
    Server::data& getData() const;

    void cleanup();

    public:
    Server();
    Server(const std::string&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server();
    void sendExec(const std::string&);
    void removeExec(const std::string&);
    std::string runExec(const std::string& filename, const std::string& stdin_str = "", const std::vector<std::string>& args = {});
    template<typename ReturnType, typename... Args> ReturnType runExecAsFunction(const std::string&, const Args&...);
    template<typename ReturnType, typename... Args> std::future<ReturnType> runExecAsAsyncFunction(const std::string&, const Args&...);
    size_t getNumJobs() const;
};

class Client {
    private:
    std::vector<Server> machines;

    Server& leastConnections();

    public:
    Client(const std::vector<Server>&);
    Client(std::initializer_list<Server>);
    Client(const Client&);
    Client& operator=(const Client&);
    ~Client();

    size_t numMachines() const;
    Server& getMachine(const size_t);
    template<typename ReturnType, typename... Args> std::future<ReturnType> roundRobinAsync(const std::string&, const Args&...);
};
#include "dc.tpp"

#endif