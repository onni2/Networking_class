# Networking Notes

## OSI Model (7 Layers)

1. **Physical Layer**
2. **Data Link Layer**
3. **Network Layer** - ip, arp
4. **Transport Layer** - TCP, UDP, ICMP, OSPF
5. **Session Layer**
6. **Presentation Layer**
7. **Application Layer** - HTTP, Telnet, DNS, Ping

---

## Key Terms to Remember

- **ARP** – Address Resolution Protocol
- **MAC** – Media Access Control address
- **NIC** – Network Interface Card
- **TDM** – Time Division Multiplexing
- **FDMA** – Frequency Division Multiple Access
- **LAN** – Local Area Network
- **WAN** – Wide Area Network

---

## Network Guarantees
- There are **no guarantees!**
- You must handle:
  - Bandwidth
  - Security
  - Delivery

---

## Ethernet and Wi-Fi

- **Ethernet** uses **CSMA/CD** (Carrier Sense Multiple Access with Collision Detection).
- **Wi-Fi** uses **CSMA/CA** (Carrier Sense Multiple Access with Collision Avoidance).

---

## General Networking Concepts

- **Packets ≈ Frames ≈ Segments**
- Ethernet standard since **1982**
- Adopted by **IEEE as 802.3**
- **EtherType field vs. Length**
  - Frame length: **max 1500 bytes (octets)**

---

## Preamble

- Helps synchronize sender and receiver.
- Pattern `010101…` used to align the connection.

---

## Media Access Control

- Sender waits, then transmits.
- Must check for collisions:
  - If collision detected → both hosts wait a **random backoff time**.
  - Randomness is important to avoid repeated collisions.
- Early protocol: **ALOHA**
  - Simple but inefficient
  - Modern improvements ensure better bandwidth & fewer collisions.

---

## LAN (Local Area Network)

- Originally: shared medium → collisions common.
- Now: **switches** give each device its own "line" → reduces collisions.

---

## Addressing

- Like writing sender + recipient details on a postcard.
- **MAC addresses:**
  - Must be globally unique.
  - Burned into NIC hardware before shipping.
  - First bits = **manufacturer ID**.
- **Nobody cares about your MAC address** (not sensitive like IP).
- Originally **48-bit** space (good until ~2080).
- **64-bit MAC addresses** → astronomically large address space.

---

## Addressing Types

- **Unicast** – one-to-one communication.
- **Multicast** – one-to-many (hosts can choose to ignore).
- **Broadcast** – `FF:FF:FF:FF:FF:FF` → all hosts process.
- **Unicast Flooding**:
  - Switch forwards a frame to **all hosts** if destination unknown.
  - Stops once MAC is learned.

---

## ARP (Address Resolution Protocol)

- Translates **IP address → hardware (MAC) address**.
- Every host has an IP address → ARP asks for the matching MAC.
- ARP packet fields:
  - Sender MAC
  - Sender IP
  - Target MAC (all zero → “question”)
  - Target IP (address being queried)
- ARP caches results to avoid repeated requests.

### Proxy ARP
- A router can respond to ARP requests on behalf of another host.
- Then it forwards traffic correctly in the background.

---

## Address Types Summary

- **Hardware (MAC) address** → one-to-one link.
- **IP address** → logical addressing for routing across multiple networks.

---

## Wi-Fi (CSMA/CA)

- Uses **collision avoidance** instead of detection.
- Listens before transmitting, but "hidden node problem" exists:
  - Example: Host A and Host C cannot hear each other, but both try to talk to Host B → collision at B.

---

## LAN vs WAN
- **Local** area network
- **Wide** area network
# WAN architeture
- X.25
- ATM 
- Internet

---

## Circuit vs packet switching
- ?
## Problems to solve
- Addressing - identify hosts
- Routing - how do packets know where to got to
- Fragmentation - If laarge packets get broken down how do we rebuild them
- Reliability - Handle errors (hamming and parity bits)
- Order - how do they know what order (big- and small endian
- Scale - num of nodes network can handle and how much traffic they can generate

---

### How the internet architecure was found
- Connectivity is its own reward
- End to end function req end to end protocols
- Experience trumps theory
- Alld design must scale
- Be strict when sending and tolerant in receiving - Dont be a asshole and send random things. We use it all together
- Circular dependencies must avoided
- Modularity is important. Keep things seperate when possible
- Keep it simple -avoid options and parameters whatever....

---

## IPv4
- It is form 20-60 bytes
