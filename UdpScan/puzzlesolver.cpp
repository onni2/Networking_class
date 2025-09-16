#include <cstring>
#include <iostream>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

// for the S.E.C.R.E.T port
char* generateSecretMessage(uint32_t secret_num, const char* users, size_t& out_len) {
    size_t users_len = std::strlen(users);
    out_len = 1 + 4 + users_len; // 1 byte 'S' + 4 bytes secret + usernames

    char* msg = new char[out_len];

    // First byte = 'S'
    msg[0] = 'S';

    // Next 4 bytes = secret in network byte order
    uint32_t secret_net = htonl(secret_num);
    std::memcpy(msg + 1, &secret_net, 4);

    // Remaining bytes = usernames
    std::memcpy(msg + 5, users, users_len);

    return msg;
}


// using for readability
std::string splitter = "\n==========================================================================";

// Functions for each port
void handlePort1(int sock, sockaddr_in& target, bool mode) {
    const char* msg = mode ? "ping1" : "123456";
    sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));
}

void handlePort2(int sock, sockaddr_in& target, bool mode) {
    const char* msg = mode ? "ping2" : "123456";
    sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));
}

void handlePort3(int sock, sockaddr_in& target, bool mode) {
    const char* msg = mode ? "ping3" : "123456";
    sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));
}

void handleSecretPort(int sock, sockaddr_in& target, bool mode, char* secret_msg, size_t msg_len) {
    const char* msg = mode ? secret_msg : "123456";
    size_t len = mode ? msg_len : strlen(msg);
    sendto(sock, msg, len, 0, (sockaddr*)&target, sizeof(target));
}

int main(int argc, char* argv[]){

	uint32_t secret_num = 0x00816BF2;
    const char* users = "odinns24,thorvardur23,thora23";
    size_t msg_len;

	if (argc != 7){
		std::cout << "Use it by typing ./scanner <IP Address> <port1> <port2> <port3> <port4> <mode>" << std::endl;
		std::cout << "Mode 0 = send 123456, Mode 1 = send custom/secret messages" << std::endl;
		return -1;
	}

	char* secret_msg = generateSecretMessage(secret_num, users, msg_len);
	for (size_t i = 0; i < msg_len; i++) {
		printf("%02X ", static_cast<unsigned char>(secret_msg[i]));
	}
	int sock;
	char*ipaddr = argv[1];
	int port1 = atoi(argv[2]); int port2 = atoi(argv[3]);
	int port3 = atoi(argv[4]); int port4 = atoi(argv[5]);
	std::vector<int> o_ports = {port1,port2,port3,port4};

	int mode = atoi(argv[6]);

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0))< 0){
		std::cerr << "Error could not create socket" << std::endl;
		return -1;
	}

	//set a interval timer for 1 sec
	timeval tv{};
	tv.tv_sec = 1;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	sockaddr_in target{};
	target.sin_family = AF_INET;
	inet_pton(AF_INET, ipaddr, &target.sin_addr);

	char buffer[1024];
	for (size_t i = 0; i < o_ports.size(); ++i) {
        target.sin_port = htons(o_ports[i]);

        // Call the correct function depending on index
        if (i == 0) handlePort1(sock, target, mode);
        else if (i == 1) handlePort2(sock, target, mode);
        else if (i == 2) handlePort3(sock, target, mode);
        else if (i == 3) handleSecretPort(sock, target, mode, secret_msg, msg_len);
		socklen_t len = sizeof(target);
		int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);
		
		if (n>0){
			std::cout << splitter << "\n\nPORT " << o_ports[i] << " says:\n\n " << buffer << std::endl;
		}
	}
	delete[] secret_msg;
	if (close(sock) < 0){
		std::cerr << "Error closing the socket" << std::endl;
	}
}

// i need to use
// socket
// htons
// inet_pton
// sendto for just sending without connecting
// recvfrom same just send and hope for the best
// setsockopt

