===============================================================================
T-409-TSAM Assignment 5: Early Bot Bonus Submission (2a)
Group: A5_1
Bonus points claimed: 10 points (2a - Early Bot)
===============================================================================

CONTENTS
--------

- server.cpp          : Server stub implementation
- client.cpp          : Client implementation  
- protocol.h          : Protocol header file
- protocol.cpp        : Protocol implementation
- Makefile            : Build configuration
- README.md           : This file
- client_server.pcap  : Wireshark trace showing all client commands

WHAT IS IMPLEMENTED
-------------------

This is an EARLY submission for bonus points (2a). It includes:

✓ Complete client implementation with all required commands
✓ Server stub that responds to client commands
✓ Proper protocol framing (<SOH><length><STX><command><ETX>)
✓ Wireshark trace showing all client-to-server communication

NOTE: The server does NOT yet implement full peer-to-peer communication
with other servers. This will be added in the final submission.

COMPILATION

-----------
Operating System: Linux (tested on Arch)

To compile both server and client:
    make

To clean build artifacts:
    make clean

RUNNING THE SERVER
------------------

Start the server stub:
    ./tsamgroup1 4044

The server will:

- Listen on port 4044 for client connections
- Accept one client connection at a time
- Respond to client commands (SENDMSG, GETMSG, LISTSERVERS)
- Log all activity to A5_1_server.log

RUNNING THE CLIENT
------------------

Start the client (in a separate terminal):
    ./client 127.0.0.1 4044

CLIENT COMMANDS IMPLEMENTED
----------------------------

1. SENDMSG,<group_id>,<message>
   Example: SENDMSG,A5_2,Hello from my client!

2. GETMSG
   Example: GETMSG
   (Server has pre-loaded test messages to demonstrate this works)

3. LISTSERVERS
   Example: LISTSERVERS
   (Shows current server - more servers will be added in final version)

4. QUIT
   Example: QUIT

PROTOCOL FORMAT
---------------

All messages between client and server use:
    <SOH><length><STX><command><ETX>

Where:

- SOH = 0x01 (Start of Header)
- length = 16-bit unsigned integer in network byte order
- STX = 0x02 (Start of Text)  
- command = Comma-separated command string
- ETX = 0x03 (End of Text)

TESTING PROCEDURE
-----------------

1. Start server:
   ./tsamgroup1 4044

2. Start client in another terminal:
   ./client 127.0.0.1 4044

3. Test all commands:
   LISTSERVERS
   SENDMSG,A5_2,Test message from early bot
   GETMSG
   GETMSG
   QUIT

4. Check logs:
   cat A5_1_server.log
   cat A5_1_client.log

WHAT IS NOT YET IMPLEMENTED
----------------------------

The following will be added for the final submission:

- Full server-to-server communication (HELO, SERVERS handshake)
- Connection management (maintaining 3-8 peer connections)
- Message forwarding between servers
- KEEPALIVE mechanism
- Connecting to other groups' servers
- Automatic reconnection logic

This stub is sufficient for the Early Bot bonus (2a - 10 points).

CODE STRUCTURE
--------------

protocol.cpp/h:

- sendCommand() - Sends protocol-framed messages
- receiveCommand() - Receives protocol-framed messages
- parseCommand() - Parses comma-separated commands
- build*() functions - Construct protocol commands
- getTimestamp() - Creates log timestamps

server.cpp (STUB):

- open_socket() - Creates listening socket
- handleClientCommand() - Processes client commands
- Main loop accepts client connections one at a time
- Stores messages in simple queue for demo
- Pre-loads test messages to demonstrate GETMSG

client.cpp:

- Connects to server for each command
- Sends properly framed protocol messages
- Receives and displays server responses
- Logs all activity with timestamps

LOG FILES
---------

A5_1_server.log - Server activity log
A5_1_client.log - Client activity log

Both files contain timestamped entries showing all sent and received commands.
