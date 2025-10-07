#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include <vector>
#include <cstdint>
#include <tuple>

// Protocol frame markers
const char SOH = 0x01;  // Start of Header
const char STX = 0x02;  // Start of Text
const char ETX = 0x03;  // End of Text

const size_t MAX_MESSAGE_LENGTH = 5000;
const size_t HEADER_SIZE = 5; // SOH(1) + length(2) + STX(1) + ETX(1)

/**
 * Send a command using the protocol format:
 * <SOH><length><STX><command><ETX>
 * 
 * @param socket The socket to send on
 * @param command The command string to send
 * @return true if successful, false otherwise
 */
bool sendCommand(int socket, const std::string& command);

/**
 * Receive a command from a socket.
 * This function will block until a complete message is received.
 * 
 * @param socket The socket to receive from
 * @param command Output parameter - the received command
 * @return true if successful, false if connection closed or error
 */
bool receiveCommand(int socket, std::string& command);

/**
 * Parse a command string into tokens (split by comma)
 * 
 * @param command The command string to parse
 * @return Vector of tokens
 */
std::vector<std::string> parseCommand(const std::string& command);

/**
 * Split server list by semicolon separators
 * Used for parsing SERVERS responses
 * 
 * @param serverList The server list string to parse
 * @return Vector of server strings
 */
std::vector<std::string> splitServers(const std::string& serverList);

/**
 * Build a HELO command
 * Format: HELO,<FROM GROUP ID>
 */
std::string buildHELO(const std::string& groupId);

/**
 * Build a SERVERS command
 * Format: SERVERS,<server1_id>,<ip1>,<port1>;<server2_id>,<ip2>,<port2>;...
 */
std::string buildSERVERS(const std::vector<std::tuple<std::string, std::string, int>>& servers);

/**
 * Build a KEEPALIVE command
 * Format: KEEPALIVE,<No. of Messages>
 */
std::string buildKEEPALIVE(int messageCount);

/**
 * Build a GETMSGS command
 * Format: GETMSGS,<GROUP ID>
 */
std::string buildGETMSGS(const std::string& groupId);

/**
 * Build a SENDMSG command
 * Format: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
 */
std::string buildSENDMSG(const std::string& toGroup, const std::string& fromGroup, 
                         const std::string& message);

/**
 * Build a STATUSREQ command
 */
std::string buildSTATUSREQ();

/**
 * Build a STATUSRESP command
 * Format: STATUSRESP,<server1>,<msgs1>,<server2>,<msgs2>,...
 */
std::string buildSTATUSRESP(const std::vector<std::pair<std::string, int>>& serverMessages);

/**
 * Get current timestamp as string for logging
 * Format: YYYY-MM-DD HH:MM:SS
 */
std::string getTimestamp();

#endif // PROTOCOL_H