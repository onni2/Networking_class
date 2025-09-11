#include <sys/socket.h>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <vector>
int main(int argc, char *argv[]) {
    int sock;
    char buffer[1025];
    if (argc != 3){
        std::cout << "Usage: " << argv[0] << "<port> <ip>" << std::endl;
        return 1;
    }
    int port = atoi(argv[1]); //TODO: make this a command line argument
    char *ip_string =  argv[2];//TODO  check for right number of arguments
    //create a socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Error: could not create socket\n";
        return 1;
    }
    //construct a destiation address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_string, &server_addr.sin_addr) != 1){
        std::cout << "ip_address is weird" << std::endl;
        exit(0);
    }
    if (connect(sock, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(0);
    };
    std::string message;
    while (!(message == "exit") ){
        std::cout << "Enter command: ";
        std::getline(std::cin, message);

        int n = send(sock, message.c_str(), message.length(), 0);
        if (n< static_cast<ssize_t>(message.length())) {
            std::cerr << "Error: could not send complete message\n";
        }
        int recive = recv(sock, buffer, sizeof(buffer),0);
        if (recive < 0){
            std::cout << "ERROR" << std::endl;
        } else if (recive == 0) {
            std::cout << "Lost connection, trying to reconnect" << std::endl;
            if (connect(sock, (const sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                perror("connect failed");
                exit(0);
            };
        } else {
            buffer[recive] = '\0';
            std::stringstream stream(buffer);
            std::string line;
            std::cout << "We are receiving word from server!:\n" << std::endl;
            while (std::getline(stream, line)) {
                std::cout << line << std::endl;
            }
            
            

        }
    }
    if (close(sock) < 0) {
        std::cerr << "Error: could not close socket\n";
        return 1;
    }
}   