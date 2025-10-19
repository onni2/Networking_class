#include "protocol.h"
#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <map>
#include <queue>
#include <vector>
#include <set>
#include <pthread.h>
#include <algorithm>
#include <signal.h>

const std::string MY_GROUP_ID = "A5_1";
const std::string TSAM_SERVER_IP = "130.208.246.98";
const std::vector<int> INSTRUCTOR_PORTS = {5001, 5002, 5003};
const int MAX_HOPS = 48;

struct Message {
    std::string content, fromGroup, toGroup, hops;
    time_t timestamp;
    int hopCount;
};

struct ServerInfo {
    int socket;
    std::string groupId, ip;
    int port;
    time_t lastSeen, connectedSince;
    bool isOutgoing, isInstructor;
};

struct KnownServer {
    std::string groupId, ip;
    int port;
    time_t lastHeard;
};

void* peerCommunicationThread(void* arg);

std::map<int, ServerInfo> connectedServers;
std::vector<KnownServer> knownServers;
std::set<std::string> connectedGroupIds;
std::map<std::string, std::queue<Message>> messageQueue;
std::ofstream logFile;
int listenPort;
std::string myIpAddress;
pthread_mutex_t serverMutex = PTHREAD_MUTEX_INITIALIZER;
bool isScanning = false;
int messagesReceived = 0, messagesSent = 0, messagesForwarded = 0, loopsDetected = 0;

std::map<std::string, time_t> lastHeloAttempt;

void logMessage(const std::string &msg) {
    std::string entry = "[" + getTimestamp() + "] " + msg;
    std::cout << entry << std::endl;
    if (logFile.is_open()) { logFile << entry << std::endl; logFile.flush(); }
}

std::string getLocalIPAddress() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return "127.0.0.1";
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);
    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) { close(sock); return "127.0.0.1"; }
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &len) < 0) { close(sock); return "127.0.0.1"; }
    close(sock);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local.sin_addr, ip, INET_ADDRSTRLEN);
    return std::string(ip);
}

int open_socket(int portno) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    int set = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(portno);
    return bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ? -1 : sock;
}

void triggerScan();

void connectToServer(const std::string &ip, int port) {
    pthread_mutex_lock(&serverMutex);
    for (const auto &p : connectedServers)
        if (p.second.ip == ip && p.second.port == port) { pthread_mutex_unlock(&serverMutex); return; }
    if (connectedServers.size() >= 8) { pthread_mutex_unlock(&serverMutex); return; }
    pthread_mutex_unlock(&serverMutex);
    
    logMessage("Connecting to " + ip + ":" + std::to_string(port));
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0 || 
        connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) { 
        logMessage("Failed to connect to " + ip + ":" + std::to_string(port));
        close(sock); 
        return; 
    }
    
    if (!sendCommand(sock, buildHELO(MY_GROUP_ID))) { close(sock); return; }
    
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = {10, 0};
    if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0) { close(sock); return; }
    
    std::string response;
    if (!receiveCommand(sock, response)) { 
        logMessage("No response from " + ip + ":" + std::to_string(port));
        close(sock); 
        return; 
    }
    logMessage("Got response: " + response.substr(0, 50));
    std::vector<std::string> tokens = parseCommand(response);
    if (tokens.empty()) { close(sock); return; }
    
    std::string responderId = "";
    
    if (tokens[0] == "HELO" && tokens.size() >= 2) {
        responderId = tokens[1];
        pthread_mutex_lock(&serverMutex);
        if (connectedGroupIds.find(responderId) != connectedGroupIds.end()) {
            pthread_mutex_unlock(&serverMutex);
            close(sock);
            return;
        }
        pthread_mutex_unlock(&serverMutex);
        
        std::vector<std::tuple<std::string, std::string, int>> servers;
        servers.push_back(std::make_tuple(MY_GROUP_ID, myIpAddress, listenPort));
        pthread_mutex_lock(&serverMutex);
        for (const auto &p : connectedServers)
            if (!p.second.groupId.empty() && p.second.port > 0)  // Only share if we know their port
                servers.push_back(std::make_tuple(p.second.groupId, p.second.ip, p.second.port));
        pthread_mutex_unlock(&serverMutex);
        
        if (!sendCommand(sock, buildSERVERS(servers))) { close(sock); return; }
        
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 10;
        if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0 || !receiveCommand(sock, response)) {
            close(sock);
            return;
        }
        tokens = parseCommand(response);
        if (tokens.empty() || tokens[0] != "SERVERS") { close(sock); return; }
    }
    
    if (tokens[0] == "SERVERS" && tokens.size() > 1) {
        pthread_mutex_lock(&serverMutex);
        std::string serverList;
        for (size_t i = 1; i < tokens.size(); i++) {
            serverList += tokens[i];
            if (i < tokens.size() - 1) serverList += ",";
        }
        std::vector<std::string> entries = splitServers(serverList);
        if (responderId.empty() && !entries.empty()) {
            std::vector<std::string> parts = parseCommand(entries[0]);
            if (parts.size() >= 3) responderId = parts[0];
        }
        
        // DON'T CONNECT TO SERVERS WITH OUR OWN GROUP ID!
        if (responderId == MY_GROUP_ID) {
            pthread_mutex_unlock(&serverMutex);
            logMessage("Rejecting " + ip + ":" + std::to_string(port) + " - they claim to be " + responderId + " (our ID!)");
            close(sock);
            return;
        }
        
        for (const auto &entry : entries) {
            std::vector<std::string> parts = parseCommand(entry);
            if (parts.size() >= 3) {
                KnownServer ks = {parts[0], parts[1], std::stoi(parts[2]), time(nullptr)};
                if (ks.groupId != MY_GROUP_ID && connectedGroupIds.find(ks.groupId) == connectedGroupIds.end()) {
                    bool found = false;
                    for (auto &e : knownServers) {
                        if (e.groupId == ks.groupId) { e = ks; found = true; break; }
                    }
                    if (!found) knownServers.push_back(ks);
                }
            }
        }
        
        if (connectedGroupIds.find(responderId) != connectedGroupIds.end()) {
            pthread_mutex_unlock(&serverMutex);
            close(sock);
            return;
        }
        
        bool isInstr = false;
        for (int p : INSTRUCTOR_PORTS) if (port == p && ip == TSAM_SERVER_IP) { isInstr = true; break; }
        
        connectedServers[sock] = {sock, responderId, ip, port, time(nullptr), time(nullptr), true, isInstr};
        connectedGroupIds.insert(responderId);
        pthread_mutex_unlock(&serverMutex);
        
        logMessage("Connected to " + responderId + " [" + std::to_string(connectedServers.size()) + " total]");
        
        int* ptr = new int(sock);
        pthread_t tid;
        if (pthread_create(&tid, nullptr, peerCommunicationThread, ptr) == 0) {
            pthread_detach(tid);
        } else {
            delete ptr;
            pthread_mutex_lock(&serverMutex);
            connectedServers.erase(sock);
            connectedGroupIds.erase(responderId);
            pthread_mutex_unlock(&serverMutex);
            close(sock);
        }
    } else close(sock);
}

void tryKnownServers() {
    pthread_mutex_lock(&serverMutex);
    std::vector<KnownServer> cands = knownServers;
    pthread_mutex_unlock(&serverMutex);
    
    for (const auto &s : cands) {
        pthread_mutex_lock(&serverMutex);
        bool connected = connectedGroupIds.find(s.groupId) != connectedGroupIds.end();
        bool full = connectedServers.size() >= 8;
        pthread_mutex_unlock(&serverMutex);
        if (full) break;
        if (!connected) { connectToServer(s.ip, s.port); sleep(2); }
    }
}

void triggerScan() {
    pthread_mutex_lock(&serverMutex);
    if (isScanning) { pthread_mutex_unlock(&serverMutex); return; }
    isScanning = true;
    pthread_mutex_unlock(&serverMutex);
    
    tryKnownServers();
    
    pthread_mutex_lock(&serverMutex);
    int conn = connectedServers.size();
    pthread_mutex_unlock(&serverMutex);
    
    if (conn >= 3) {
        pthread_mutex_lock(&serverMutex);
        isScanning = false;
        pthread_mutex_unlock(&serverMutex);
        return;
    }
    
    std::vector<int> ports = scanForServers(TSAM_SERVER_IP, 4000, 4200, listenPort);
    for (int p : ports) {
        pthread_mutex_lock(&serverMutex);
        if (connectedServers.size() >= 8) { pthread_mutex_unlock(&serverMutex); break; }
        pthread_mutex_unlock(&serverMutex);
        connectToServer(TSAM_SERVER_IP, p);
        sleep(2);
    }
    
    pthread_mutex_lock(&serverMutex);
    conn = connectedServers.size();
    pthread_mutex_unlock(&serverMutex);
    
    if (conn < 3) {
        for (int p : INSTRUCTOR_PORTS) {
            if (isPortOpen(TSAM_SERVER_IP, p, 500)) {
                connectToServer(TSAM_SERVER_IP, p);
                pthread_mutex_lock(&serverMutex);
                if (connectedServers.size() >= 3) { pthread_mutex_unlock(&serverMutex); break; }
                pthread_mutex_unlock(&serverMutex);
                sleep(2);
            }
        }
    }
    
    pthread_mutex_lock(&serverMutex);
    isScanning = false;
    pthread_mutex_unlock(&serverMutex);
}

void *healthMonitorThread(void *arg) {
    (void)arg;
    time_t lastKA = time(nullptr), lastGM = time(nullptr), lastSR = time(nullptr), lastCC = time(nullptr);
    
    while (true) {
        sleep(30);
        time_t now = time(nullptr);
        
        if (now - lastKA >= 60) {
            pthread_mutex_lock(&serverMutex);
            int sent = 0;
            std::vector<int> failed;
            for (const auto &p : connectedServers) {
                if (!p.second.groupId.empty()) {
                    if (sendCommand(p.first, buildKEEPALIVE(messageQueue[p.second.groupId].size()))) {
                        sent++;
                    } else {
                        failed.push_back(p.first);
                    }
                }
            }
            for (int sock : failed) {
                logMessage("Removing dead connection: " + connectedServers[sock].groupId);
                std::string gid = connectedServers[sock].groupId;
                connectedGroupIds.erase(gid);
                lastHeloAttempt.erase(gid);
                close(sock);
                connectedServers.erase(sock);
            }
            pthread_mutex_unlock(&serverMutex);
            if (sent > 0) logMessage("Sent KEEPALIVE to " + std::to_string(sent) + " peers");
            lastKA = now;
        }
        
        if (now - lastGM >= 90) {
            pthread_mutex_lock(&serverMutex);
            int sent = 0;
            std::vector<int> failed;
            for (const auto &p : connectedServers) {
                if (!p.second.groupId.empty()) {
                    if (sendCommand(p.first, buildGETMSGS(MY_GROUP_ID))) {
                        sent++;
                    } else {
                        failed.push_back(p.first);
                    }
                }
            }
            for (int sock : failed) {
                logMessage("Removing dead connection: " + connectedServers[sock].groupId);
                std::string gid = connectedServers[sock].groupId;
                connectedGroupIds.erase(gid);
                lastHeloAttempt.erase(gid);
                close(sock);
                connectedServers.erase(sock);
            }
            pthread_mutex_unlock(&serverMutex);
            if (sent > 0) logMessage("Sent GETMSGS to " + std::to_string(sent) + " peers");
            lastGM = now;
        }
        
        if (now - lastSR >= 180) {
            pthread_mutex_lock(&serverMutex);
            int sent = 0;
            std::vector<int> failed;
            for (const auto &p : connectedServers) {
                if (!p.second.groupId.empty()) {
                    if (sendCommand(p.first, buildSTATUSREQ())) {
                        sent++;
                    } else {
                        failed.push_back(p.first);
                    }
                }
            }
            for (int sock : failed) {
                logMessage("Removing dead connection: " + connectedServers[sock].groupId);
                std::string gid = connectedServers[sock].groupId;
                connectedGroupIds.erase(gid);
                lastHeloAttempt.erase(gid);
                close(sock);
                connectedServers.erase(sock);
            }
            pthread_mutex_unlock(&serverMutex);
            if (sent > 0) logMessage("Sent STATUSREQ to " + std::to_string(sent) + " peers");
            lastSR = now;
        }
        
        pthread_mutex_lock(&serverMutex);
        std::vector<int> toRemove;
        for (auto &p : connectedServers)
            if (now - p.second.lastSeen > 300)
                toRemove.push_back(p.first);
        for (int s : toRemove) {
            std::string gid = connectedServers[s].groupId;
            connectedGroupIds.erase(gid);
            lastHeloAttempt.erase(gid);
            close(s);
            connectedServers.erase(s);
        }
        
        int conn = connectedServers.size(), stuConn = 0, insConn = 0;
        for (const auto &p : connectedServers)
            p.second.isInstructor ? insConn++ : stuConn++;
        pthread_mutex_unlock(&serverMutex);
        
        logMessage("Status: " + std::to_string(conn) + " connections (" + std::to_string(stuConn) + 
                   " students, " + std::to_string(insConn) + " instructors) | RX:" + std::to_string(messagesReceived) +
                   " TX:" + std::to_string(messagesSent) + " FWD:" + std::to_string(messagesForwarded));
        
        if (now - lastCC >= 120) {
            if (conn < 3) triggerScan();
            else if (stuConn < 3 && conn < 8) tryKnownServers();
            lastCC = now;
        }
    }
    return NULL;
}

void handleServerCommand(int sock, const std::string &cmd) {
    std::string main, hops;
    parseSENDMSGWithHops(cmd, main, hops);
    std::vector<std::string> tokens = parseCommand(main);
    if (tokens.empty()) return;
    
    pthread_mutex_lock(&serverMutex);
    if (connectedServers.find(sock) != connectedServers.end())
        connectedServers[sock].lastSeen = time(nullptr);
    pthread_mutex_unlock(&serverMutex);
    
    if (tokens[0] == "HELO" && tokens.size() >= 2) {
        std::string from = tokens[1];
        logMessage("HELO from " + from);
        pthread_mutex_lock(&serverMutex);
        if (connectedGroupIds.find(from) != connectedGroupIds.end()) {
            pthread_mutex_unlock(&serverMutex);
            return;
        }
        if (connectedServers.find(sock) != connectedServers.end()) {
            connectedServers[sock].groupId = from;
            connectedGroupIds.insert(from);
            logMessage("Accepted HELO from " + from + " [" + std::to_string(connectedGroupIds.size()) + " peers]");
        } else { pthread_mutex_unlock(&serverMutex); return; }
        pthread_mutex_unlock(&serverMutex);
        
        std::vector<std::tuple<std::string, std::string, int>> servers;
        servers.push_back(std::make_tuple(MY_GROUP_ID, myIpAddress, listenPort));
        pthread_mutex_lock(&serverMutex);
        for (const auto &p : connectedServers)
            if (p.first != sock && !p.second.groupId.empty() && p.second.port > 0)  // Only share if we know their port
                servers.push_back(std::make_tuple(p.second.groupId, p.second.ip, p.second.port));
        pthread_mutex_unlock(&serverMutex);
        sendCommand(sock, buildSERVERS(servers));
    }
    else if (tokens[0] == "KEEPALIVE" && tokens.size() >= 2) {
        int cnt = std::stoi(tokens[1]);
        pthread_mutex_lock(&serverMutex);
        std::string from = connectedServers.find(sock) != connectedServers.end() ? connectedServers[sock].groupId : "?";
        pthread_mutex_unlock(&serverMutex);
        logMessage("KEEPALIVE from " + from + " (" + std::to_string(cnt) + " msgs)");
        if (cnt > 0) sendCommand(sock, buildGETMSGS(MY_GROUP_ID));
    }
    else if (tokens[0] == "GETMSGS" && tokens.size() >= 2) {
        std::string forGroup = tokens[1];
        logMessage("GETMSGS request for " + forGroup);
        pthread_mutex_lock(&serverMutex);
        bool has = !messageQueue[forGroup].empty();
        pthread_mutex_unlock(&serverMutex);
        if (has) {
            pthread_mutex_lock(&serverMutex);
            Message msg = messageQueue[forGroup].front();
            messageQueue[forGroup].pop();
            pthread_mutex_unlock(&serverMutex);
            sendCommand(sock, buildSENDMSG(forGroup, msg.fromGroup, msg.content, msg.hops));
        } else sendCommand(sock, "NO_MESSAGES");
    }
    else if (tokens[0] == "SENDMSG" && tokens.size() >= 4) {
        std::string to = tokens[1], from = tokens[2], content;
        for (size_t i = 3; i < tokens.size(); i++) {
            content += tokens[i];
            if (i < tokens.size() - 1) content += ",";
        }
        
        if (isInHops(hops, MY_GROUP_ID)) { 
            loopsDetected++; 
            logMessage("Loop detected, dropping msg (loops:" + std::to_string(loopsDetected) + ")");
            return; 
        }
        
        int hopCnt = 0;
        if (!hops.empty()) {
            hopCnt = 1;
            for (char c : hops) if (c == ',') hopCnt++;
        }
        
        if (hopCnt >= MAX_HOPS) {
            Message msg = {content, from, to, "", time(nullptr), 0};
            pthread_mutex_lock(&serverMutex);
            messageQueue[to].push(msg);
            pthread_mutex_unlock(&serverMutex);
            return;
        }
        
        if (to == MY_GROUP_ID) {
            Message msg = {content, from, to, hops, time(nullptr), hopCnt};
            pthread_mutex_lock(&serverMutex);
            messageQueue[to].push(msg);
            pthread_mutex_unlock(&serverMutex);
            messagesReceived++;
            logMessage("Received msg from " + from + " (hops:" + std::to_string(hopCnt) + ")");
        } else {
            bool fwd = false;
            pthread_mutex_lock(&serverMutex);
            for (const auto &p : connectedServers) {
                if (p.second.groupId == to) {
                    pthread_mutex_unlock(&serverMutex);
                    std::string newHops = hops.empty() ? from : hops + "," + MY_GROUP_ID;
                    if (sendCommand(p.first, buildSENDMSG(to, from, content, newHops))) {
                        messagesForwarded++;
                        logMessage("Forwarded " + from + "->" + to + " [" + std::to_string(messagesForwarded) + "]");
                        fwd = true;
                    }
                    pthread_mutex_lock(&serverMutex);
                    break;
                }
            }
            pthread_mutex_unlock(&serverMutex);
            
            if (!fwd) {
                Message msg = {content, from, to, hops.empty() ? from : hops + "," + MY_GROUP_ID, time(nullptr), hopCnt + 1};
                pthread_mutex_lock(&serverMutex);
                messageQueue[to].push(msg);
                for (const auto &p : connectedServers) {
                    if (!p.second.groupId.empty() && !isInHops(msg.hops, p.second.groupId))
                        sendCommand(p.first, buildSENDMSG(to, from, content, msg.hops));
                }
                pthread_mutex_unlock(&serverMutex);
            }
        }
    }
    else if (tokens[0] == "STATUSREQ") {
        logMessage("STATUSREQ received");
        pthread_mutex_lock(&serverMutex);
        std::vector<std::pair<std::string, int>> status;
        for (const auto &p : messageQueue)
            if (!p.second.empty())
                status.push_back({p.first, p.second.size()});
        pthread_mutex_unlock(&serverMutex);
        sendCommand(sock, buildSTATUSRESP(status));
    }
    else if (tokens[0] == "STATUSRESP") {
        logMessage("STATUSRESP: " + main);
    }
    else if (tokens[0] == "NO_MESSAGES") {
        logMessage("NO_MESSAGES from peer");
    }
}

void handleClientCommand(int sock, const std::string &cmd) {
    std::vector<std::string> tokens = parseCommand(cmd);
    if (tokens.empty()) return;
    
    if (tokens[0] == "SENDMSG" && tokens.size() >= 3) {
        std::string to = tokens[1], msg;
        for (size_t i = 2; i < tokens.size(); i++) {
            msg += tokens[i];
            if (i < tokens.size() - 1) msg += ",";
        }
        
        bool fwd = false;
        pthread_mutex_lock(&serverMutex);
        for (const auto &p : connectedServers) {
            if (p.second.groupId == to) {
                pthread_mutex_unlock(&serverMutex);
                if (sendCommand(p.first, buildSENDMSG(to, MY_GROUP_ID, msg, MY_GROUP_ID))) {
                    messagesSent++;
                    fwd = true;
                }
                pthread_mutex_lock(&serverMutex);
                break;
            }
        }
        pthread_mutex_unlock(&serverMutex);
        
        if (!fwd) {
            Message m = {msg, MY_GROUP_ID, to, MY_GROUP_ID, time(nullptr), 1};
            pthread_mutex_lock(&serverMutex);
            messageQueue[to].push(m);
            for (const auto &p : connectedServers)
                if (!p.second.groupId.empty())
                    sendCommand(p.first, buildSENDMSG(to, MY_GROUP_ID, msg, MY_GROUP_ID));
            pthread_mutex_unlock(&serverMutex);
        }
        sendCommand(sock, fwd ? "OK,Delivered" : "OK,Queued");
    }
    else if (tokens[0] == "GETMSG") {
        pthread_mutex_lock(&serverMutex);
        bool has = !messageQueue[MY_GROUP_ID].empty();
        pthread_mutex_unlock(&serverMutex);
        if (has) {
            pthread_mutex_lock(&serverMutex);
            Message msg = messageQueue[MY_GROUP_ID].front();
            messageQueue[MY_GROUP_ID].pop();
            pthread_mutex_unlock(&serverMutex);
            sendCommand(sock, buildSENDMSG(MY_GROUP_ID, msg.fromGroup, msg.content));
        } else sendCommand(sock, "NO_MESSAGES");
    }
    else if (tokens[0] == "LISTSERVERS") {
        pthread_mutex_lock(&serverMutex);
        std::vector<std::tuple<std::string, std::string, int>> list;
        list.push_back(std::make_tuple(MY_GROUP_ID, myIpAddress, listenPort));
        for (const auto &p : connectedServers)
            if (!p.second.groupId.empty() && p.second.port > 0)  // Only show if we know their port
                list.push_back(std::make_tuple(p.second.groupId, p.second.ip, p.second.port));
        pthread_mutex_unlock(&serverMutex);
        sendCommand(sock, buildSERVERS(list));
    }
}

void *peerCommunicationThread(void *arg) {
    int sock = *(int *)arg;
    delete (int *)arg;
    std::string cmd;
    while (receiveCommand(sock, cmd)) {
        handleServerCommand(sock, cmd);
        pthread_mutex_lock(&serverMutex);
        if (connectedServers.find(sock) != connectedServers.end())
            connectedServers[sock].lastSeen = time(nullptr);
        pthread_mutex_unlock(&serverMutex);
    }
    pthread_mutex_lock(&serverMutex);
    std::string gid;
    if (connectedServers.find(sock) != connectedServers.end()) {
        gid = connectedServers[sock].groupId;
        connectedGroupIds.erase(gid);
        connectedServers.erase(sock);
        lastHeloAttempt.erase(gid); // Clean up rate limit tracking
    }
    pthread_mutex_unlock(&serverMutex);
    if (!gid.empty()) logMessage("Peer " + gid + " disconnected");
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <port> [--scan] [server_ip:port] ...\n", argv[0]); exit(0); }
    
    // Ignore SIGPIPE to prevent crashes on disconnected sockets
    signal(SIGPIPE, SIG_IGN);
    
    listenPort = atoi(argv[1]);
    myIpAddress = getLocalIPAddress();
    bool doScan = false;
    
    logFile.open(MY_GROUP_ID + "_server.log", std::ios::app);
    logMessage("======================================");
    logMessage("=== NEW SERVER INSTANCE STARTED ===");
    logMessage("======================================");
    logMessage("Server starting: " + MY_GROUP_ID + " on port " + std::to_string(listenPort));
    
    int listenSock = open_socket(listenPort);
    if (listenSock < 0 || listen(listenSock, 10) < 0) exit(1);
    
    pthread_t hThread;
    if (pthread_create(&hThread, NULL, healthMonitorThread, NULL) == 0) pthread_detach(hThread);
    
    for (int i = 2; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--scan") { doScan = true; continue; }
        size_t pos = arg.find(':');
        if (pos != std::string::npos) {
            connectToServer(arg.substr(0, pos), std::stoi(arg.substr(pos + 1)));
            sleep(2);
        }
    }
    
    if (doScan) triggerScan();
    else {
        pthread_mutex_lock(&serverMutex);
        if (connectedServers.size() == 0) { pthread_mutex_unlock(&serverMutex); connectToServer(TSAM_SERVER_IP, 5001); }
        else pthread_mutex_unlock(&serverMutex);
    }
    
    logMessage("Ready - listening for connections");
    
    while (true) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int cSock = accept(listenSock, (struct sockaddr *)&client, &len);
        if (cSock < 0) continue;
        
        logMessage("Accepted connection from " + std::string(inet_ntoa(client.sin_addr)) + 
                   ":" + std::to_string(ntohs(client.sin_port)));
        
        std::string cmd;
        if (receiveCommand(cSock, cmd)) {
            std::vector<std::string> tokens = parseCommand(cmd);
            if (!tokens.empty() && tokens[0] == "HELO") {
                pthread_mutex_lock(&serverMutex);
                connectedServers[cSock] = {cSock, "", inet_ntoa(client.sin_addr), 0, time(nullptr), time(nullptr), false, false};
                pthread_mutex_unlock(&serverMutex);
                
                handleServerCommand(cSock, cmd);
                
                // Check if HELO was accepted (socket still in connectedServers with groupId set)
                pthread_mutex_lock(&serverMutex);
                bool accepted = (connectedServers.find(cSock) != connectedServers.end() && 
                                !connectedServers[cSock].groupId.empty());
                pthread_mutex_unlock(&serverMutex);
                
                if (accepted) {
                    pthread_t tid;
                    int *ptr = new int(cSock);
                    if (pthread_create(&tid, NULL, peerCommunicationThread, ptr) == 0) {
                        pthread_detach(tid);
                    } else {
                        delete ptr;
                        close(cSock);
                        pthread_mutex_lock(&serverMutex);
                        connectedServers.erase(cSock);
                        pthread_mutex_unlock(&serverMutex);
                    }
                } else {
                    // HELO was rejected, clean up
                    close(cSock);
                    pthread_mutex_lock(&serverMutex);
                    connectedServers.erase(cSock);
                    pthread_mutex_unlock(&serverMutex);
                }
            } else {
                handleClientCommand(cSock, cmd);
                while (receiveCommand(cSock, cmd)) handleClientCommand(cSock, cmd);
                close(cSock);
            }
        } else close(cSock);
    }
    
    close(listenSock);
    logFile.close();
    return 0;
}