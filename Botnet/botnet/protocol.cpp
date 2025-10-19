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
    if (command.length() > MAX_MESSAGE_LENGTH - HEADER_SIZE) {
        std::cerr << "Command too long: " << command.length() << " bytes" << std::endl;
        return false;
    }

    uint16_t totalLength = command.length() + HEADER_SIZE;
    
    std::string frame;
    frame += SOH;
    
    // Send length in BIG-ENDIAN (network byte order) - high byte first
    frame += static_cast<char>((totalLength >> 8) & 0xFF);  // High byte
    frame += static_cast<char>(totalLength & 0xFF);         // Low byte
    
    frame += STX;
    frame += command;
    frame += ETX;

    size_t totalSent = 0;
    while (totalSent < frame.length()) {
        ssize_t sent = send(socket, frame.c_str() + totalSent, frame.length() - totalSent, 0);
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
        return false;
    }
    
    if (buffer[0] != SOH) {
        std::cerr << "Expected SOH, got: 0x" << std::hex 
                  << (int)(unsigned char)buffer[0] << std::dec << std::endl;
        return false;
    }

    // Read length (2 bytes) - BIG-ENDIAN
    n = recv(socket, buffer, 2, MSG_WAITALL);
    if (n != 2) {
        return false;
    }
    
    // Extract length from big-endian bytes
    uint16_t totalLength = (static_cast<unsigned char>(buffer[0]) << 8) | 
                           static_cast<unsigned char>(buffer[1]);

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

std::vector<std::string> splitServers(const std::string& serverList) {
    std::vector<std::string> servers;
    std::stringstream ss(serverList);
    std::string server;
    
    while (std::getline(ss, server, ';')) {
        if (!server.empty()) {  // Skip empty entries
            servers.push_back(server);
        }
    }
    
    return servers;
}

bool parseSENDMSGWithHops(const std::string& command, std::string& mainCommand, std::string& hops) {
    // Find EOT marker that separates command from hops
    size_t eotPos = command.find(EOT);
    
    if (eotPos == std::string::npos) {
        // No hops present - backward compatible
        mainCommand = command;
        hops = "";
        return true;
    }
    
    // Split at EOT
    mainCommand = command.substr(0, eotPos);
    hops = command.substr(eotPos + 1);
    
    return true;
}

bool isInHops(const std::string& hops, const std::string& groupId) {
    if (hops.empty()) {
        return false;
    }
    
    // Check if groupId appears in the comma-separated hop list
    std::stringstream ss(hops);
    std::string hop;
    
    while (std::getline(ss, hop, ',')) {
        if (hop == groupId) {
            return true;
        }
    }
    
    return false;
}

std::string buildHELO(const std::string& groupId) {
    return "HELO," + groupId;
}

// FIXED: Correct SERVERS format per spec
// Format: SERVERS,id1,ip1,port1;id2,ip2,port2;...
std::string buildSERVERS(const std::vector<std::tuple<std::string, std::string, int>>& servers) {
    if (servers.empty()) {
        return "SERVERS";
    }
    
    std::string cmd = "SERVERS";
    
    for (size_t i = 0; i < servers.size(); i++) {
        if (i == 0) {
            cmd += ",";  // First comma after SERVERS
        } else {
            cmd += ";";  // Semicolon before subsequent servers
        }
        
        cmd += std::get<0>(servers[i]) + "," +
               std::get<1>(servers[i]) + "," +
               std::to_string(std::get<2>(servers[i]));
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
                         const std::string& message, const std::string& hops) {
    std::string cmd = "SENDMSG," + toGroup + "," + fromGroup + "," + message;
    
    // Add hop tracking if hops provided
    if (!hops.empty()) {
        // Check if adding hops would exceed max length
        if (cmd.length() + 1 + hops.length() <= MAX_PAYLOAD_LENGTH) {
            cmd += EOT;
            cmd += hops;
        } else {
            std::cerr << "Warning: Hops too long, truncating" << std::endl;
        }
    }
    
    return cmd;
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