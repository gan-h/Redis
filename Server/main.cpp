#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <utility>
#include <iostream>

class Socket
{
public:
    int fd;
    Socket()
    {
        fd = socket(AF_INET, SOCK_STREAM, 0);
    }
    int bindToAddress(uint16_t port, uint32_t address)
    {
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = ntohs(port);
        addr.sin_addr.s_addr = ntohl(address);
        return bind(fd, (const sockaddr *)&addr, sizeof(addr));
    }

    int startListening(int maxConnections)
    {
        return listen(fd, maxConnections);
    }
    ~Socket()
    {
        if (fd > 0)
        {
            close(fd);
        }
    }

    std::pair<sockaddr_in, int> acceptConnection()
    {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int fd = accept(fd, (sockaddr *)&client_addr, &addrlen);
        return std::make_pair(client_addr, fd);
    }
};

void do_something(int fd)
{
    char rbuf[256] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    write(fd, "wee woo wee woo", 16);
    std::cout << "Okay" << std::endl;
    std::cout << rbuf << std::endl;
}

int main()
{
    // man tcp.7: How to create a TCP socket
    Socket s = Socket();
    s.bindToAddress(/* port */ 1234, /*address*/ 0);
    s.startListening(/* max connections */ 128);
    while (true)
    {
        auto newConnection = s.acceptConnection();
        if (newConnection.second < 0)
            continue;
        do_something(newConnection.second);
        close(newConnection.second);
    }
}