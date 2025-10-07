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
- Makefile           : Build configuration
- README.txt         : This file
- client_server.pcap : Wireshark trace showing all client commands

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

WIRESHARK CAPTURE INSTRUCTIONS
-------------------------------

To create the required Wireshark trace:

1. Start Wireshark with root privileges:
   sudo wireshark

2. Start capturing on the loopback interface (lo)

3. Apply display filter: tcp.port == 4044

4. Start the server: ./tsamgroup1 4044

5. Start the client and run ALL commands:
   ./client 127.0.0.1 4044
   > LISTSERVERS
   > SENDMSG,A5_2,Test message
   > GETMSG
   > QUIT

6. Stop the capture in Wireshark

7. Save as: File → Export Specified Packets → client_server.pcap

8. Verify the capture shows:
   - TCP connection establishment
   - Protocol framing with 0x01, 0x02, 0x03 bytes
   - All three client commands (LISTSERVERS, SENDMSG, GETMSG)
   - Server responses

WHAT YOU SHOULD SEE IN WIRESHARK
---------------------------------

The .pcap file should contain:

1. TCP handshake (SYN, SYN-ACK, ACK)
2. Client sends: <SOH><length><STX>LISTSERVERS<ETX>
3. Server responds: <SOH><length><STX>SERVERS,...<ETX>
4. TCP teardown for that connection
5. New TCP handshake
6. Client sends: <SOH><length><STX>SENDMSG,A5_2,Test message<ETX>
7. Server responds with acknowledgment
8. TCP teardown
9. New TCP handshake
10. Client sends: <SOH><length><STX>GETMSG<ETX>
11. Server responds: <SOH><length><STX>SENDMSG,... or NO_MESSAGES<ETX>
12. TCP teardown

You can follow TCP streams in Wireshark to see the actual protocol bytes.

IMPORTANT CONFIGURATION
-----------------------

Before compiling, change these values:

In server.cpp (line 15):
    const std::string MY_GROUP_ID = "A5_1";  // Change to your group ID

In client.cpp (line 12):
    const std::string MY_GROUP_ID = "A5_1";  // Change to your group ID

In Makefile (line 6):
    GROUP_NUM = 1  # Change to your group number

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

SUBMISSION CHECKLIST
--------------------

This ZIP file should contain:
☑ server.cpp (stub version)
☑ client.cpp (complete)
☑ protocol.h
☑ protocol.cpp
☑ Makefile
☑ README.txt (this file)
☑ client_server.pcap (Wireshark trace)

===============================================================================
END OF README
===============================================================================
