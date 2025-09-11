#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    const char* server_ip = "127.0.0.1";
    const int server_port = 5000;   
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Error: could not create socket\n";
        return 1;
    }
    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(5000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress,
            sizeof(serverAddress));

    // sending data
    while (true) {
        std::string message;
        std::cout << "Enter command: ";
        std::getline(std::cin, message);
        if (message == "exit") {
            std::cout << "Exiting client.\n";
            break;
        }
        send(clientSocket, message.c_str(), message.size(), 0);
        // receive the response
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            std::cout << "Server output:\n" << buffer << std::endl;
        } else if (bytesReceived == 0) {
            std::cout << "Server closed connection.\n";
            break;
        } else {
            perror("recv failed");
            break;
        }
        
    }

    // closing socket
    close(clientSocket);

    return 0;
}