#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <future>
#include <unordered_map>


class Server {
    private:

    class Executable;
    friend class Server::Executable;
    
    class data;
    
    std::shared_ptr<Server::data> dataptr;

    Server::data& getData() const noexcept;
    bool containsExecutable(const std::string&) const;
    Server::Executable& getExecutable(const std::string&) const;

    public:

    Server() = default;
    Server(const std::string&);
    Server(const Server&);
    Server& operator=(const Server&);
    ~Server() noexcept = default;
    std::size_t getNumJobs() const noexcept;
    void sendExec(const std::string&) const;
    void sendExecOverwrite(const std::string&) const;
    void removeExec(const std::string&) const noexcept;
    std::string runExec(const std::string& filename, const std::string& stdin_str = "");
    std::future<std::string> runExecAsync(const std::string&, const std::string&);
    
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

class Server::data {
    public:

    data(const std::string&);
    ~data() noexcept = default;

    data(const data&) = delete;
    data(data&&) = delete;
    data& operator=(const data&) = delete;
    data& operator=(data&&) = delete;

    const std::string IPaddress;
    std::atomic<std::size_t> users;
    std::atomic<std::size_t> numThreads;
    std::mutex mut;
    std::unordered_map<std::string, Server::Executable> executables; // filenames, executable handles
};

#endif