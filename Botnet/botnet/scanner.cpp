#include "scanner.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <iostream>
#include <cstring>

bool isPortOpen(const std::string &ip, int port, int timeout_ms)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return false;

    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    // Try to connect (will return immediately)
    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    // Wait for connection with timeout
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms * 1000;

    bool isOpen = false;
    if (select(sock + 1, NULL, &writefds, NULL, &timeout) > 0)
    {
        int error;
        socklen_t len = sizeof(error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
        isOpen = (error == 0);
    }

    close(sock);
    return isOpen;
}

std::vector<int> scanForServers(const std::string &ip, int startPort,
                                int endPort, int myPort)
{
    std::vector<int> openPorts;

    std::cout << "[SCAN] Scanning ports " << startPort << "-" << endPort << std::endl;

    for (int port = startPort; port <= endPort; port++)
    {
        if (port == myPort)
        {
            continue; // Skip our own port
        }

        if (isPortOpen(ip, port, 200))
        {
            std::cout << "[SCAN] Found server on port " << port << std::endl;
            openPorts.push_back(port);
        }

        // Progress indicator
        if ((port - startPort) % 20 == 0)
        {
            std::cout << "[SCAN] Progress: " << port << "/" << endPort << std::endl;
        }
    }

    std::cout << "[SCAN] Complete. Found " << openPorts.size() << " servers" << std::endl;
    return openPorts;
}

std::vector<int> selectNeighbors(const std::vector<int> &allPorts,
                                 int myPort, int maxConnections)
{
    std::vector<int> selected;

    // Cast to avoid comparison warnings
    if (allPorts.size() <= static_cast<size_t>(maxConnections))
    {
        // Connect to all if we have maxConnections or fewer
        return allPorts;
    }

    // Separate into ports below and above us
    std::vector<int> portsBelow;
    std::vector<int> portsAbove;

    for (int port : allPorts)
    {
        if (port < myPort)
        {
            portsBelow.push_back(port);
        }
        else if (port > myPort)
        {
            portsAbove.push_back(port);
        }
    }

    // Sort: closest first
    std::sort(portsBelow.begin(), portsBelow.end(), std::greater<int>()); // Descending
    std::sort(portsAbove.begin(), portsAbove.end());                      // Ascending

    // Take half from each side
    int halfMax = maxConnections / 2;

    // Add closest below
    for (size_t i = 0; i < portsBelow.size() && i < static_cast<size_t>(halfMax); i++)
    {
        selected.push_back(portsBelow[i]);
    }

    // Add closest above
    for (size_t i = 0; i < portsAbove.size() && i < static_cast<size_t>(halfMax); i++)
    {
        selected.push_back(portsAbove[i]);
    }

    // Fill remaining slots if one side had fewer than halfMax
    if (selected.size() < static_cast<size_t>(maxConnections))
    {
        int remaining = maxConnections - selected.size();

        // Add more from below
        for (size_t i = halfMax; i < portsBelow.size() && remaining > 0; i++)
        {
            selected.push_back(portsBelow[i]);
            remaining--;
        }

        // Add more from above
        for (size_t i = halfMax; i < portsAbove.size() && remaining > 0; i++)
        {
            selected.push_back(portsAbove[i]);
            remaining--;
        }
    }

    return selected;
}