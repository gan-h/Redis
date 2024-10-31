#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

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
        addr.sin_addr.s_addr = ntohl(address); // wildcard address 0.0.0.0
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
};

int main()
{
    // man tcp.7: How to create a TCP socket
    Socket s = Socket();
    s.bindToAddress(1234, 0);
    s.startListening(128);
    while (true) {
        struct sockaddr_in client_addr = {};
    }
}