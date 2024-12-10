#include <exception>
#include <stdexcept>

#include "Client.hpp"

Client::Client() {}
Client::Client(const std::vector<Server>& servers): machines(servers) {}
Client::Client(std::initializer_list<Server> servers): machines(servers) {}
Client::Client(std::initializer_list<std::string> IPaddresses) {
    for(const std::string& IPaddress : IPaddresses) {
        machines.emplace_back(IPaddress);
    }
}

Client::Client(const Client& src): machines(src.machines) {}

Client& Client::operator=(const Client& src) {
    if(this==&src) return *this;
    this->machines = src.machines;
    return *this;
}

size_t Client::numMachines() const noexcept {
    return this->machines.size();
}

Server& Client::getMachine(const size_t index) {
    // machines.at(index) is used instead of machines[index] for safety reasons
    return this->machines.at(index);
}

Server& Client::leastConnections() {
    if(machines.empty()) {
        throw std::out_of_range("Cannot obtain a reference to a server object, client has none stored");
    }
    size_t minJobs = machines[0].getNumJobs();
    size_t minIter = 0;
    for(size_t i=1; i<machines.size(); i++) {
        const size_t numJobs = machines[i].getNumJobs();
        if(minJobs <= numJobs) continue;
        minJobs = numJobs;
        minIter = i;
    }
    return machines[minIter];
}