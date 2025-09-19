# UDP Scanner Project

A network programming project implementing UDP port scanning and puzzle solving with special protocol handling including RFC 3514 "Evil Bit" support.

## Project Overview

This project consists of two main components:
- **Scanner**: A UDP port scanner that handles multiple ports with different protocols
- **Puzzle Solver**: A utility for solving network-based puzzles and challenges

## Features

- UDP port scanning with custom payloads
- Secret message generation and handling
- RFC 3514 "Evil Bit" implementation for evil port communication
- ICMP echo request support for bonus challenges
- Wireshark-compatible packet generation
- Multiple operation modes

## Files

- `scanner.cpp` - Main UDP scanner implementation
- `puzzlesolver.cpp` - Puzzle solving utilities
- `Makefile` - Build configuration
- `README.md` - This documentation

## Building

### Prerequisites

- C++ compiler with C++17+ support (g++ recommended)
- Linux/Unix environment
- Network permissions for raw socket operations (some features may require root)

### Compilation

```bash
# Build all targets
make

# Build specific target
make scanner
make puzzlesolver

# Build with debug symbols
make debug

# Build optimized release
make release
```

## Usage

### Scanner

```bash
./scanner <IP Address> <low_port> <high_port>
```

**Parameters:**
- `IP Address`: Target IP address to scan
- `low_port-high_port`: Scans from low to high (4000-4100)

**Example:**
```bash
./scanner 130.208.246.98 4000 4100
```

### Puzzle Solver

```bash
./puzzlesolver <IP Address> <port1> <port2> <port3> <port4> <mode> <[secret_port1, secret_port2]>
```

## Port Handling

The scanner handles different ports with specialized protocols:

1. **Port 1 (Secret Port)**: Sends generated secret messages
2. **Port 2 (Signature Port)**: Sends 4-byte signature payload  
3. **Port 3 (Evil Port)**: Implements RFC 3514 "Evil Bit" for malicious packet detection
4. **Port 4 (Last prot)**: The port for the knocks
5. **mode** : It is a bit redundant now but i used it to print out the message. but now it is 1 always.

## Special Features

### RFC 3514 Evil Bit

The scanner implements RFC 3514 (April Fools' RFC) for the "evil port" challenge:
- Sets the reserved bit in IPv4 header flags field
- Creates properly formatted malicious packets
- Required for communication with security-conscious evil ports

### Secret Message Generation

Generates cryptographic secret messages using:
- 32-bit secret numbers
- User group information
- Custom encoding schemes

### ICMP Bonus Challenge

For bonus points, send ICMP echo requests:
```bash
ping -c 1 -p $(echo -n '$group_7' | xxd -p) 130.208.246.98
```

## Network Analysis

Use Wireshark to analyze network traffic:
1. Capture on appropriate network interface
2. Filter for UDP traffic: `udp`
3. Filter for specific IP: `ip.addr == 130.208.246.98`
4. Examine packet details for custom payloads and headers

## Troubleshooting

### Permission Issues

Some features require elevated privileges:
```bash
sudo ./scanner 130.208.246.98 8001 8002 8003 8004 1
```

### Network Connectivity

Test basic connectivity first:
```bash
ping -c 1 130.208.246.98
```

### Raw Socket Issues

If raw sockets fail:
1. Check if running as root
2. Verify network interface permissions
3. Check firewall/iptables rules

## Development

### Adding New Features

1. Modify source files
2. Update Makefile if needed
3. Test with `make debug`
4. Document changes

### Code Style

- C++17 standard
- Consistent indentation
- Error handling with proper cleanup
- Network byte order conversion

## Security Notes

- This tool is for educational purposes
- Use only on networks you own or have permission to test
- Raw socket operations require appropriate privileges
- Be mindful of network policies and firewalls

## License

Educational project - use responsibly.

## Group Information

- Group 7 implementation
- Network Programming Course
- UDP/ICMP Protocol Exploration
