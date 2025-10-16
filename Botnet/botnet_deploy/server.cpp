#include "protocol.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <map>
#include <queue>
#include <vector>
#include <set>
#include <pthread.h>

const std::string MY_GROUP_ID = "A5_1";
const std::string TSAM_SERVER_IP = "130.208.246.98";
const std::vector<int> INSTRUCTOR_PORTS = {5001, 5002, 5003};

// Structure to hold information about connected servers
struct ServerInfo
{
    int socket;
    std::string groupId;
    std::string ip;
    int port;
    time_t lastSeen;
    bool isOutgoing; // true if we initiated connection
};

// Structure for servers we've heard about but aren't connected to
struct KnownServer
{
    std::string groupId;
    std::string ip;
    int port;
    time_t lastHeard; // When we last heard about this server
};

// Global data structures
std::map<int, ServerInfo> connectedServers; // socket -> ServerInfo
std::vector<KnownServer> knownServers;      // Servers we've learned about
std::set<std::string> connectedGroupIds;    // Quick lookup of connected groups
std::map<std::string, std::queue<std::string>> messageQueue;
std::ofstream logFile;
int listenPort;
std::string myIpAddress;
pthread_mutex_t serverMutex = PTHREAD_MUTEX_INITIALIZER;
bool isScanning = false; // Prevent multiple simultaneous scans

void logMessage(const std::string &message)
{
    std::string logEntry = "[" + getTimestamp() + "] " + message;
    std::cout << logEntry << std::endl;
    if (logFile.is_open())
    {
        logFile << logEntry << std::endl;
        logFile.flush();
    }
}

// Get local IP address
std::string getLocalIPAddress()
{
    // Try to get real external IP by connecting to external server
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        return "127.0.0.1";
    }

    // Connect to external server (doesn't actually send data)
    // This determines which network interface would be used
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS
    serv_addr.sin_port = htons(53);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sock);
        return "127.0.0.1";
    }

    // Get local address from the socket
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(sock, (struct sockaddr *)&local_addr, &addr_len) < 0)
    {
        close(sock);
        return "127.0.0.1";
    }

    close(sock);

    // Convert to string
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, INET_ADDRSTRLEN);

    return std::string(ip_str);
}

int open_socket(int portno)
{
    struct sockaddr_in sk_addr;
    int sock;
    int set = 1;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket");
        return -1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }

    memset(&sk_addr, 0, sizeof(sk_addr));
    sk_addr.sin_family = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port = htons(portno);

    if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return -1;
    }

    return sock;
}

// Forward declarations
void triggerScan();

// Connect to another server
void connectToServer(const std::string &ip, int port)
{
    logMessage("Attempting to connect to " + ip + ":" + std::to_string(port));

    // Check if already connected
    pthread_mutex_lock(&serverMutex);
    for (const auto &pair : connectedServers)
    {
        if (pair.second.ip == ip && pair.second.port == port)
        {
            pthread_mutex_unlock(&serverMutex);
            logMessage("Already connected to " + ip + ":" + std::to_string(port));
            return;
        }
    }
    pthread_mutex_unlock(&serverMutex);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        logMessage("ERROR: Failed to create socket");
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0)
    {
        logMessage("ERROR: Invalid address");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        logMessage("ERROR: Connection failed to " + ip + ":" + std::to_string(port));
        close(sock);
        return;
    }

    logMessage("SUCCESS: Connected to " + ip + ":" + std::to_string(port));

    // Send HELO
    std::string helo = buildHELO(MY_GROUP_ID);
    if (!sendCommand(sock, helo))
    {
        logMessage("ERROR: Failed to send HELO");
        close(sock);
        return;
    }
    logMessage("Sent: " + helo);

    // Receive SERVERS response
    std::string response;
    if (receiveCommand(sock, response))
    {
        logMessage("Received: " + response);

        // Parse SERVERS response to learn about their neighbors
        std::vector<std::string> tokens = parseCommand(response);
        if (tokens[0] == "SERVERS" && tokens.size() > 1)
        {
            pthread_mutex_lock(&serverMutex);

            // Build server list string (skip "SERVERS" token)
            std::string serverList;
            for (size_t i = 1; i < tokens.size(); i++)
            {
                serverList += tokens[i];
                if (i < tokens.size() - 1)
                    serverList += ",";
            }

            // Split by semicolons
            std::vector<std::string> serverEntries = splitServers(serverList);

            for (const auto &entry : serverEntries)
            {
                std::vector<std::string> parts = parseCommand(entry);
                if (parts.size() >= 3)
                {
                    KnownServer known;
                    known.groupId = parts[0];
                    known.ip = parts[1];
                    known.port = std::stoi(parts[2]);
                    known.lastHeard = time(nullptr);

                    // Don't add ourselves or already connected servers
                    if (known.groupId != MY_GROUP_ID &&
                        connectedGroupIds.find(known.groupId) == connectedGroupIds.end())
                    {

                        // Check if already in known servers
                        bool found = false;
                        for (auto &existing : knownServers)
                        {
                            if (existing.groupId == known.groupId)
                            {
                                existing.lastHeard = time(nullptr); // Update timestamp
                                existing.ip = known.ip;             // Update in case it changed
                                existing.port = known.port;
                                found = true;
                                break;
                            }
                        }

                        if (!found)
                        {
                            knownServers.push_back(known);
                            logMessage("Learned about new server: " + known.groupId +
                                       " at " + known.ip + ":" + std::to_string(known.port));
                        }
                    }
                }
            }

            pthread_mutex_unlock(&serverMutex);
        }

        // Store this connection
        pthread_mutex_lock(&serverMutex);
        ServerInfo info;
        info.socket = sock;
        info.ip = ip;
        info.port = port;
        info.groupId = ""; // Will be filled from their messages
        info.lastSeen = time(nullptr);
        info.isOutgoing = true;
        connectedServers[sock] = info;
        pthread_mutex_unlock(&serverMutex);
    }
    else
    {
        logMessage("ERROR: Failed to receive SERVERS response");
        close(sock);
    }
}

// Try connecting to known servers (fast recovery)
void tryKnownServers()
{
    pthread_mutex_lock(&serverMutex);
    std::vector<KnownServer> candidates = knownServers;
    int currentConnections = connectedServers.size();
    pthread_mutex_unlock(&serverMutex);

    logMessage("Trying to connect to " + std::to_string(candidates.size()) +
               " known servers");

    for (const auto &server : candidates)
    {
        pthread_mutex_lock(&serverMutex);
        currentConnections = connectedServers.size();
        bool alreadyConnected = (connectedGroupIds.find(server.groupId) !=
                                 connectedGroupIds.end());
        pthread_mutex_unlock(&serverMutex);

        if (currentConnections >= 8)
        {
            logMessage("Max connections reached");
            break;
        }

        if (!alreadyConnected)
        {
            connectToServer(server.ip, server.port);
            sleep(1);
        }
    }
}

// Emergency scan when connections drop too low
void triggerScan()
{
    pthread_mutex_lock(&serverMutex);
    if (isScanning)
    {
        pthread_mutex_unlock(&serverMutex);
        logMessage("Scan already in progress, skipping");
        return;
    }
    isScanning = true;
    pthread_mutex_unlock(&serverMutex);

    logMessage("=== Emergency scan triggered ===");

    // First try known servers (fast - just a few seconds)
    tryKnownServers();

    // Check if we now have enough connections
    pthread_mutex_lock(&serverMutex);
    int connections = connectedServers.size();
    pthread_mutex_unlock(&serverMutex);

    if (connections >= 3)
    {
        logMessage("✅ Sufficient connections after trying known servers");
        pthread_mutex_lock(&serverMutex);
        isScanning = false;
        pthread_mutex_unlock(&serverMutex);
        return;
    }

    // Still not enough - do full port scan
    logMessage("Not enough connections, performing full port scan");

    std::vector<int> studentPorts = scanForServers(TSAM_SERVER_IP, 4000, 4200, listenPort);

    logMessage("Found " + std::to_string(studentPorts.size()) + " student servers");

    // Try to connect to discovered servers
    for (int port : studentPorts)
    {
        pthread_mutex_lock(&serverMutex);
        int current = connectedServers.size();
        pthread_mutex_unlock(&serverMutex);

        if (current >= 8)
            break;

        connectToServer(TSAM_SERVER_IP, port);
        sleep(1);
    }

    // Check instructor servers as last resort
    pthread_mutex_lock(&serverMutex);
    connections = connectedServers.size();
    pthread_mutex_unlock(&serverMutex);

    if (connections < 3)
    {
        logMessage("Still below minimum, trying instructor servers");
        for (int port : INSTRUCTOR_PORTS)
        {
            if (isPortOpen(TSAM_SERVER_IP, port, 500))
            {
                logMessage("Found instructor on port " + std::to_string(port));
                connectToServer(TSAM_SERVER_IP, port);

                pthread_mutex_lock(&serverMutex);
                if (connectedServers.size() >= 3)
                {
                    pthread_mutex_unlock(&serverMutex);
                    break;
                }
                pthread_mutex_unlock(&serverMutex);

                sleep(1);
            }
        }
    }

    pthread_mutex_lock(&serverMutex);
    isScanning = false;
    pthread_mutex_unlock(&serverMutex);

    logMessage("=== Emergency scan complete ===");
}

// Health monitor thread - checks connections periodically
// Enhanced healthMonitorThread with proactive commands

void *healthMonitorThread(void *arg)
{
    time_t lastKeepalive = time(nullptr);
    time_t lastGetmsgs = time(nullptr);      
    time_t lastStatusReq = time(nullptr);    
    
    while (true)
    {
        sleep(30); // Check every 30 seconds

        time_t now = time(nullptr);
        
        // ===== Send KEEPALIVE every 60 seconds =====
        if (now - lastKeepalive >= 60)
        {
            pthread_mutex_lock(&serverMutex);
            
            for (const auto &pair : connectedServers)
            {
                if (!pair.second.groupId.empty() && pair.second.isOutgoing)
                {
                    int msgCount = messageQueue[pair.second.groupId].size();
                    std::string keepalive = buildKEEPALIVE(msgCount);
                    
                    if (sendCommand(pair.first, keepalive))
                    {
                        logMessage("Sent keepalive to " + pair.second.groupId + 
                                  " (" + std::to_string(msgCount) + " msgs)");
                    }
                }
            }
            
            pthread_mutex_unlock(&serverMutex);
            lastKeepalive = now;
        }
        
        if (now - lastGetmsgs >= 90)
        {
            pthread_mutex_lock(&serverMutex);
            
            for (const auto &pair : connectedServers)
            {
                if (!pair.second.groupId.empty())
                {
                    std::string getmsgs = buildGETMSGS(MY_GROUP_ID);
                    
                    if (sendCommand(pair.first, getmsgs))
                    {
                        logMessage("Sent GETMSGS to " + pair.second.groupId);
                    }
                }
            }
            
            pthread_mutex_unlock(&serverMutex);
            lastGetmsgs = now;
        }
        
        if (now - lastStatusReq >= 180)
        {
            pthread_mutex_lock(&serverMutex);
            
            for (const auto &pair : connectedServers)
            {
                if (!pair.second.groupId.empty())
                {
                    std::string statusreq = buildSTATUSREQ();
                    
                    if (sendCommand(pair.first, statusreq))
                    {
                        logMessage("Sent STATUSREQ to " + pair.second.groupId);
                    }
                }
            }
            
            pthread_mutex_unlock(&serverMutex);
            lastStatusReq = now;
        }

        pthread_mutex_lock(&serverMutex);

        // Clean up dead connections
        std::vector<int> toRemove;

        for (auto &pair : connectedServers)
        {
            if (now - pair.second.lastSeen > 180)
            {
                logMessage("Connection timeout: " + pair.second.groupId);
                toRemove.push_back(pair.first);
            }
        }

        for (int sock : toRemove)
        {
            connectedGroupIds.erase(connectedServers[sock].groupId);
            close(sock);
            connectedServers.erase(sock);
        }

        int connections = connectedServers.size();
        pthread_mutex_unlock(&serverMutex);

        logMessage("Health check: " + std::to_string(connections) + " active connections");

        if (connections < 3)
        {
            logMessage("Below minimum connections (" + std::to_string(connections) +
                       "/3), triggering discovery");
            triggerScan();
        }
    }

    return NULL;
}

// Handle commands from OTHER SERVERS
void handleServerCommand(int clientSocket, const std::string &command)
{
    logMessage("Received from server: " + command);

    std::vector<std::string> tokens = parseCommand(command);

    if (tokens.empty())
        return;

    // Update last seen time
    pthread_mutex_lock(&serverMutex);
    if (connectedServers.find(clientSocket) != connectedServers.end())
    {
        connectedServers[clientSocket].lastSeen = time(nullptr);
    }
    pthread_mutex_unlock(&serverMutex);

    // Handle HELO from other servers
    if (tokens[0] == "HELO" && tokens.size() >= 2)
    {
        std::string fromGroup = tokens[1];

        pthread_mutex_lock(&serverMutex);
        connectedServers[clientSocket].groupId = fromGroup;
        connectedGroupIds.insert(fromGroup);
        pthread_mutex_unlock(&serverMutex);

        logMessage("Server " + fromGroup + " connected to us");

        // Build SERVERS response with our neighbors
        std::vector<std::tuple<std::string, std::string, int>> servers;
        servers.push_back(std::make_tuple(MY_GROUP_ID, myIpAddress, listenPort));

        pthread_mutex_lock(&serverMutex);
        for (const auto &pair : connectedServers)
        {
            if (pair.first != clientSocket && !pair.second.groupId.empty())
            {
                servers.push_back(std::make_tuple(
                    pair.second.groupId,
                    pair.second.ip,
                    pair.second.port));
            }
        }
        pthread_mutex_unlock(&serverMutex);

        std::string response = buildSERVERS(servers);
        sendCommand(clientSocket, response);
        logMessage("Sent: " + response);
    }
    // Handle KEEPALIVE
    else if (tokens[0] == "KEEPALIVE" && tokens.size() >= 2)
    {
        logMessage("Keepalive from " + connectedServers[clientSocket].groupId);
    }
    else if (tokens[0] == "GETMSGS" && tokens.size() >= 2)
    {
        std::string forGroup = tokens[1];
        
        if (!messageQueue[forGroup].empty())
        {
            std::string msg = messageQueue[forGroup].front();
            messageQueue[forGroup].pop();
            
            std::string response = buildSENDMSG(forGroup, MY_GROUP_ID, msg);
            if (sendCommand(clientSocket, response))
            {
                logMessage("📤 Sent queued message to " + forGroup);
            }
        }
        else
        {
            logMessage("📭 No messages for " + forGroup);
        }
    }
    // Handle SENDMSG
    else if (tokens[0] == "SENDMSG" && tokens.size() >= 4)
    {
        std::string toGroup = tokens[1];
        std::string fromGroup = tokens[2];

        std::string messageContent;
        for (size_t i = 3; i < tokens.size(); i++)
        {
            messageContent += tokens[i];
            if (i < tokens.size() - 1)
                messageContent += ",";
        }

        // Check if message is for us
        if (toGroup == MY_GROUP_ID)
        {
            messageQueue[toGroup].push(messageContent);
            logMessage("Received message for us from " + fromGroup);
        }
        else
        {
            bool forwarded = false;
            
            pthread_mutex_lock(&serverMutex);
            
            for (const auto &pair : connectedServers)
            {
                if (pair.second.groupId == toGroup)
                {
                    pthread_mutex_unlock(&serverMutex);
                    
                    if (sendCommand(pair.first, command))
                    {
                        logMessage("Forwarded message from " + fromGroup + 
                                  " to " + toGroup);
                        forwarded = true;
                    }
                    else
                    {
                        logMessage("Failed to forward message to " + toGroup);
                    }
                    
                    pthread_mutex_lock(&serverMutex);
                    break;
                }
            }
            
            pthread_mutex_unlock(&serverMutex);
            
            if (!forwarded)
            {
                messageQueue[toGroup].push(messageContent);
                logMessage("Stored message from " + fromGroup + " for " + toGroup + 
                          " (not connected yet)");
            }
        }
    }
    else if (tokens[0] == "STATUSREQ")
    {
        std::vector<std::pair<std::string, int>> status;
        
        for (const auto &pair : messageQueue)
        {
            if (!pair.second.empty())
            {
                status.push_back(std::make_pair(pair.first, pair.second.size()));
            }
        }
        
        std::string response = buildSTATUSRESP(status);
        if (sendCommand(clientSocket, response))
        {
            logMessage("Sent status: " + response);
        }
    }
}

// Handle commands from CLIENT
void handleClientCommand(int clientSocket, const std::string &command)
{
    logMessage("Received from client: " + command);

    std::vector<std::string> tokens = parseCommand(command);

    if (tokens.empty())
        return;

    // Handle SENDMSG from client
    if (tokens[0] == "SENDMSG" && tokens.size() >= 3)
    {
        std::string toGroup = tokens[1];
        std::string message;
        for (size_t i = 2; i < tokens.size(); i++)
        {
            message += tokens[i];
            if (i < tokens.size() - 1)
                message += ",";
        }

        logMessage("Client wants to send message to " + toGroup + ": " + message);

        // ✅ TRY TO FORWARD IMMEDIATELY
        bool forwarded = false;
        
        pthread_mutex_lock(&serverMutex);
        
        // Check if we're connected to destination
        for (const auto &pair : connectedServers)
        {
            if (pair.second.groupId == toGroup)
            {
                pthread_mutex_unlock(&serverMutex);
                
                // Build proper SENDMSG command with FROM field
                std::string forwardCmd = buildSENDMSG(toGroup, MY_GROUP_ID, message);
                
                if (sendCommand(pair.first, forwardCmd))
                {
                    logMessage("📬 Forwarded message to " + toGroup + " immediately");
                    forwarded = true;
                }
                else
                {
                    logMessage("❌ Failed to forward to " + toGroup);
                }
                
                pthread_mutex_lock(&serverMutex);
                break;
            }
        }
        
        pthread_mutex_unlock(&serverMutex);
        
        // If not forwarded, store for later
        if (!forwarded)
        {
            messageQueue[toGroup].push(message);
            logMessage("📦 Stored message for " + toGroup + " (not connected)");
        }

        // Send acknowledgment to client
        std::string response = forwarded ? 
            "OK,Message delivered to " + toGroup :
            "OK,Message queued for " + toGroup;
        sendCommand(clientSocket, response);
        logMessage("Sent: " + response);
    }
    // Handle GETMSG from client
    else if (tokens[0] == "GETMSG")
    {
        if (!messageQueue[MY_GROUP_ID].empty())
        {
            std::string msg = messageQueue[MY_GROUP_ID].front();
            messageQueue[MY_GROUP_ID].pop();

            std::string response = "SENDMSG," + MY_GROUP_ID + ",Unknown," + msg;
            sendCommand(clientSocket, response);

            logMessage("Delivered message to client: " + msg);
        }
        else
        {
            std::string response = "NO_MESSAGES";
            sendCommand(clientSocket, response);
            logMessage("No messages available");
        }
    }
    // Handle LISTSERVERS from client
    else if (tokens[0] == "LISTSERVERS")
    {
        pthread_mutex_lock(&serverMutex);

        std::string response = "SERVERS," + MY_GROUP_ID + "," + myIpAddress + "," +
                               std::to_string(listenPort);

        // Add connected servers
        for (const auto &pair : connectedServers)
        {
            if (!pair.second.groupId.empty())
            {
                response += ";" + pair.second.groupId + "," +
                            pair.second.ip + "," + std::to_string(pair.second.port);
            }
        }

        pthread_mutex_unlock(&serverMutex);

        sendCommand(clientSocket, response);
        logMessage("Sent server list: " + response);
    }
    else
    {
        logMessage("Unknown command: " + tokens[0]);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <port> [server_ip:port] ...\n", argv[0]);
        printf("Example: %s 4044\n", argv[0]);
        printf("Example: %s 4044 130.208.246.98:5001\n", argv[0]);
        printf("\nThe server will:\n");
        printf("  - Accept incoming connections from other servers\n");
        printf("  - Learn about neighbors passively\n");
        printf("  - Only scan when connections drop below 3\n");
        exit(0);
    }

    listenPort = atoi(argv[1]);
    myIpAddress = getLocalIPAddress();

    // Open log file
    std::string logFilename = MY_GROUP_ID + "_server.log";
    logFile.open(logFilename, std::ios::app);

    logMessage("========================================");
    logMessage("Server starting: " + MY_GROUP_ID);
    logMessage("Port: " + std::to_string(listenPort));
    logMessage("IP: " + myIpAddress);
    logMessage("Mode: REACTIVE DISCOVERY");
    logMessage("========================================");

    // Open listening socket
    int listenSock = open_socket(listenPort);
    if (listenSock < 0)
    {
        logMessage("Failed to open listening socket");
        exit(1);
    }

    if (listen(listenSock, 10) < 0)
    {
        logMessage("Listen failed on port " + std::to_string(listenPort));
        exit(1);
    }

    logMessage("Server listening on port " + std::to_string(listenPort));

    // Start health monitor thread
    pthread_t healthThreadId;
    if (pthread_create(&healthThreadId, NULL, healthMonitorThread, NULL) == 0)
    {
        pthread_detach(healthThreadId);
        logMessage("Health monitor started");
    }

    // Connect to any servers specified on command line
    for (int i = 2; i < argc; i++)
    {
        std::string serverAddr(argv[i]);
        size_t colonPos = serverAddr.find(':');
        if (colonPos != std::string::npos)
        {
            std::string ip = serverAddr.substr(0, colonPos);
            int port = std::stoi(serverAddr.substr(colonPos + 1));

            logMessage("Connecting to initial server: " + ip + ":" + std::to_string(port));
            connectToServer(ip, port);
            sleep(1);
        }
    }

    // Add some test messages for demo
    messageQueue[MY_GROUP_ID].push("Welcome to the botnet!");
    messageQueue[MY_GROUP_ID].push("Server is running in reactive mode");

    logMessage("Waiting for connections...");

    // Main accept loop
    // Replace the main accept loop (around line 595-655) with this:

    // Main accept loop
    while (true)
    {
        struct sockaddr_in client;
        socklen_t clientLen = sizeof(client);

        int clientSock = accept(listenSock, (struct sockaddr *)&client, &clientLen);

        if (clientSock < 0)
        {
            perror("Accept failed");
            continue;
        }

        std::string clientIp = inet_ntoa(client.sin_addr);
        int clientPort = ntohs(client.sin_port);

        logMessage("Accepted connection from " + clientIp + ":" +
                std::to_string(clientPort));

        // Add to connected servers (will be identified by HELO)
        pthread_mutex_lock(&serverMutex);
        ServerInfo info;
        info.socket = clientSock;
        info.ip = clientIp;
        info.port = 0;
        info.groupId = "";
        info.lastSeen = time(nullptr);
        info.isOutgoing = false;
        connectedServers[clientSock] = info;
        pthread_mutex_unlock(&serverMutex);

        // Receive first command
        std::string command;
        if (receiveCommand(clientSock, command))
        {
            std::vector<std::string> tokens = parseCommand(command);

            if (!tokens.empty() && tokens[0] == "HELO")
            {
                // This is a server connecting to us
                handleServerCommand(clientSock, command);
                // Don't close - keep connection open for servers
                logMessage("Peer server connection maintained");
            }
            else
            {
                // This is a client - handle first command
                handleClientCommand(clientSock, command);
                
                // *** FIX: Loop to handle multiple commands from client ***
                while (receiveCommand(clientSock, command))
                {
                    handleClientCommand(clientSock, command);
                }
                
                // Close connection only when client disconnects
                close(clientSock);
                pthread_mutex_lock(&serverMutex);
                connectedServers.erase(clientSock);
                pthread_mutex_unlock(&serverMutex);
                logMessage("Client connection closed");
            }
        }
        else
        {
            logMessage("Failed to receive command or connection closed");
            close(clientSock);
            pthread_mutex_lock(&serverMutex);
            connectedServers.erase(clientSock);
            pthread_mutex_unlock(&serverMutex);
        }
    }

    close(listenSock);
    logFile.close();
    return 0;
}