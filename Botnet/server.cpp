#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <map>
#include <queue>
#include <vector>


const std::string MY_GROUP_ID = "A5_1";

// Simple message queue for this stub
std::map<std::string, std::queue<std::string>> messageQueue;
std::ofstream logFile;

void logMessage(const std::string& message) {
    std::string logEntry = "[" + getTimestamp() + "] " + message;
    std::cout << logEntry << std::endl;
    if (logFile.is_open()) {
        logFile << logEntry << std::endl;
        logFile.flush();
    }
}

// Open socket for listening
int open_socket(int portno) {
    struct sockaddr_in sk_addr;
    int sock;
    int set = 1;

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to open socket");
        return -1;
    }

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
        perror("Failed to set SO_REUSEADDR:");
    }

    memset(&sk_addr, 0, sizeof(sk_addr));
    sk_addr.sin_family = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port = htons(portno);

    if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0) {
        perror("Failed to bind to socket:");
        return -1;
    }

    return sock;
}

// Handle commands from client
void handleClientCommand(int clientSocket, const std::string& command) {
    logMessage("Received: " + command);
    
    std::vector<std::string> tokens = parseCommand(command);
    
    if (tokens.empty()) {
        return;
    }
    
    // Handle SENDMSG from client
    if (tokens[0] == "SENDMSG" && tokens.size() >= 3) {
        std::string toGroup = tokens[1];
        std::string message;
        for (size_t i = 2; i < tokens.size(); i++) {
            message += tokens[i];
            if (i < tokens.size() - 1) message += ",";
        }
        
        // For stub: just store the message
        std::string fullMsg = "From: " + MY_GROUP_ID + " - " + message;
        messageQueue[toGroup].push(fullMsg);
        
        logMessage("Stored message for " + toGroup + ": " + message);
        
        // Send acknowledgment (optional, but nice for client)
        std::string response = "OK,Message queued for " + toGroup;
        sendCommand(clientSocket, response);
        logMessage("Sent: " + response);
    }
    // Handle GETMSG from client (client sends just "GETMSG")
    else if (tokens[0] == "GETMSG") {
        if (!messageQueue[MY_GROUP_ID].empty()) {
            std::string msg = messageQueue[MY_GROUP_ID].front();
            messageQueue[MY_GROUP_ID].pop();
            
            // Send message back to client
            std::string response = "SENDMSG," + MY_GROUP_ID + ",TestServer," + msg;
            sendCommand(clientSocket, response);
            
            logMessage("Delivered message to client: " + msg);
        } else {
            std::string response = "NO_MESSAGES";
            sendCommand(clientSocket, response);
            logMessage("No messages available");
        }
    }
    // Handle LISTSERVERS from client
    else if (tokens[0] == "LISTSERVERS") {
        // For stub: return empty or mock server list
        std::string response = "SERVERS," + MY_GROUP_ID + ",127.0.0.1,4044";
        sendCommand(clientSocket, response);
        logMessage("Sent server list: " + response);
    }
    else {
        logMessage("Unknown command: " + tokens[0]);
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        printf("Example: %s 4044\n", argv[0]);
        exit(0);
    }

    int listenPort = atoi(argv[1]);
    
    // Open log file
    std::string logFilename = MY_GROUP_ID + "_server.log";
    logFile.open(logFilename, std::ios::app);
    
    logMessage("========================================");
    logMessage("Server starting: " + MY_GROUP_ID);
    logMessage("========================================");
    
    // Open listening socket
    int listenSock = open_socket(listenPort);
    if (listenSock < 0) {
        logMessage("Failed to open listening socket");
        exit(1);
    }
    
    if(listen(listenSock, 10) < 0) {
        logMessage("Listen failed on port " + std::to_string(listenPort));
        exit(1);
    }
    
    logMessage("Server listening on port " + std::to_string(listenPort));
    logMessage("Waiting for client connections...");
    
    // Add some test messages for demo purposes
    messageQueue[MY_GROUP_ID].push("Welcome to the botnet!");
    messageQueue[MY_GROUP_ID].push("This is a test message");
    
    // Main accept loop
    while(true) {
        struct sockaddr_in client;
        socklen_t clientLen = sizeof(client);
        
        int clientSock = accept(listenSock, (struct sockaddr *)&client, &clientLen);
        
        if (clientSock < 0) {
            perror("Accept failed");
            continue;
        }
        
        std::string clientIp = inet_ntoa(client.sin_addr);
        int clientPort = ntohs(client.sin_port);
        
        logMessage("Accepted connection from " + clientIp + ":" + std::to_string(clientPort));
        
        // Receive command from client
        std::string command;
        if (receiveCommand(clientSock, command)) {
            handleClientCommand(clientSock, command);
        } else {
            logMessage("Failed to receive command or connection closed");
        }
        
        // Close client connection after handling command
        close(clientSock);
        logMessage("Closed connection from " + clientIp + ":" + std::to_string(clientPort));
    }
    
    close(listenSock);
    logFile.close();
    return 0;
}