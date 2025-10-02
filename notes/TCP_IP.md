# Computer Networks Study Notes - Assignment Prep 

## 1. TCP/IP Basics & Round Trip Time (RTT) 

### What is RTT?
**Round Trip Time (RTT)** = Time for a packet to travel from sender → receiver → back to sender

**Formula**: RTT = 2 × (Distance ÷ Speed)

### Example Calculations:
- **Local network**: ~1ms (data travels through switches/routers)
- **Cross-country**: ~50ms (fiber optic cables)
- **Satellite**: ~500ms (up to satellite, down to ground)
- **To the Moon**: ~2.6 seconds (384,400 km at light speed)
- **To Alpha Centauri**: 4.4 light years × 2 = **8.8 years!** 

### Why RTT Matters:
- TCP waits for acknowledgments before sending more data
- Longer RTT = more waiting = slower communication
- In space, this becomes a HUGE problem!

---

## 2. TCP Connection Management 🔗

### TCP Flags:
- **URG**: Urgent data, prioritize  
- **ACK**: Acknowledgment
- **PSH**: Send data to application immediately
- **RST**: Reset connection
- **SYN**: Establish connection (first packet)
- **FIN**: Terminate connection

### Special TCP Flags:
- **ECE**: ECN-Echo (Explicit Congestion Notification)
- **CWR**: Congestion Window Reduced  
- **NS**: ECN-nonce (experimental RFC 3540)

### Connection Management Details:
**3-Way Handshake** uses SYN and ACK flags:
```
Client                    Server
  |                         |
  |---> SYN=1 ------------->|  "Let's connect!"
  |<-- SYN=1,ACK=1 <--------|  "Sure! Ready?"  
  |---> ACK=1 ------------->|  "Great, let's go!"
  |                         |
```

**4-Way Close** uses FIN and ACK flags:
```
Client                    Server
  |                         |
  |---> FIN=1 ------------->|  "I'm done"
  |<--- ACK=1 <-------------|  "Got it"
  |<--- FIN=1 <-------------|  "Me too"  
  |---> ACK=1 ------------->|  "Bye!"
  |                         |
```

### Why Setup/Teardown Per Message is BAD for Space:
- **Earth to Alpha Centauri example**:
  - Send "Hello" → Wait 8.8 years for connection
  - Send actual message → Wait 8.8 years for ACK
  - Close connection → Wait 8.8 years for close ACK
  - **Total: 26.4 years for one "Hello"!**

---

## 3. TCP Sliding Windows

### What are Sliding Windows?
Think of it like a conveyor belt of packets that can be "in flight" at once.

### Window Size Calculation:
**Optimal Window Size = Bandwidth × RTT**

### Example:
- Link speed: 1 Mbps
- RTT: 8.8 years = 277,000,000 seconds
- Window needed: 1,000,000 bits/sec × 277,000,000 sec = **277 billion bits!**
- That's about 34 GB of buffer space!

### Visual Example:
```
Sender Buffer:    [1][2][3][4][5][6][7][8]
                   ^           ^
                 Sent      Window Edge
                           
Can send packets 1-4, waiting for ACK for packet 1
When ACK(1) arrives, window slides: [2][3][4][5][6][7][8][9]
```

---

## 4. TCP Congestion Control (Question 3!)
**CWND** stands for **Congestion Window** - it's one of the most important concepts in TCP:

## What is CWND? 

**CWND (Congestion Window)** = The limit that controls how much data can be sent out onto the network before receiving an ACK from the receiver.

### The Key Idea:
TCP actually has **TWO** window sizes working at the same time:

1. **Receiver's Advertised Window** - "I can handle this much data in my buffer"
2. **Congestion Window (CWND)** - "The network can handle this much data without congestion"

**Actual window size = min(Receiver's window, CWND)**

### Think of it like this:
- **Receiver's window** = Size of your friend's mailbox 
- **CWND** = How much traffic the postal system can handle 
- You can only send as much as the **smaller** of the two allows!

### How CWND Changes:

**During Slow Start:**
- Starts at **CWND = MSS** (Maximum Segment Size, usually ~1500 bytes)
- For each ACK received: **CWND += 1 MSS**
- Example: 1 → 2 → 4 → 8 → 16 → 32... (exponential growth!)

**During Congestion Avoidance:**
- **CWND increases by 1 MSS every RTT** (much slower, linear growth)

**During Fast Recovery (TCP Reno):**
- After packet loss: **CWND = ssthreshold** 
- Each new ACK: **CWND += MSS/CWND** (additive increase)

### Why CWND Matters:

In **Figure 1** (your TCP Reno graph), the Y-axis shows CWND in "KB or packets" - this is what you're tracking as it goes up and down with the sawtooth pattern!

- **Steep curves** = CWND growing exponentially (slow start)
- **Gentle slopes** = CWND growing linearly (congestion avoidance)  
- **Sharp drops** = CWND cut in half due to packet loss

### Real Example from Slides:
The flow control diagram shows:
- Window = 2500 bytes → sender can send bytes 1-2500
- When ACK comes back with "window = 500" → sender can only send 500 more bytes
- When "window = 0" → sender must STOP completely!

So CWND is basically TCP's way of saying *"Don't flood the network - only send this much data at once!"* 
---
### TCP Reno States:

#### **Slow Start** 
- **When**: Start of connection OR after timeout (CWND = MSS, Threshold = large)
- **Mechanism**: For each ACKed segment, CWND += 1 MSS
- **Pattern**: Exponential growth (1 → 2 → 4 → 8 → 16 → 32...)
- **Example from Lecture**:
  - Start: CWND = 1, send 1 segment
  - Receive 1 ACK: CWND = 2, send 2 segments  
  - Receive 2 ACKs: CWND = 4, send 4 segments
- **Stops when**: Reaches ssthreshold OR packet loss detected

#### **Congestion Avoidance** 
- **When**: Window size ≥ ssthreshold
- **Behavior**: Linear growth - increases by 1 MSS every RTT
- **Pattern**: Gentle slope (32 → 33 → 34 → 35...)
- **Goal**: Cautiously probe for available bandwidth

#### **Fast Retransmit** (TCP Reno Innovation)
- **Trigger**: Receiver detects out-of-order segment
- **Process**:
  1. Receiver generates immediate ACK (duplicate ACK)
  2. Receiver informs sender what segment number is expected
  3. If sender receives **3+ consecutive duplicate ACKs**: Retransmit immediately
- **Problem it solves**: Don't wait for timeout - recover faster!

#### **Fast Recovery** (TCP Reno's Key Feature)
- **When**: After fast retransmit occurs
- **Actions**:
  1. **Reset threshold to half last CWND** (ssthreshold = CWND/2)
  2. **Skip slow start** (this is the key difference from TCP Tahoe!)
  3. **Each new ACK increases CWND by MSS/CWND** (additive increase)
- **Why important**: Avoids dropping back to CWND=1 like TCP Tahoe

### Reading Figure 1 from Your Assignment:
The classic "sawtooth pattern" shows:
- **Steep exponential curves**: Slow start periods
- **Gentle linear slopes**: Congestion avoidance (additive increase)
- **Sharp drops to threshold**: Packet loss events with fast recovery
- **Pattern repeats**: The characteristic TCP Reno sawtooth behavior

### Key Differences: TCP Tahoe vs TCP Reno
- **TCP Tahoe**: Loss → ssthreshold = CWND/2, CWND = 1, restart slow start
- **TCP Reno**: Loss → ssthreshold = CWND/2, CWND = ssthreshold, continue with congestion avoidance

---

## 5. Acknowledgment Strategies 

### Regular ACKs:
```
A sends: [Packet 1]
B sends: [ACK 1]
A sends: [Packet 2] 
B sends: [ACK 2]
```

### Piggyback ACKs:
```
A sends: [Packet 1]
B sends: [Packet X + ACK 1]  ← ACK rides on data packet
A sends: [Packet 2 + ACK X]  ← Both directions use piggyback
```

### Delayed ACKs (RFC 1122 - From Your Course):
- **Rule**: Host may delay sending ACK by up to **500ms**
- **Exception**: With stream of full-size segments, must ACK every 2nd segment
- **Benefit**: ACKs can piggyback on data going the other way
- **Savings**: 40+ bytes each time this works
- **Problem**: Can create "Silly Window Syndrome"

### When Piggyback ACKs Work Well:
- ✅ **Interactive applications**: Both sides exchange data
- ✅ **Request-response protocols**: Natural back-and-forth
- ❌ **One-way streams**: No return data to piggyback on
- ❌ **Infrequent communication**: Long delays between messages

### Nagle's Algorithm (From Your Slides):
- **Goal**: Avoid sending lots of small packets
- **Default**: Typically ON by default  
- **Rule**: As long as there's an unacknowledged packet:
  - Buffer new data instead of sending immediately
  - Send when: enough data collected OR ACK arrives
- **Disable with**: `setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, ...)`

### Nagle + Delayed ACK = DEADLOCK:
- **Sender**: Waits for ACK before sending more data (Nagle)
- **Receiver**: Waits for data to piggyback ACK on (Delayed ACK)
- **Result**: Both wait forever!
- **Solution**: Disable Nagle for real-time apps (games, live updates)

---

## 6. Socket Programming Essentials

### UDP vs TCP:
```cpp
// UDP Socket Creation
socket(AF_INET, SOCK_DGRAM, 0)   // Datagram = UDP

// TCP Socket Creation  
socket(AF_INET, SOCK_STREAM, 0)  // Stream = TCP
```

### Key UDP Characteristics:
- **Connectionless**: No handshake needed
- **Unreliable**: Packets can be lost, duplicated, reordered
- **Fast**: No connection overhead
- **Packet boundaries**: Each send() = one packet

### Common UDP Issues:
1. **Buffer size limits**: 
   - Message longer than buffer → truncated
   - Solution: Use larger buffer or split messages

2. **No guaranteed delivery**:
   - Packets can disappear without notice
   - Solution: Add application-level ACKs

3. **Packet reordering**:
   - Packets can arrive out of order
   - Solution: Add sequence numbers

---

## 7. Protocol Design for Extreme Conditions 

### Problems with Standard TCP over Interstellar Links:
1. **Huge RTT**: 8.8 years means very slow error recovery
2. **Massive buffers**: Need gigabytes of memory
3. **Timeouts**: Standard timeouts are way too short
4. **Congestion control**: Designed for milliseconds, not years

### Better Approach - Store and Forward:
```
Earth → Relay Stations → Alpha Centauri

[Message] → [Store] → [Forward] → [Store] → [Forward]
           ↓                    ↓
       Send ACK            Send ACK
```

### Application-Level Reliability:
Instead of TCP's automatic retransmission, use:
- **Message IDs**: Each message gets unique number
- **Explicit ACKs**: "I got message #47"
- **Timeouts adapted to distance**: Wait years, not seconds
- **Store-and-forward**: Use UDP + custom reliability

---

## 8. Debugging Network Code 

### Common Issues:

#### **Buffer Size Problems**:
```cpp
char buffer[25];  // ← Too small!
recv(sock, buffer, sizeof(buffer), 0);
// Long messages get truncated
```

#### **String Termination**:
```cpp
// WRONG:
std::cout << buffer << std::endl;  // May print garbage

// RIGHT:
std::cout << std::string(buffer, n) << std::endl;
```

#### **Protocol Mismatches**:
- Client expects TCP, server uses UDP
- Different message formats
- Different byte ordering (endianness)

### Debugging Strategy:
1. **Check protocols match** (TCP vs UDP)
2. **Verify buffer sizes** (are messages being truncated?)
3. **Add logging** (print what you send vs receive)
4. **Test locally first** (same machine, then different machines)

---

## 9. Key Formulas & Numbers 

### Essential Calculations:
- **RTT**: 2 × distance ÷ speed
- **Window Size**: Bandwidth × RTT  
- **Throughput**: Window Size ÷ RTT
- **Light Speed**: ~300,000 km/second
- **1 Light Year**: ~9.46 × 10¹² km

### TCP Reno Rules:
- **Slow Start**: Window doubles each RTT
- **Congestion Avoidance**: Window += 1 each RTT
- **Fast Recovery**: ssthreshold = window/2, then window = ssthreshold

---

## 10. Assignment Strategy Tips

### Question 1-2 (Space TCP):
- Calculate RTT first (distance × 2 ÷ light speed)
- Think about why standard TCP fails (connection overhead, huge windows)
- Consider store-and-forward alternatives with relay stations
- Remember: 1000 bytes/month is VERY low traffic
- **Key insight**: TCP's 3-way handshake per message is catastrophic over interstellar distances

### Question 3 (TCP Graph Analysis) - **Use Figure 1 from your assignment**:
- **Identify slow start**: Look for exponential/steep growth curves
- **Spot congestion avoidance**: Linear/gentle slope increases  
- **Find packet loss events**: Sharp drops in window size
- **Track ssthreshold**: Horizontal dashed lines after each loss
- **Note the sawtooth pattern**: Classic TCP Reno behavior
- **From your course**: ssthreshold gets set to CWND/2 at each loss

### Question 4 (Socket Debug):
- Compare message lengths vs buffer sizes (truncation issues)
- Check UDP vs TCP protocol mismatches
- Consider packet reordering in UDP
- Look for missing null terminators in C++ strings
- Think about Nagle algorithm interactions
- **From slides**: UDP has packet boundaries, TCP is a stream

### Specific References for Your Assignment:
- **TCP Reno details**: See Lecture 11 slide 25 from your course
- **Fast Recovery formula**: Each new ACK increases CWND by MSS/CWND  
- **Delayed ACK rules**: 500ms max delay, must ACK every 2nd full segment
- **Nagle algorithm**: Can be disabled with TCP_NODELAY socket option
