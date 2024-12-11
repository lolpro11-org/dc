#include <exception>
#include <stdexcept>

#include "Client.hpp"

Client::Client(): machines({Server()}) {}
Client::Client(const std::vector<Server>& servers): machines(servers) {
    if(machines.empty()) machines = {Server()};
}
Client::Client(std::initializer_list<Server> servers): machines(servers) {
    if(machines.empty()) machines = {Server()};
}
Client::Client(std::initializer_list<std::string> IPaddresses) {
    for(const std::string& IPaddress : IPaddresses) {
        machines.emplace_back(IPaddress);
    }
    if(machines.empty()) machines = {Server()};
}

std::size_t Client::numMachines() const noexcept {
    return this->machines.size();
}

// may throw std::out_of_range because of std::vector::at
Server& Client::getMachine(const std::size_t index) {
    // machines.at(index) is used instead of machines[index] for safety reasons
    return this->machines.at(index);
}

Server& Client::leastConnections() noexcept {
    // this case should never happen as the default constructor creates at least one server object
    // will crash the program, as something has probably gone very wrong (crash happens because noexcept)
    if(this->numMachines()==0) throw std::out_of_range("Cannot obtain a reference to a server object, client has none stored");
    else if(this->numMachines()==1) return machines[0];
    
    std::size_t minJobs = machines[0].getNumJobs();
    std::size_t minIter = 0;
    for(std::size_t i=1; i<this->numMachines(); i++) {
        const std::size_t numJobs = machines[i].getNumJobs();
        if(minJobs <= numJobs) continue;
        minJobs = numJobs;
        minIter = i;
    }
    return machines[minIter];
}