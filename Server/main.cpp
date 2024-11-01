#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <utility>
#include <iostream>
#include <sys/epoll.h> // epoll functions
#include <unistd.h>    // for close
#include <fcntl.h>     // fcntl to set non-blocking
#include <vector>
#include <cstring>
#include <cstdlib>     // for exit

#define CHECK(expr)             \
    ({                          \
        auto result = (expr);   \
        if (result == -1)       \
        {                       \
            perror(#expr);      \
            exit(EXIT_FAILURE); \
        }                       \
        result;                 \
    })

class Socket
{
public:
    int fd;
    Socket()
    {
        fd = CHECK(socket(AF_INET, SOCK_STREAM, 0));
    }
    int bindToAddress(uint16_t port, uint32_t address)
    {
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port); // Use htons for network byte order
        addr.sin_addr.s_addr = htonl(address); // Use htonl for network byte order
        return CHECK(bind(fd, (const sockaddr *)&addr, sizeof(addr)));
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
        int client_fd = accept(fd, (sockaddr *)&client_addr, &addrlen);
        if (client_fd == -1)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("accept");
            }
            return std::make_pair(client_addr, -1);
        }
        return std::make_pair(client_addr, client_fd);
    }
};

// Utility function to set a file descriptor to non-blocking mode
int setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

int main()
{
    // Create and set up the listening socket
    Socket s;
    s.bindToAddress(/* port */ 1234, /*address*/ 0); // 0.0.0.0
    s.startListening(/* max connections */ 128);

    // Set the listening socket to non-blocking mode
    if (setNonBlocking(s.fd) == -1)
    {
        std::cerr << "Failed to set listening socket to non-blocking.\n";
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    int epoll_fd = CHECK(epoll_create1(0));

    // Define the event for the listening socket
    struct epoll_event event;
    event.events = EPOLLIN; // Monitor read (incoming connections)
    event.data.fd = s.fd;

    // Add the listening socket to epoll
    CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, s.fd, &event));

    // Buffer for events
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];

    std::cout << "Server is listening on port 1234...\n";

    while (true)
    {
        // Wait for events
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1)
        {
            if (errno == EINTR)
                continue; // Interrupted by signal, retry
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == s.fd)
            {
                // Incoming connection
                while (true)
                {
                    auto [client_addr, client_fd] = s.acceptConnection();
                    if (client_fd == -1)
                        break; // No more connections to accept

                    std::cout << "Accepted connection from "
                              << inet_ntoa(client_addr.sin_addr)
                              << ":" << ntohs(client_addr.sin_port) << "\n";

                    // Set the client socket to non-blocking
                    if (setNonBlocking(client_fd) == -1)
                    {
                        std::cerr << "Failed to set client socket to non-blocking.\n";
                        close(client_fd);
                        continue;
                    }

                    // Add the client socket to epoll
                    struct epoll_event client_event;
                    client_event.events = EPOLLIN | EPOLLET; // Edge-triggered
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1)
                    {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                        continue;
                    }
                }
            }
            else
            {
                // Data from a client
                int client_fd = events[i].data.fd;
                bool done = false;

                while (true)
                {
                    char buffer[512];
                    ssize_t count = read(client_fd, buffer, sizeof(buffer));
                    if (count == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // All data read
                            break;
                        }
                        else
                        {
                            // Read error
                            perror("read");
                            done = true;
                            break;
                        }
                    }
                    else if (count == 0)
                    {
                        // Client closed connection
                        done = true;
                        break;
                    }

                    // Echo the data back to the client (for demonstration)
                    ssize_t written = 0;
                    while (written < count)
                    {
                        ssize_t w = write(client_fd, buffer + written, count - written);
                        if (w == -1)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                // Cannot write now, try later
                                break;
                            }
                            else
                            {
                                perror("write");
                                done = true;
                                break;
                            }
                        }
                        written += w;
                    }

                    if (done)
                        break;
                }

                if (done)
                {
                    std::cout << "Closed connection on descriptor " << client_fd << "\n";
                    CHECK(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL));
                    close(client_fd);
                }
            }
        }
    }

    // Clean up
    close(epoll_fd);
    return 0;
}
