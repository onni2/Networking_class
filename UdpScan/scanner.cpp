#include <cstring>
#include <iostream>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>


int main(int argc, char* argv[]){
	if (argc != 4){
		std::cout << "Use it by typing ./scanner <IP Address> <low port> <high port>" << std::endl;
		return -1;
	}
	int sock;
	char*ipaddr = argv[1];
	int l_port = atoi(argv[2]); int h_port = atoi(argv[3]);

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
	for (int port = l_port; port <=h_port; port++){
		target.sin_port = htons(port);

		const char* msg = "ping";
		sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));

		socklen_t len = sizeof(target);
		int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);

		if (n>0){
			std::cout << "PORT: " << port << " seems OPEN" << std::endl;
		}
	}
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
