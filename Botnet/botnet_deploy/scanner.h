#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include <vector>

/**
 * Check if a port is open on given IP address
 * @param ip IP address to check
 * @param port Port number to check
 * @param timeout_ms Timeout in milliseconds
 * @return true if port is open, false otherwise
 */
bool isPortOpen(const std::string &ip, int port, int timeout_ms = 500);

/**
 * Scan a range of ports for open servers
 * @param ip IP address to scan
 * @param startPort Starting port number
 * @param endPort Ending port number
 * @param myPort Our own port (to skip)
 * @return Vector of open port numbers
 */
std::vector<int> scanForServers(const std::string &ip, int startPort,
                                int endPort, int myPort);

/**
 * Select closest neighbors based on port proximity
 * @param allPorts All discovered open ports
 * @param myPort Our listening port
 * @param maxConnections Maximum connections to maintain (default 8)
 * @return Vector of selected port numbers
 */
std::vector<int> selectNeighbors(const std::vector<int> &allPorts,
                                 int myPort, int maxConnections = 8);

#endif // SCANNER_H