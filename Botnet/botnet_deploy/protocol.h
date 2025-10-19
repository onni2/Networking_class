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
const char EOT = 0x04;  // End of Transmission (for hop separator)

const size_t MAX_MESSAGE_LENGTH = 5000;
const size_t HEADER_SIZE = 5; // SOH(1) + length(2) + STX(1) + ETX(1)
const size_t MAX_HOPS_LENGTH = 240; // ~48 hops at 5 chars each (A5_XX,)
const size_t MAX_PAYLOAD_LENGTH = MAX_MESSAGE_LENGTH - HEADER_SIZE - MAX_HOPS_LENGTH - 1; // -1 for EOT

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
 * Parse SENDMSG command with hop tracking
 * Separates main command from hops using EOT marker
 * 
 * @param command The full SENDMSG command string
 * @param mainCommand Output - the command without hops
 * @param hops Output - the hop tracking string (empty if no hops)
 * @return true if successfully parsed
 */
bool parseSENDMSGWithHops(const std::string& command, std::string& mainCommand, std::string& hops);

/**
 * Check if a group ID exists in hop string (loop detection)
 * 
 * @param hops Comma-separated hop string
 * @param groupId Group ID to search for
 * @return true if group ID found in hops
 */
bool isInHops(const std::string& hops, const std::string& groupId);

/**
 * Build a HELO command
 * Format: HELO,<FROM GROUP ID>
 */
std::string buildHELO(const std::string& groupId);

/**
 * Build a SERVERS command
 * Format: SERVERS,<server1_id>,<ip1>,<port1>;<server2_id>,<ip2>,<port2>;...
 * Note: First server should be the sending server itself
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
 * Build a SENDMSG command with optional hop tracking
 * Format: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content><EOT><hops>
 * The EOT and hops are only included if hops is not empty
 */
std::string buildSENDMSG(const std::string& toGroup, const std::string& fromGroup, 
                         const std::string& message, const std::string& hops = "");

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