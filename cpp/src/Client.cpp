#include <exception>
#include <stdexcept>

#include "Client.hpp"

// may throw std::bad_alloc
Client::Client(): machines({Server()}) {}
// may throw std::bad_alloc, std::length_error
Client::Client(const std::vector<Server>& servers): machines(servers) {
    if(machines.empty()) machines = {Server()};
}
// may throw std::bad_alloc, std::length_error
Client::Client(std::initializer_list<Server> servers): machines(servers) {
    if(machines.empty()) machines = {Server()};
}
// may throw std::bad_alloc, std::length_error
Client::Client(std::initializer_list<std::string> IPaddresses) {
    for(const std::string& IPaddress : IPaddresses) {
        machines.emplace_back(IPaddress);
    }
    if(machines.empty()) machines = {Server()};
}

std::size_t Client::numMachines() const noexcept {
    return this->machines.size();
}

// may throw std::out_of_range
Server& Client::getMachine(const std::size_t index) {
    // machines.at(index) is used instead of machines[index] for safety reasons
    return this->machines.at(index);
}

// may throw std::out_of_range
Server& Client::leastConnections() noexcept {
    // this case should never happen as the default constructor creates at least one server object
    // will crash the program, as something has probably gone very wrong (crash happens because noexcept)
    if(this->numMachines()==0) throw std::out_of_range("Cannot obtain a reference to a server object, client has none stored (Client::leastConnections)");
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

// may throw: std::bad_alloc, std::out_of_range, and std::system_error
// calling get on the future may throw: std::bad_alloc, std::runtime_error
std::future<std::string> Client::distributeAndRun(const std::string& filename, const std::string& stdin_str) {
    return leastConnections().runExecAsync(filename, stdin_str);
}