#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <regex>
#include <netinet/ip_icmp.h>

char sig_msg[5]; // signature storage
constexpr size_t MAX_BUFFER = 1024;
constexpr int TIMEOUT_SEC = 1;

void die(const std::string &msg) {
    perror(msg.c_str());
    exit(EXIT_FAILURE);
}

// find text between first and last quotes
std::string extract_secret_phrase(const std::string& msg) {
    size_t start = msg.find('"');
    size_t end   = msg.rfind('"');
    if (start != std::string::npos && end != std::string::npos && end > start)
        return msg.substr(start + 1, end - start - 1);
    return "";
}

// try various port patterns until one works
int extract_port_number(const std::string& msg) {
    std::regex port_regex(R"(port:\s*(\d+))");
    std::smatch match;
    
    if (std::regex_search(msg, match, port_regex)) {
        return std::stoi(match[1].str());
    }
    
    std::regex alt_regex(R"(port\s+(\d+))");
    if (std::regex_search(msg, match, alt_regex)) {
        return std::stoi(match[1].str());
    }
    
    std::regex secret_regex(R"(secret port[:\s]*\s*(\d+))");
    if (std::regex_search(msg, match, secret_regex)) {
        return std::stoi(match[1].str());
    }
    
    std::regex general_regex(R"(port[^0-9]*(\d+))");
    if (std::regex_search(msg, match, general_regex)) {
        return std::stoi(match[1].str());
    }
    
    return -1;
}

void printHex(const std::vector<uint8_t>& data) {
    for (uint8_t b : data) printf("%02X ", b);
    printf("\n");
}

// build message: 'S' + 32-bit number + user list
std::vector<uint8_t> generateSecretMessage(uint32_t secret_num, const std::string& users) {
    std::vector<uint8_t> msg;
    msg.reserve(1 + 4 + users.size());
    msg.push_back('S'); 
    uint32_t secret_net = htonl(secret_num);
    msg.insert(msg.end(), reinterpret_cast<uint8_t*>(&secret_net), reinterpret_cast<uint8_t*>(&secret_net) + 4);
    msg.insert(msg.end(), users.begin(), users.end());
    return msg;
}

// socket with timeout
int create_udp_socket() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) die("socket failed");

    timeval tv{};
    tv.tv_sec = TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        die("setsockopt failed");

    return sock;
}

sockaddr_in build_target(const char* ipaddr, int port = 0) {
    sockaddr_in target{};
    target.sin_family = AF_INET;
    if (inet_pton(AF_INET, ipaddr, &target.sin_addr) <= 0)
        die("inet_pton failed");
    if (port > 0) target.sin_port = htons(port);
    return target;
}

int recv_from_socket(int sock, sockaddr_in& target, char* buffer, size_t bufsize) {
    socklen_t len = sizeof(target);
    return recvfrom(sock, buffer, bufsize, 0, (sockaddr*)&target, &len);
}

struct pseudo_udp_packet {
    iphdr ip;
    udphdr udp;
    char data[32];
};

// standard UDP checksum with pseudo header
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
    ph.udp_len = udp.len; 

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
    if (total_len & 1) { 
        sum += buf[total_len - 1] << 8;
        if (sum > 0xffff) sum = (sum & 0xffff) + 1;
    }
    return htons(~sum & 0xffff);
}

// craft UDP packet with specific checksum
void send_fake_udp(int sock, sockaddr_in& server, uint16_t target_checksum, const char* src_ip) {
    pseudo_udp_packet pkt{};
    const char* payload = "secret";
    size_t payload_len = strlen(payload);

    // basic IP header
    pkt.ip.version = 4;
    pkt.ip.ihl = 5;
    pkt.ip.tot_len = htons(sizeof(iphdr) + sizeof(udphdr) + payload_len + 2);
    pkt.ip.ttl = 64;
    pkt.ip.protocol = IPPROTO_UDP;
    pkt.ip.saddr = inet_addr(src_ip);
    pkt.ip.daddr = server.sin_addr.s_addr;

    // UDP header
    pkt.udp.source = htons(12345);
    pkt.udp.dest = server.sin_port;
    pkt.udp.len = htons(sizeof(udphdr) + payload_len + 2);

    memcpy(pkt.data, payload, payload_len);

    // calculate current checksum
    uint16_t csum = ntohs(udp_checksum(pkt.ip, pkt.udp, pkt.data, payload_len));

    // add bytes to force desired checksum
    uint16_t diff = csum - target_checksum;
    pkt.data[payload_len]     = diff >> 8;
    pkt.data[payload_len + 1] = diff & 0xff;
    payload_len += 2;

    pkt.udp.check = udp_checksum(pkt.ip, pkt.udp, pkt.data, payload_len);

    if (sendto(sock, &pkt, sizeof(iphdr) + sizeof(udphdr) + payload_len, 0,
               (sockaddr*)&server, sizeof(server)) < 0) {
        perror("sendto failed");
    } else {
        std::cout << "Sent encapsulated UDP packet with checksum 0x"
                  << std::hex << target_checksum
                  << " and src=" << src_ip << std::dec << "\n";
    }
}

void getFinalPort(int sock, sockaddr_in& target, const char* combined) {
    if (sendto(sock, combined, strlen(combined), 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }
}

// send signature, get checksum and IP from response
void checksum(int sock, sockaddr_in& target, const char* msg, size_t msg_len) {
    if (sendto(sock, msg, msg_len, 0, (sockaddr*)&target, sizeof(target)) < 0) {
        die("sendto failed");
    }

    char buf[2048];
    socklen_t tlen = sizeof(target);
    int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&target, &tlen);
    if (n < 0) {
        perror("recvfrom failed");
        return;
    }

    std::cout << "Server replied: " << std::string(buf, n) << "\n";

    if (n >= 6) {
        // extract last 6 bytes: 2-byte checksum + 4-byte IP
        const uint8_t* last6 = reinterpret_cast<uint8_t*>(buf + n - 6);

        uint16_t checksum_val;
        memcpy(&checksum_val, last6, 2);
        checksum_val = ntohs(checksum_val); 

        uint32_t ip_raw;
        memcpy(&ip_raw, last6 + 2, 4);
        in_addr ip_addr{};
        ip_addr.s_addr = ip_raw; 

        std::cout << "Extracted checksum: 0x" << std::hex << checksum_val << std::dec << "\n";
        std::cout << "Extracted IP: " << inet_ntoa(ip_addr) << "\n";

        send_fake_udp(sock, target, checksum_val, inet_ntoa(ip_addr));
    } else {
        std::cout << "Message too short to contain secret.\n";
    }
}

// send raw packet with evil bit set
int handleEvilPort(int udpSock, sockaddr_in& target, const uint8_t sig[4]) {
    sockaddr_in local{};
    socklen_t len = sizeof(local);
    getsockname(udpSock, (sockaddr*)&local, &len);

    // build minimal IP+UDP packet
    uint8_t pkt[32] = {0};
    pkt[0] = 0x45;                     // IPv4
    uint16_t* flags = (uint16_t*)(pkt + 6);
    *flags = htons(0x8000);            // evil bit
    pkt[8] = 64;                       // TTL
    pkt[9] = 17;                       // UDP
    memcpy(pkt + 12, &local.sin_addr, 4);
    memcpy(pkt + 16, &target.sin_addr, 4);

    uint16_t sport = local.sin_port;
    uint16_t dport = target.sin_port;
    uint16_t ulen = htons(8 + 4);
    memcpy(pkt + 20, &sport, 2);
    memcpy(pkt + 22, &dport, 2);
    memcpy(pkt + 24, &ulen, 2);
    memcpy(pkt + 28, sig, 4);

    // send via raw socket
    int rawSock = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
    int one = 1;
    setsockopt(rawSock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));
    sendto(rawSock, pkt, sizeof(pkt), 0, (sockaddr*)&target, sizeof(target));
    close(rawSock);

    // get reply on normal socket
    char buf[1024];
    socklen_t slen = sizeof(target);
    int n = recvfrom(udpSock, buf, sizeof(buf), 0, (sockaddr*)&target, &slen);
    if (n > 0) {
        std::string msg(buf, n);
        std::cout << "Evil port replied: " << msg << "\n";
        return extract_port_number(msg);
    } else {
        perror("recvfrom failed or timed out");
        return -1;
    }
}

// challenge-response exchange
int handleSecretPort(int sock, sockaddr_in& target, const std::vector<uint8_t>& secret_msg, uint32_t secret_num) {
    if (sendto(sock, secret_msg.data(), secret_msg.size(), 0, (sockaddr*)&target, sizeof(target)) < 0) die("sendto failed");

    char response[5];
    socklen_t slen = sizeof(target);
    ssize_t n = recvfrom(sock, response, sizeof(response), 0, (sockaddr*)&target, &slen);
    if (n != 5) { std::cerr << "Invalid challenge length\n"; return -1; }

    uint32_t challenge;
    std::memcpy(&challenge, response + 1, 4);
    challenge = ntohl(challenge);

    // XOR challenge with secret
    uint32_t signature = secret_num ^ challenge;
    uint32_t signature_net = htonl(signature);

    sig_msg[0] = response[0];
    std::memcpy(sig_msg + 1, &signature_net, 4);

    if (sendto(sock, sig_msg, 5, 0, (sockaddr*)&target, slen) < 0) die("sendto failed");

    char secret_port[MAX_BUFFER];
    n = recvfrom(sock, secret_port, sizeof(secret_port), 0, (sockaddr*)&target, &slen);
    if (n > 0) {
        std::string msg(secret_port, n);
        std::cout << "Received secret port message:\n" << msg << "\n";
        return extract_port_number(msg);
    } else {
        perror("recvfrom timed out");
        return -1;
    }
}

// standard ICMP checksum
unsigned short icmp_checksum(void *b, int len) {
    unsigned short *buf = (unsigned short*)b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) {
        sum += *buf++;
    }

    if (len == 1) {
        sum += *(unsigned char*)buf << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    result = ~sum;
    return result;
}

// send ICMP ping with group identifier
void send_icmp_bonus(const std::string& target_ip) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        perror("ICMP socket failed (need root privileges)");
        return;
    }

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    inet_pton(AF_INET, target_ip.c_str(), &target.sin_addr);

    const char* group_data = "$group_7$";
    size_t data_len = strlen(group_data);

    std::vector<char> packet(sizeof(struct icmphdr) + data_len);
    struct icmphdr* icmp = reinterpret_cast<struct icmphdr*>(packet.data());

    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.id = static_cast<uint16_t>(getpid() & 0xFFFF);
    icmp->un.echo.sequence = 1;
    icmp->checksum = 0;

    memcpy(packet.data() + sizeof(struct icmphdr), group_data, data_len);

    icmp->checksum = icmp_checksum(packet.data(), packet.size());

    if (sendto(sock, packet.data(), packet.size(), 0,
               (struct sockaddr*)&target, sizeof(target)) < 0) {
        perror("ICMP sendto failed");
        close(sock);
        return;
    } else {
        std::cout << "*** Sent ICMP packet with '" << group_data
                  << "' to " << target_ip << " ***\n";
    }
}

// knock on ports in sequence
void send_knocks(int sock, sockaddr_in target,
                 const std::vector<int>& knock_sequence,
                 const char sig_msg[5], char* buffer,
                 const std::string& splitter,
                 const std::string& phrase)
{
    const uint8_t* signature = reinterpret_cast<const uint8_t*>(sig_msg + 1);
    std::string target_ip = inet_ntoa(target.sin_addr);

    for (size_t i = 0; i < knock_sequence.size(); ++i) {
        int port = knock_sequence[i];
        target.sin_port = htons(port);

        // signature + phrase
        std::vector<uint8_t> knock(4 + phrase.size());
        memcpy(knock.data(), signature, 4);
        memcpy(knock.data() + 4, phrase.data(), phrase.size());

        if (sendto(sock, knock.data(), knock.size(), 0,
                   (struct sockaddr*)&target, sizeof(target)) < 0) {
            perror("sendto failed");
            return;
        }

        std::cout << "Sent knock " << (i + 1) << "/" << knock_sequence.size()
                  << " to port " << port << " (" << knock.size() << " bytes)\n";

        socklen_t slen = sizeof(target);
        int n = recvfrom(sock, buffer, 1024, 0, (struct sockaddr*)&target, &slen);
        if (n > 0) {
            std::string reply(buffer, n);
            std::cout << splitter << "PORT " << port << " replied:\n" << reply << "\n";

            if (reply.find("Congratulations") != std::string::npos) {
                break;
            }
        } else {
            perror("recvfrom timed out or failed");
        }
    }
    
    send_icmp_bonus(target_ip);
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cout << "Usage: ./scanner <IP> <port1> <port2> <port3> <port4>\n";
        std::cout << "Automated challenge solver for UDP port sequences\n";
        return EXIT_FAILURE;
    }

    const char* ipaddr = argv[1];
    std::vector<int> ports = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5])};
    
    std::string phrase;
    int secret_port1 = -1;
    int secret_port2 = -1;

    uint32_t secret_num = 0x00816BF2;
    std::string users = "odinns24,thorvardur23,thora23";
    auto secret_msg = generateSecretMessage(secret_num, users);

    std::cout << "Secret message (hex): ";
    printHex(secret_msg);

    int sock = create_udp_socket();
    sockaddr_in target = build_target(ipaddr);

    char buffer[1024];
    std::string splitter = "\n==============================================================\n";

    // hit each port in order
    for (size_t i = 0; i < ports.size(); ++i) {
        target.sin_port = htons(ports[i]);

        if (i == 0) {
            secret_port1 = handleSecretPort(sock, target, secret_msg, secret_num);
            if (secret_port1 != -1) {
                std::cout << "*** EXTRACTED SECRET PORT 1: " << secret_port1 << " ***\n";
            }
        }
        else if (i == 1) {
            const char* signature = sig_msg + 1;
            checksum(sock, target, signature, 4);

            int n = recv_from_socket(sock, target, buffer, sizeof(buffer));
            if (n > 0) {
                std::string msg(buffer, n);
                std::cout << splitter << "PORT " << ports[i] << " says:\n" << msg << "\n";

                phrase = extract_secret_phrase(msg);
                if (!phrase.empty()) {
                    std::cout << "*** EXTRACTED SECRET PHRASE: \"" << phrase << "\" ***\n";
                } else {
                    std::cerr << "Failed to extract secret phrase!\n";
                }
            }
        }
        else if (i == 2) {
            uint8_t sig[4] = {0xBA, 0x5C, 0xEB, 0x88};
            secret_port2 = handleEvilPort(sock, target, sig);
            if (secret_port2 != -1) {
                std::cout << "*** EXTRACTED SECRET PORT 2: " << secret_port2 << " ***\n";
            }
        }
        else {
            int n = recv_from_socket(sock, target, buffer, sizeof(buffer));
            if (n > 0) {
                std::cout << splitter << "PORT " << ports[i] << " says:\n" << std::string(buffer, n) << "\n";
            }
        }
    }

    // proceed if we got everything
    if (secret_port1 != -1 && secret_port2 != -1 && !phrase.empty()) {
        std::cout << "\n*** PROCEEDING WITH FINAL CHALLENGE ***\n";
        std::cout << "Using secret ports: " << secret_port1 << ", " << secret_port2 << "\n";
        std::cout << "Using secret phrase: \"" << phrase << "\"\n\n";
        
        char combined[64];
        snprintf(combined, sizeof(combined), "%d,%d", secret_port1, secret_port2);
        
        target.sin_port = htons(ports[3]);
        getFinalPort(sock, target, combined);

        int n = recv_from_socket(sock, target, buffer, sizeof(buffer));
        if (n > 0) {
            std::string reply(buffer, n);
            std::cout << splitter << "PORT " << ports[3] << " says:\n" << reply << "\n";

            // parse comma-separated knock sequence
            std::vector<int> knock_sequence;
            std::stringstream ss(reply);
            std::string token;
            while (std::getline(ss, token, ',')) {
                try {
                    knock_sequence.push_back(std::stoi(token));
                } catch (const std::exception& e) {
                    continue;
                }
            }

            if (!knock_sequence.empty()) {
                std::cout << "\n*** STARTING PORT KNOCKING SEQUENCE ***\n";
                send_knocks(sock, target, knock_sequence, sig_msg, buffer, splitter, phrase);
            } else {
                std::cerr << "No valid knock sequence found in response!\n";
            }
        } else {
            std::cerr << "No response from final port!\n";
        }
    } else {
        std::cout << "\n*** MISSING REQUIRED INFORMATION ***\n";
        std::cout << "Secret port 1: " << (secret_port1 == -1 ? "NOT FOUND" : std::to_string(secret_port1)) << "\n";
        std::cout << "Secret port 2: " << (secret_port2 == -1 ? "NOT FOUND" : std::to_string(secret_port2)) << "\n";
        std::cout << "Secret phrase: " << (phrase.empty() ? "NOT FOUND" : "\"" + phrase + "\"") << "\n";
        std::cout << "Cannot proceed with final challenge.\n";
    }

    close(sock);
    return EXIT_SUCCESS;
}