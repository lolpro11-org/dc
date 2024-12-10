#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <string>
#include <future>
#include "Server.hpp"

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

    size_t numMachines() const noexcept;
    Server& getMachine(const size_t);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    template<typename ReturnType, typename... Args> std::future<ReturnType> distributeAndRun(const std::string&, const Args&...);
};
#include "Client.tpp"

#endif