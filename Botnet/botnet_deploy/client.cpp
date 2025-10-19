#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>

const std::string MY_GROUP_ID = "A5_1";

std::ofstream logFile;

void logMessage(const std::string& message) {
    std::string logEntry = "[" + getTimestamp() + "] " + message;
    std::cout << logEntry << std::endl;
    if (logFile.is_open()) {
        logFile << logEntry << std::endl;
        logFile.flush();
    }
}

void printUsage() {
    std::cout << "\n=== Client Commands ===" << std::endl;
    std::cout << "GETMSG                          - Get a message for your group" << std::endl;
    std::cout << "SENDMSG,<group_id>,<message>    - Send message to another group" << std::endl;
    std::cout << "LISTSERVERS                     - List connected servers" << std::endl;
    std::cout << "QUIT                            - Exit client" << std::endl;
    std::cout << "======================\n" << std::endl;
}

int main(int argc, char* argv[]) {
    if(argc != 3) {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        printUsage();
        exit(0);
    }

    // Open log file
    std::string logFilename = MY_GROUP_ID + "_client.log";
    logFile.open(logFilename, std::ios::app);
    
    logMessage("========================================");
    logMessage("Client starting: " + MY_GROUP_ID);
    logMessage("========================================");

    std::string serverIp = argv[1];
    int serverPort = atoi(argv[2]);

    logMessage("Client for group: " + MY_GROUP_ID);
    logMessage("Server: " + serverIp + ":" + std::to_string(serverPort));
    
    printUsage();

    // *** FIX: Create persistent connection ONCE ***
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        logMessage("ERROR: Failed to create socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    
    if(inet_pton(AF_INET, serverIp.c_str(), &serv_addr.sin_addr) <= 0) {
        logMessage("ERROR: Invalid server address");
        close(serverSocket);
        exit(1);
    }

    if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        logMessage("ERROR: Connection to server failed");
        close(serverSocket);
        exit(1);
    }

    logMessage("✅ Connected to server successfully");

    // Command loop - using same connection
    std::string input;
    while(true) {
        std::cout << MY_GROUP_ID << "> ";
        std::getline(std::cin, input);
        
        if(input.empty()) continue;
        
        // Parse command
        std::vector<std::string> tokens = parseCommand(input);
        
        if(tokens.empty()) continue;
        
        // Handle QUIT locally
        if(tokens[0] == "QUIT") {
            logMessage("Client exiting");
            break;
        }
        
        // *** FIX: Use the persistent connection ***
        // Process command
        if(tokens[0] == "SENDMSG" && tokens.size() >= 3) {
            std::string toGroup = tokens[1];
            std::string message;
            for(size_t i = 2; i < tokens.size(); i++) {
                message += tokens[i];
                if(i < tokens.size() - 1) message += ",";
            }
            
            // Client sends: SENDMSG,TO_GROUP,FROM_GROUP,message
            std::string cmd = "SENDMSG," + toGroup + "," + message;
            if(sendCommand(serverSocket, cmd)) {
                logMessage("Sent: " + cmd);
                
                // Wait for acknowledgment
                std::string response;
                if(receiveCommand(serverSocket, response)) {
                    logMessage("Received: " + response);
                    std::cout << "Message sent to " << toGroup << std::endl;
                } else {
                    logMessage("ERROR: Failed to receive response or connection closed");
                    break; // Exit if connection lost
                }
            } else {
                logMessage("ERROR: Failed to send message");
                break; // Exit if send failed
            }
        }
        else if(tokens[0] == "GETMSG") {
            // Client sends just: GETMSG 
            std::string cmd = "GETMSG";
            if(sendCommand(serverSocket, cmd)) {
                logMessage("Sent: GETMSG");
                
                std::string response;
                if(receiveCommand(serverSocket, response)) {
                    logMessage("Received: " + response);
                    
                    if(response == "NO_MESSAGES") {
                        std::cout << "No messages available" << std::endl;
                    } else {
                        // Parse SENDMSG response
                        std::vector<std::string> respTokens = parseCommand(response);
                        if(respTokens[0] == "SENDMSG" && respTokens.size() >= 4) {
                            std::string toGroup = respTokens[1];
                            std::string message;
                            for(size_t i = 2; i < respTokens.size(); i++) {
                                message += respTokens[i];
                                if(i < respTokens.size() - 1) message += ",";
                            }
                            std::cout << "Message from " << ": " << message << std::endl;
                        } else {
                            std::cout << response << std::endl;
                        }
                    }
                } else {
                    logMessage("ERROR: Failed to receive response or connection closed");
                    break; // Exit if connection lost
                }
            } else {
                logMessage("ERROR: Failed to send GETMSG");
                break; // Exit if send failed
            }
        }
        else if(tokens[0] == "LISTSERVERS") {
            // Client sends: LISTSERVERS 
            std::string cmd = "LISTSERVERS";
            if(sendCommand(serverSocket, cmd)) {
                logMessage("Sent: LISTSERVERS");
                
                std::string response;
                if(receiveCommand(serverSocket, response)) {
                    logMessage("Received: " + response);
                    
                    // Parse SERVERS response
                    std::vector<std::string> respTokens = parseCommand(response);
                    if(respTokens[0] == "SERVERS") {
                        if(respTokens.size() == 1) {
                            std::cout << "No servers connected" << std::endl;
                        } else {
                            std::cout << "Connected servers:" << std::endl;
                            // Server list format: SERVERS,id,ip,port;id,ip,port
                            std::string serverList;
                            for(size_t i = 1; i < respTokens.size(); i++) {
                                serverList += respTokens[i];
                                if(i < respTokens.size() - 1) serverList += ",";
                            }
                            
                            std::vector<std::string> servers = splitServers(serverList);
                            for(const auto& server : servers) {
                                std::vector<std::string> parts = parseCommand(server);
                                if(parts.size() >= 3) {
                                    std::cout << "  - " << parts[0] << " @ " 
                                              << parts[1] << ":" << parts[2] << std::endl;
                                }
                            }
                        }
                    } else {
                        std::cout << response << std::endl;
                    }
                } else {
                    logMessage("ERROR: Failed to receive response or connection closed");
                    break; // Exit if connection lost
                }
            } else {
                logMessage("ERROR: Failed to send LISTSERVERS");
                break; // Exit if send failed
            }
        }
        else {
            std::cout << "Unknown command. Type one of: GETMSG, SENDMSG, LISTSERVERS, QUIT" << std::endl;
        }
    }

    // *** FIX: Close connection only on exit ***
    close(serverSocket);
    logMessage("Connection closed");
    logFile.close();
    return 0;
}