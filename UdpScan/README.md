# UDP Scanner and Challenge Solver Project

A comprehensive network programming project implementing UDP port discovery and automated challenge solving with advanced protocol handling including RFC 3514 "Evil Bit" support.

## Project Overview

This project consists of two complementary programs:
- **Scanner**: Discovers and identifies challenge ports on a target server
- **Puzzle Solver**: Automatically completes complex network challenges using extracted secrets

## Features

- **Intelligent port discovery** with response pattern matching
- **Automatic challenge identification** and port ordering
- **Fully automated challenge solving** with secret extraction
- **RFC 3514 "Evil Bit"** implementation for evil port communication
- **UDP checksum manipulation** for cryptographic challenges
- **Port knocking sequences** with signature-based authentication
- **ICMP echo request** support for bonus challenges
- **File output** with ready-to-run commands

## Files

- `scanner.cpp` - Port discovery and challenge identification
- `puzzlesolver.cpp` - Automated challenge solver
- `Makefile` - Build configuration
- `README.md` - This documentation

## Building

### Prerequisites

- C++ compiler with C++17+ support (g++ recommended)
- Linux/Unix environment
- Network permissions for raw socket operations (requires root for evil bit functionality)

### Compilation

```bash
# Build all targets
make

# Build individual programs
make scanner
make puzzlesolver

# Build with debug symbols
make debug

# Build optimized release
make release
```

## Usage

### Step 1: Port Discovery

Use the scanner to discover and identify challenge ports:

```bash
./scanner <IP Address> <low_port> <high_port>
```

**Parameters:**
- `IP Address`: Target server IP address
- `low_port`: Starting port number for scan range
- `high_port`: Ending port number for scan range

**Example:**
```bash
./scanner 130.208.246.98 4000 4100
```

**What it does:**
- Scans all ports in the specified range
- Identifies challenge ports by response patterns
- Orders ports by challenge type (secret, checksum, evil, final)
- Saves results to `open_ports.txt` with ready-to-run command

**Expected output:**
```
PORT: 8001 seems OPEN
PORT: 4011 seems OPEN  
PORT: 8003 seems OPEN
PORT: 8004 seems OPEN
Saved 4 open ports and ready-to-run command in open_ports.txt
```

### Step 2: Challenge Solving

Use the puzzle solver with the discovered ports:

```bash
./puzzlesolver <IP> <port1> <port2> <port3> <port4>
```

**Parameters:**
- `IP`: Target server IP address
- `port1-port4`: The four challenge ports in correct order

**Example:**
```bash
./puzzlesolver 130.208.246.98 8001 4011 8003 8004
```

**Automated workflow:**
1. **Secret Port Challenge**: Sends cryptographic secret message, extracts first secret port
2. **Checksum Challenge**: Handles UDP checksum manipulation, extracts secret phrase
3. **Evil Port Challenge**: Sends RFC 3514 evil bit packet, extracts second secret port
4. **Final Challenge**: Combines extracted ports, initiates port knocking sequence
5. **Port Knocking**: Automatically completes authentication sequence

## Challenge Types Handled

### 1. Secret Port (S.E.C.R.E.T Protocol)
- Sends generated secret message with group information
- Handles challenge-response authentication
- Extracts first secret port number

### 2. Checksum Port  
- Manipulates UDP checksum to match server requirements
- Sends encapsulated UDP packets with specific source IPs
- Extracts secret phrases from server responses

### 3. Evil Port (RFC 3514)
- Implements "Evil Bit" in IPv4 header flags
- Creates raw packets with malicious intent marking
- Required for communication with security-conscious ports

### 4. Final Port
- Combines extracted secret ports
- Initiates port knocking sequences
- Handles signature-based authentication

## Automatic Information Extraction

The puzzle solver automatically detects and extracts:

- **Secret Port Numbers**: From messages like "You have earned the right to know the port: 4079!"
- **Secret Phrases**: From quoted text in server responses
- **Port Knock Sequences**: From comma-separated lists in final responses

## ICMP Bonus Challenge

For bonus points, send ICMP echo requests with group identifiers:

```bash
ping -c 1 -p $(echo -n '$group_7' | xxd -p) 130.208.246.98
```

## Sample Workflow

```bash
# 1. Discover challenge ports
./scanner 130.208.246.98 4000 4100

# 2. Check the generated command
cat open_ports.txt

# 3. Run the challenge solver (copy command from file)
./puzzlesolver 130.208.246.98 8001 4011 8003 8004

# 4. (Optional) Send ICMP for bonus points
ping -c 1 -p $(echo -n '$group_7' | xxd -p) 130.208.246.98
```

## Network Analysis

Use Wireshark to analyze network traffic:
1. Capture on appropriate network interface
2. Filter for UDP traffic: `udp`
3. Filter for specific IP: `ip.addr == 130.208.246.98`
4. Look for custom headers (evil bit, checksums, encapsulated packets)

## Troubleshooting

### Permission Issues

Raw socket operations require elevated privileges:
```bash
sudo ./puzzlesolver 130.208.246.98 8001 4011 8003 8004
```

### Port Discovery Issues

If scanner doesn't find 4 ports:
- Expand search range: `./scanner 130.208.246.98 3000 5000`
- Check network connectivity: `ping 130.208.246.98`
- Verify target server is running

### Challenge Solver Issues

If automatic extraction fails:
- Check server response formats in output
- Verify all 4 challenge ports are correct
- Ensure proper network permissions for raw sockets

## Security Notes

- This tool is for educational purposes
- Use only on networks you own or have permission to test
- Raw socket operations require appropriate privileges
- Be mindful of network policies and firewalls

## Group Configuration

Current configuration is set for Group 7:
- Group identifier: `$group_7`
- Secret number: `0x00816BF2`
- Users: `odinns24,thorvardur23,thora23`
- Evil signature: `{0xBA, 0x5C, 0xEB, 0x88}`

## License

Educational project - use responsibly.

## Group Information

- Group 7 implementation
- Network Programming Course
- UDP/ICMP Protocol Exploration
- Advanced Raw Socket Programming