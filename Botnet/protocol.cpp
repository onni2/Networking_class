#include "protocol.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <cstring>
#include <ctime>
#include <iomanip>

bool sendCommand(int socket, const std::string& command) {
    // Check message length
    if (command.length() > MAX_MESSAGE_LENGTH - HEADER_SIZE) {
        std::cerr << "Command too long: " << command.length() << " bytes" << std::endl;
        return false;
    }

    // Calculate total length including framing
    uint16_t totalLength = command.length() + HEADER_SIZE;
    
    // Convert length to network byte order
    uint16_t networkLength = htons(totalLength);

    // Build the message: <SOH><length><STX><command><ETX>
    std::string frame;
    frame += SOH;
    frame += std::string(reinterpret_cast<char*>(&networkLength), 2);
    frame += STX;
    frame += command;
    frame += ETX;

    // Send the entire frame
    size_t totalSent = 0;
    while (totalSent < frame.length()) {
        ssize_t sent = send(socket, frame.c_str() + totalSent, 
                           frame.length() - totalSent, 0);
        if (sent <= 0) {
            perror("send failed");
            return false;
        }
        totalSent += sent;
    }

    return true;
}

bool receiveCommand(int socket, std::string& command) {
    char buffer[MAX_MESSAGE_LENGTH + 1];
    
    // Read SOH
    ssize_t n = recv(socket, buffer, 1, MSG_WAITALL);
    if (n <= 0) {
        return false; // Connection closed or error
    }
    
    if (buffer[0] != SOH) {
        std::cerr << "Expected SOH, got: 0x" << std::hex 
                  << (int)(unsigned char)buffer[0] << std::dec << std::endl;
        return false;
    }

    // Read length (2 bytes)
    n = recv(socket, buffer, 2, MSG_WAITALL);
    if (n != 2) {
        return false;
    }
    
    uint16_t networkLength;
    memcpy(&networkLength, buffer, 2);
    uint16_t totalLength = ntohs(networkLength);

    // Validate length
    if (totalLength < HEADER_SIZE || totalLength > MAX_MESSAGE_LENGTH) {
        std::cerr << "Invalid message length: " << totalLength << std::endl;
        return false;
    }

    // Calculate command length
    uint16_t commandLength = totalLength - HEADER_SIZE;

    // Read STX
    n = recv(socket, buffer, 1, MSG_WAITALL);
    if (n != 1 || buffer[0] != STX) {
        std::cerr << "Expected STX" << std::endl;
        return false;
    }

    // Read command content
    size_t totalReceived = 0;
    while (totalReceived < commandLength) {
        n = recv(socket, buffer + totalReceived, 
                commandLength - totalReceived, 0);
        if (n <= 0) {
            return false;
        }
        totalReceived += n;
    }
    buffer[commandLength] = '\0';
    command = std::string(buffer, commandLength);

    // Read ETX
    n = recv(socket, buffer, 1, MSG_WAITALL);
    if (n != 1 || buffer[0] != ETX) {
        std::cerr << "Expected ETX" << std::endl;
        return false;
    }

    return true;
}

std::vector<std::string> parseCommand(const std::string& command) {
    std::vector<std::string> tokens;
    std::stringstream ss(command);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    
    return tokens;
}

// New function to parse SERVERS response with semicolon separators
std::vector<std::string> splitServers(const std::string& serverList) {
    std::vector<std::string> servers;
    std::stringstream ss(serverList);
    std::string server;
    
    while (std::getline(ss, server, ';')) {
        servers.push_back(server);
    }
    
    return servers;
}

std::string buildHELO(const std::string& groupId) {
    return "HELO," + groupId;
}

std::string buildSERVERS(const std::vector<std::tuple<std::string, std::string, int>>& servers) {
    std::string cmd = "SERVERS";
    
    for (size_t i = 0; i < servers.size(); i++) {
        cmd += "," + std::get<0>(servers[i]) + "," + 
               std::get<1>(servers[i]) + "," + 
               std::to_string(std::get<2>(servers[i]));
        
        if (i < servers.size() - 1) {
            cmd += ";";
        }
    }
    
    return cmd;
}

std::string buildKEEPALIVE(int messageCount) {
    return "KEEPALIVE," + std::to_string(messageCount);
}

std::string buildGETMSGS(const std::string& groupId) {
    return "GETMSGS," + groupId;
}

std::string buildSENDMSG(const std::string& toGroup, const std::string& fromGroup, 
                         const std::string& message) {
    return "SENDMSG," + toGroup + "," + fromGroup + "," + message;
}

std::string buildSTATUSREQ() {
    return "STATUSREQ";
}

std::string buildSTATUSRESP(const std::vector<std::pair<std::string, int>>& serverMessages) {
    std::string cmd = "STATUSRESP";
    
    for (size_t i = 0; i < serverMessages.size(); i++) {
        cmd += "," + serverMessages[i].first + "," + 
               std::to_string(serverMessages[i].second);
    }
    
    return cmd;
}

std::string getTimestamp() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    return std::string(buffer);
}