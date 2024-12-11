#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <vector>
#include <string>
#include <future>
#include "Server.hpp"

class Client {
    private:
    std::vector<Server> machines;

    Server& leastConnections() noexcept;

    public:
    Client();
    Client(const Client&) = default;
    Client& operator=(const Client&) = default;
    Client(const std::vector<Server>&);
    Client(std::initializer_list<Server>);
    Client(std::initializer_list<std::string>);

    std::size_t numMachines() const noexcept;
    Server& getMachine(const std::size_t);
    // keep arguments valid until the function returns, arguments are passed by reference not by value
    std::future<std::string> distributeAndRun(const std::string&, const std::string&);
};

#endif