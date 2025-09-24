#include <cstring>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <map>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: ./scanner <IP Address> <low port> <high port>\n";
        return -1;
    }

    const char* ipaddr = argv[1];
    int l_port = atoi(argv[2]);
    int h_port = atoi(argv[3]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Error: could not create socket\n";
        return -1;
    }

    timeval tv{};
    tv.tv_sec = 1;  // for a 1 second tie outs
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    sockaddr_in target{};
    target.sin_family = AF_INET;
    inet_pton(AF_INET, ipaddr, &target.sin_addr);

    char buffer[2048];
    std::map<int, std::string> open_ports;  // store port -> response message

    for (int port = l_port; port <= h_port; port++) {
        target.sin_port = htons(port);

        const char* msg = "ping";  // simple probe
        sendto(sock, msg, strlen(msg), 0, (sockaddr*)&target, sizeof(target));

        socklen_t len = sizeof(target);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&target, &len);

        if (n > 0) {
            std::string reply(buffer, n);
            open_ports[port] = reply;
            std::cout << "PORT: " << port << " seems OPEN\n";
        }
    }

    close(sock);

    if (open_ports.size() < 4) {
        std::cerr << "Did not find all 4 expected ports. Try again.\n";
        return -1;
    }

    std::map<int, int> ordered_ports; // map step -> port number
	for (auto& [port, msg] : open_ports) {
		if (msg.find("Greetings from S.E.C.R.E.T") != std::string::npos)
			ordered_ports[1] = port;
		else if (msg.find("Send me a 4-byte message containing the signature") != std::string::npos)
			ordered_ports[2] = port;
		else if (msg.find("The dark side of network programming") != std::string::npos)
			ordered_ports[3] = port;
		else
			ordered_ports[4] = port;
	}

	// Write to file
	std::ofstream outfile("open_ports.txt");
	if (!outfile.is_open()) {
		std::cerr << "Failed to open open_ports.txt\n";
		return -1;
	}

	for (int i = 1; i <= 4; i++) {
		if (ordered_ports.find(i) != ordered_ports.end())
			outfile << ordered_ports[i] << "\n";
	}

	outfile << "\nNow you can run the code with:\n";
	outfile << "sudo ./puzzlesolver " << ipaddr << " "
			<< ordered_ports[1] << " "
			<< ordered_ports[2] << " "
			<< ordered_ports[3] << " "
			<< ordered_ports[4] << "\n";

	outfile.close();
	std::cout << "Saved 4 open ports and ready-to-run command in open_ports.txt\n";
	return 0;

}
