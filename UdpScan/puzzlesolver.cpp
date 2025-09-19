#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sstream>
// Global for signature (kept since later ports depend on it)
char sig_msg[5];


struct pseudo_udp_packet {
    iphdr ip;
    udphdr udp;
    char data[32]; // small payload
};

std::string extract_secret_phrase(const std::string& msg) {
    size_t start = msg.find('"');
    size_t end   = msg.rfind('"');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        return msg.substr(start + 1, end - start - 1);
    }
    return ""; // fallback
}

// Compute UDP checksum over pseudo-header + UDP header + data
uint16_t udp_checksum(iphdr& ip, udphdr& udp, const char* data, size_t len) {
    struct pseudo_header {
        uint32_t src;
        uint32_t dst;
        uint8_t zero;
        uint8_t protocol;
        uint16_t udp_len;
    } ph;

    ph.src = ip.saddr;
    ph.dst = ip.daddr;
    ph.zero = 0;
    ph.protocol = IPPROTO_UDP;
    ph.udp_len = udp.len; // network byte order

    size_t total_len = sizeof(ph) + ntohs(udp.len);
    std::vector<uint8_t> buf(total_len);

    memcpy(buf.data(), &ph, sizeof(ph));
    memcpy(buf.data() + sizeof(ph), &udp, sizeof(udphdr));
    memcpy(buf.data() + sizeof(ph) + sizeof(udphdr), data, len);

    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < total_len; i += 2) {
        sum += (buf[i] << 8) | buf[i + 1];
        if (sum > 0xffff) sum = (sum & 0xffff) + 1;
    }
    if (total_len & 1) { // odd length
        sum += buf[total_len - 1] << 8;
        if (sum > 0xffff) sum = (sum & 0xffff) + 1;
    }
    return htons(~sum & 0xffff);
}

void send_fake_udp(int sock, sockaddr_in& server, uint16_t target_checksum, const char* src_ip) {
    pseudo_udp_packet pkt{};
    const char* payload = "secret";
    size_t payload_len = strlen(payload);

    // IPv4 header
    pkt.ip.version = 4;
    pkt.ip.ihl = 5;
    pkt.ip.tot_len = htons(sizeof(iphdr) + sizeof(udphdr) + payload_len + 2); // +2 for tweak bytes
    pkt.ip.ttl = 64;
    pkt.ip.protocol = IPPROTO_UDP;
    pkt.ip.saddr = inet_addr(src_ip);
    pkt.ip.daddr = server.sin_addr.s_addr;

    // UDP header
    pkt.udp.source = htons(12345);
    pkt.udp.dest = server.sin_port;
    pkt.udp.len = htons(sizeof(udphdr) + payload_len + 2); // +2 for tweak bytes

    // Copy payload
    memcpy(pkt.data, payload, payload_len);

    // Compute current checksum
    uint16_t csum = ntohs(udp_checksum(pkt.ip, pkt.udp, pkt.data, payload_len));

    // Compute tweak bytes to force desired checksum
    uint16_t diff = csum - target_checksum; // 16-bit difference
    pkt.data[payload_len]     = diff >> 8;
    pkt.data[payload_len + 1] = diff & 0xff;
    payload_len += 2;

    // Recompute UDP checksum over modified payload
    pkt.udp.check = udp_checksum(pkt.ip, pkt.udp, pkt.data, payload_len);

    // Send encapsulated packet as UDP payload
    if (sendto(sock, &pkt, sizeof(iphdr) + sizeof(udphdr) + payload_len, 0,
               (sockaddr*)&server, sizeof(server)) < 0) {
        perror("sendto failed");
    } else {
        std::cout << "Sent encapsulated UDP packet with checksum 0x"
                  << std::hex << target_checksum
                  << " and src=" << src_ip << std::dec << "\n";
    }
}

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
void getFinalPort(int sock, sockaddr_in& target, bool mode, const char* combined) {
    const char* msg = mode ? combined : "123456";
    if (sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }
}

void checksum(int sock, sockaddr_in& target, bool mode, const char* msg, size_t msg_len) {
    const char* payload = mode ? msg : "123456";
    size_t len = mode ? msg_len : strlen(payload);

    // send message
    if (sendto(sock, payload, len, 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }

    // receive reply
    char buf[2048];
    socklen_t tlen = sizeof(target);
    int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&target, &tlen);
    if (n < 0) {
        perror("recvfrom failed");
        return;
    }

    std::cout << "Server replied: " << std::string(buf, n) << "\n";

    if (n >= 6) {
        // Last 6 bytes
        const uint8_t* last6 = reinterpret_cast<uint8_t*>(buf + n - 6);

        uint16_t checksum_val;
        memcpy(&checksum_val, last6, 2);
        checksum_val = ntohs(checksum_val); // convert from network to host order

        uint32_t ip_raw;
        memcpy(&ip_raw, last6 + 2, 4);
        in_addr ip_addr{};
        ip_addr.s_addr = ip_raw; // already in network order

        std::cout << "Extracted checksum: 0x" << std::hex << checksum_val << std::dec << "\n";
        std::cout << "Extracted IP: " << inet_ntoa(ip_addr) << "\n";

        // Send back the encapsulated UDP packet using extracted values
        send_fake_udp(sock, target, checksum_val, inet_ntoa(ip_addr));
    } else {
        std::cout << "Message too short to contain secret.\n";
    }
}



void handleEvilPort(int udpSock, sockaddr_in& target, const uint8_t sig[4]) {
    // 1. Get the local IP and port of the normal UDP socket
    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(udpSock, (sockaddr*)&local, &len);

    // 2. Create a minimal header template
    uint8_t pkt[32] = {0};            // IPv4 header + UDP header + 4-byte payload
    pkt[0] = 0x45;                     // IPv4 version + IHL
    uint16_t* flags = (uint16_t*)(pkt + 6);
    *flags = htons(0x8000);            // Set Evil Bit
    pkt[8] = 64;                        // TTL
    pkt[9] = 17;                        // Protocol = UDP
    memcpy(pkt + 12, &local.sin_addr, 4);   // Source IP
    memcpy(pkt + 16, &target.sin_addr, 4);  // Destination IP

    // UDP header
    uint16_t sport = local.sin_port;   // Source port = bound UDP socket
    uint16_t dport = target.sin_port;
    uint16_t ulen = htons(8 + 4);     // UDP length = header + payload
    memcpy(pkt + 20, &sport, 2);
    memcpy(pkt + 22, &dport, 2);
    memcpy(pkt + 24, &ulen, 2);

    // 4-byte signature payload
    memcpy(pkt + 28, sig, 4);

    // 3. Send via raw socket
    int rawSock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    int one = 1;
    setsockopt(rawSock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    sendto(rawSock, pkt, sizeof(pkt), 0, (sockaddr*)&target, sizeof(target));
    close(rawSock);

    // 4. Receive reply on normal UDP socket
    char buf[1024];
    socklen_t slen = sizeof(target);
    int n = recvfrom(udpSock, buf, sizeof(buf), 0, (sockaddr*)&target, &slen);
    if (n > 0) {
        std::cout << "Evil port replied: " << std::string(buf, n) << "\n";
    } else {
        perror("recvfrom failed or timed out");
    }
}



void handleSecretPort(int sock, sockaddr_in& target, bool mode, const std::vector<uint8_t>& secret_msg, uint32_t secret_num) {
    // Step 2: send initial secret message
    const char* msg = mode ? reinterpret_cast<const char*>(secret_msg.data()) : "123456";
    size_t len = mode ? secret_msg.size() : strlen(msg);
    if (sendto(sock, msg, len, 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }
	if (mode == false){return;}
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

// Assumes:
// - sock is your UDP socket (int) with SO_RCVTIMEO already set
// - target is a sockaddr_in with sin_family and sin_addr already set
// - sig_msg is char sig_msg[5] where sig_msg[1..4] are your 4-byte signature
// - secret_ports is a vector<int> containing the ports you want to knock
// - buffer and splitter are available like in your program

void send_knocks(int sock, sockaddr_in target,
                 const std::vector<int>& knock_sequence,
                 const char sig_msg[5], char* buffer,
                 const std::string& splitter,
                 const std::string& phrase) // <- added
{
    const uint8_t* signature = reinterpret_cast<const uint8_t*>(sig_msg + 1);

    for (size_t i = 0; i < knock_sequence.size(); ++i) {
        int port = knock_sequence[i];
        target.sin_port = htons(port);

        std::vector<uint8_t> knock(4 + phrase.size());
        memcpy(knock.data(), signature, 4);
        memcpy(knock.data() + 4, phrase.data(), phrase.size());

        // Send knock
        if (sendto(sock, knock.data(), knock.size(), 0,
                   (struct sockaddr*)&target, sizeof(target)) < 0) {
            perror("sendto failed");
            return;
        }

        std::cout << "Sent knock " << (i + 1) << "/" << knock_sequence.size()
                  << " to port " << port << " (" << knock.size() << " bytes)\n";

        // Wait for reply
        socklen_t slen = sizeof(target);
        int n = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&target, &slen);
        if (n > 0) {
            std::string reply(buffer, n);
            std::cout << splitter << "\n\nPORT " << port << " replied:\n" << reply << "\n";

            if (reply.find("Congratulations") != std::string::npos) {
                std::cout << "🎉 SUCCESS: Final knock accepted!\n";
                break;
            }
        } else {
            perror("recvfrom timed out or failed");
        }
    }
}






int main(int argc, char* argv[]) {
    if (argc != 7 && argc != 9) {
        std::cout << "Usage: ./scanner <IP Address> <port1> <port2> <port3> <port4> <mode> [optional extra args]\n";
        std::cout << "Mode 0 = send 123456, Mode 1 = send custom/secret messages\n";
        return EXIT_FAILURE;
    }
    std::string phrase; // for the secret phrase later
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
            checksum(sock, target, mode, signature, 4);

            // --- Receive the secret phrase from port 4011 ---
            socklen_t len = sizeof(target);
            int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);
            
            if (n > 0) {
                std::string msg(buffer, n);
                std::cout << splitter << "\n\nPORT " << ports[i] << " says:\n" << msg << "\n";

                // Extract phrase dynamically
                phrase = extract_secret_phrase(msg);

                if (phrase.empty()) {
                    std::cerr << "Failed to extract secret phrase!\n";
                    return EXIT_FAILURE;
                } else {
                    std::cout << "Extracted secret phrase: " << phrase << "\n";
                }
            }
        }
        else if (i == 2) {
            uint8_t sig[4] = {0xBA, 0x5C, 0xEB, 0x88}; // your S.E.C.R.E.T7
			handleEvilPort(sock, target, sig);
        }

        socklen_t len = sizeof(target);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);
        if (n > 0) {
            std::cout << splitter << "\n\nPORT " << ports[i]
                      << " says:\n\n" << std::string(buffer, n) << "\n";
        }
    }
    if (argc == 9) {
    // Combine extra args as port list for the last port
    char combined[64];
    snprintf(combined, sizeof(combined), "%s,%s", argv[7], argv[8]);
    target.sin_port = htons(ports[3]); // E.X.P.S.T.N

    getFinalPort(sock, target, mode, combined);

    socklen_t slen = sizeof(target);
    int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &slen);
    if (n > 0) {
        std::string reply(buffer, n);
        std::cout << splitter << "\n\nPORT " << ports[3] << " says:\n" << reply << "\n";

        // Parse knock sequence
        std::vector<int> knock_sequence;
        std::stringstream ss(reply);
        std::string token;
        while (std::getline(ss, token, ',')) {
            knock_sequence.push_back(std::stoi(token));
        }

        // Send knocks in order and wait for response after each
        if (!phrase.empty()) {
            send_knocks(sock, target, knock_sequence, sig_msg, buffer, splitter, phrase);
        } else {
            std::cerr << "Cannot send knocks: secret phrase missing!\n";
            return EXIT_FAILURE;
        }
    }
}



    if (close(sock) < 0) die("close failed");

    return EXIT_SUCCESS;
}
