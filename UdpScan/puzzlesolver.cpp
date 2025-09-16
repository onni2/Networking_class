#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

// Global for signature (kept since later ports depend on it)
char sig_msg[5];

// Helper: print error and exit
void die(const std::string& msg) {
    perror(msg.c_str());
    exit(EXIT_FAILURE);
}

// Helper: print hex for debugging
void printHex(const std::vector<uint8_t>& data) {
    for (uint8_t b : data) {
        printf("%02X ", b);
    }
    printf("\n");
}

// Construct secret message (S + secret_num + usernames)
std::vector<uint8_t> generateSecretMessage(uint32_t secret_num, const std::string& users) {
    std::vector<uint8_t> msg;
    msg.reserve(1 + 4 + users.size());

    msg.push_back('S'); // first byte
    uint32_t secret_net = htonl(secret_num);
    const uint8_t* ptr = reinterpret_cast<uint8_t*>(&secret_net);
    msg.insert(msg.end(), ptr, ptr + 4);
    msg.insert(msg.end(), users.begin(), users.end());

    return msg;
}

// Port handlers
void handlePortPingOrPassword(int sock, sockaddr_in& target, bool mode) {
    const char* msg = mode ? "ping1" : "123456";
    if (sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }
}

void handlePortWithMessage(int sock, sockaddr_in& target, bool mode,
                           const char* msg, size_t msg_len) {
    const char* payload = mode ? msg : "123456";
    size_t len = mode ? msg_len : strlen(payload);

    if (sendto(sock, payload, len, 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }
}

void handleSecretPort(int sock, sockaddr_in& target, bool mode,
                      const std::vector<uint8_t>& secret_msg, uint32_t secret_num) {
    // Step 2: send initial secret message
    const char* msg = mode ? reinterpret_cast<const char*>(secret_msg.data()) : "123456";
    size_t len = mode ? secret_msg.size() : strlen(msg);

    if (sendto(sock, msg, len, 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }

    // Step 3: receive 5-byte challenge
    char response[5];
    socklen_t sender_len = sizeof(target);
    ssize_t recv_bytes = recvfrom(sock, response, sizeof(response), 0,
                                  (sockaddr*)&target, &sender_len);
    if (recv_bytes != 5) {
        std::cerr << "Invalid challenge length received\n";
        return;
    }

    // Step 4: extract challenge and compute XOR signature
    uint32_t challenge;
    std::memcpy(&challenge, response + 1, 4);
    challenge = ntohl(challenge);
    uint32_t signature = secret_num ^ challenge;
    uint32_t signature_net = htonl(signature);

    // Step 5: send 5-byte signature
    sig_msg[0] = response[0]; // group ID
    std::memcpy(sig_msg + 1, &signature_net, 4);
    if (sendto(sock, sig_msg, 5, 0, (sockaddr*)&target, sender_len) < 0) {
        die("sendto failed");
    }

    // Step 6: receive final secret port message
    char secret_port[1024];
    recv_bytes = recvfrom(sock, secret_port, sizeof(secret_port), 0,
                          (sockaddr*)&target, &sender_len);
    if (recv_bytes > 0) {
        std::cout << "Received secret port message:\n"
                  << std::string(secret_port, recv_bytes) << "\n\n";
    } else {
        perror("recvfrom failed or timed out");
    }
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        std::cout << "Usage: ./scanner <IP Address> <port1> <port2> <port3> <port4> <mode>\n";
        std::cout << "Mode 0 = send 123456, Mode 1 = send custom/secret messages\n";
        return EXIT_FAILURE;
    }

    const char* ipaddr = argv[1];
    int port1 = atoi(argv[2]);
    int port2 = atoi(argv[3]);
    int port3 = atoi(argv[4]);
    int port4 = atoi(argv[5]);
    int mode = atoi(argv[6]);

    std::vector<int> ports = {port1, port2, port3, port4};

    // Secret setup
    uint32_t secret_num = 0x00816BF2;
    std::string users = "odinns24,thorvardur23,thora23";
    auto secret_msg = generateSecretMessage(secret_num, users);

    std::cout << "Secret message (hex): ";
    printHex(secret_msg);

    // Socket setup
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) die("socket failed");

    timeval tv{};
    tv.tv_sec = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        die("setsockopt failed");
    }

    sockaddr_in target{};
    target.sin_family = AF_INET;
    if (inet_pton(AF_INET, ipaddr, &target.sin_addr) <= 0) {
        die("inet_pton failed");
    }

    char buffer[1024];
    std::string splitter =
        "\n==========================================================================";

    for (size_t i = 0; i < ports.size(); ++i) {
        target.sin_port = htons(ports[i]);

        if (i == 0) {
            handleSecretPort(sock, target, mode, secret_msg, secret_num);
        } else if (i == 1) {
            const char* signature = sig_msg + 1; // 4-byte signature
            handlePortWithMessage(sock, target, mode, signature, 4);
        } else {
            handlePortPingOrPassword(sock, target, mode);
        }

        socklen_t len = sizeof(target);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);
        if (n > 0) {
            std::cout << splitter << "\n\nPORT " << ports[i]
                      << " says:\n\n" << std::string(buffer, n) << "\n";
        }
    }

    if (close(sock) < 0) die("close failed");

    return EXIT_SUCCESS;
}
