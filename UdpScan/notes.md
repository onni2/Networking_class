
---

### PORT 4040 says:

Send me a 4-byte message containing the signature you got from **S.E.C.R.E.T** in the first 4 bytes (in network byte order).

---

### PORT 4057 says:

The dark side of network programming is a pathway to many abilities some consider to be...unnatural. I am an evil port, I will only communicate with evil processes! ([Evil bit](https://en.wikipedia.org/wiki/Evil_bit))  
Send us a message of 4 bytes containing the signature that you created with **S.E.C.R.E.T**.

---

### PORT 4076 says:

Greetings! I am **E.X.P.S.T.N**, which stands for "Enhanced X-link Port Storage Transaction Node".

What can I do for you?  
- If you provide me with a list of secret ports (comma-separated), I can guide you on the exact sequence of "knocks" to ensure you score full marks.

**How to use E.X.P.S.T.N?**  
1. Each "knock" must be paired with both a secret phrase and your unique **S.E.C.R.E.T** signature.  
2. The correct format to send a knock: First, 4 bytes containing your **S.E.C.R.E.T** signature, followed by the secret phrase.

**Tip:** To discover the secret ports and their associated phrases, start by solving challenges on the ports detected using your port scanner. Happy hunting!

---

### PORT 4096 says: 

Greetings from **S.E.C.R.E.T.** (Secure Encryption Certification Relay with Enhanced Trust)! Here's how to access the secret port I'm safeguarding:  
1. Generate a 32-bit secret number (and remember it for later).  
2. Send me a message where the first 5 bytes are the letter `'S'` followed by your secret number (in network byte order) and the rest of the message is a comma-separated list of the RU usernames of all your group members.  
3. I will reply with a 5-byte message, where the first byte is your group ID and the remaining 4 bytes are a 32-bit challenge number (in network byte order).  
4. Combine this challenge using the XOR operation with the secret number you generated in step 1 to obtain a 4-byte signature.  
5. Reply with a 5-byte message: the first byte is your group number, followed by the 4-byte signature (in network byte order).  
6. If your signature is correct, I will respond with a secret port number. Good luck!  
7. Remember to keep your group ID and signature for later, you will need them for other ports.


