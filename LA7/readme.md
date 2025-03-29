[A Guide to Using Raw Sockets](https://www.opensourceforu.com/2015/03/a-guide-to-using-raw-sockets/)

```plaintext
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Message Type |  Payload Len  |        Transaction ID         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Reserved                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Reserved                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

## CLDP Header Format (12 bytes)

- Message Type (1 byte):
    - 0x01: HELLO
    - 0x02: QUERY
    - 0x03: RESPONSE

- Payload Length (1 byte): Length of the payload in bytes

- Transaction ID (2 bytes): Unique identifier for matching queries and responses

- Reserved (8 bytes): Reserved for future use, set to 0

## Message Types and Payloads
- HELLO Message (Type 0x01)
    - No payload required
    - Used to announce node presence

- QUERY Message (Type 0x02)
    - Payload (1 byte): Query type
        - 0x01: Request hostname
        - 0x02: Request system time
        - 0x03: Request CPU load

- RESPONSE Message (Type 0x03)
    - Payload format: [Query Type (1 byte)][Response Data (variable)]
    - Response data format depends on the query type:
        - Hostname: Null-terminated string
        - System time: 8-byte timestamp (seconds since epoch)
        - CPU load: 4-byte float representing load average


---
---

Custom Lightweight Discovery Protocol (CLDP)
===============================================

1. Overview
-----------
CLDP is a custom discovery protocol designed for a closed network environment where nodes announce their presence and exchange metadata. The protocol runs over raw IP packets (using protocol number 253) and does not use any transport-layer protocols such as TCP or UDP.

2. Packet Format
----------------
Each CLDP packet consists of:
  A. IP Header (constructed manually by the sender with IP_HDRINCL enabled)
     - Version, IHL, TOS, Total Length, Identification, Flags/Fragment Offset,
       TTL, Protocol (set to 253), Header Checksum, Source and Destination Addresses.
       
  B. CLDP Header (8 bytes minimum):
     ------------------------------------------------------
     | Field             | Size (bytes) | Description    |
     ------------------------------------------------------
     | Message Type      | 1            | 0x01 = HELLO   |
     |                   |              | 0x02 = QUERY   |
     |                   |              | 0x03 = RESPONSE|
     ------------------------------------------------------
     | Reserved          | 1            | Reserved for future use   |
     ------------------------------------------------------
     | Payload Length    | 2            | Length of the payload     |
     ------------------------------------------------------
     | Transaction ID    | 4            | Unique ID for matching    |
     |                   |              | requests and responses    |
     ------------------------------------------------------
     
  C. Payload (Optional; based on message type)
     - HELLO: May be empty.
     - QUERY: Contains a 1-byte query mask indicating which metadata are requested.
              (Bitwise flags: 0x01 = Hostname, 0x02 = System Time, 0x04 = CPU Load)
     - RESPONSE: Contains a UTF-8 string with the metadata formatted as:
              "Hostname: <hostname>; System Time: <timestamp>; CPU Load: <load>"
              
3. Message Types
----------------
- **0x01 – HELLO:**  
  Used by servers to announce that they are active. These packets are broadcast periodically (every 10 seconds).

- **0x02 – QUERY:**  
  Used by a client to request metadata from active nodes. For the demo, the client requests the hostname and system time.
  
- **0x03 – RESPONSE:**  
  Used by servers to respond with the requested metadata.

4. Supported Metadata (for this implementation)
-------------------------------------------------
This implementation supports at least the following three metadata items:
  - **Hostname:** Obtained via gethostname()
  - **System Time:** Retrieved using gettimeofday() and formatted as a readable timestamp.
  - **CPU Load:** Obtained using getloadavg() (average over 1 minute).

5. Implementation Details
-------------------------
- **Raw Socket Programming:**  
  Both the client and server use raw sockets (`AF_INET`, `SOCK_RAW`) with the IP_HDRINCL option set so that the custom IP header is constructed manually.

- **Checksum Calculation:**  
  The IP header checksum is computed for outgoing packets.

- **Packet Crafting and Parsing:**  
  Outgoing packets are constructed manually by filling in the IP header and appending the CLDP header and payload. Incoming packets are filtered by protocol number (253) and parsed to extract the CLDP header and its payload.

- **Privileges:**  
  The programs must be run with elevated privileges (e.g., using sudo) because raw sockets require administrative permissions.

6. Demo Scenario
----------------
- **Server:**  
  - Listens on a raw socket.
  - Every 10 seconds, sends a HELLO packet (broadcast).
  - When receiving a QUERY packet, it responds with a RESPONSE packet containing the metadata.

- **Client:**  
  - Constructs and sends a QUERY packet (broadcast or to a specific IP) requesting the hostname and system time.
  - Listens for RESPONSE packets and displays the returned metadata.

End of Specification.

---

# Custom Lightweight Discovery Protocol (CLDP)

## Overview
This project implements a custom lightweight discovery protocol (CLDP) using raw sockets in C (POSIX standard). CLDP is designed for a closed network environment where nodes announce their presence and query each other for application-level metadata.

The protocol uses raw IP packets with a custom protocol number (253) and a custom header with the following format:
- **Message Type (1 byte):**  
  - 0x01: HELLO (announce node is active)
  - 0x02: QUERY (request metadata)
  - 0x03: RESPONSE (response with metadata)
- **Reserved (1 byte):** Reserved for future use.
- **Payload Length (2 bytes):** Length of the optional payload.
- **Transaction ID (4 bytes):** Unique identifier for matching requests and responses.

## Supported Metadata
The implementation supports three metadata queries:
- **Hostname:** Obtained via `gethostname()`.
- **System Time:** Retrieved using `gettimeofday()` and formatted as a human-readable timestamp.
- **CPU Load:** Obtained via `getloadavg()`.

## Files
- `cldp_spec.pdf` (or text): Protocol specification document.
- `cldp_server.c`: Server source code.
- `cldp_client.c`: Client source code.
- `README.md`: Build, run instructions, assumptions, and demo output.

## Build Instructions
Compile the server and client using gcc:
```bash
gcc -o cldp_server cldp_server.c
gcc -o cldp_client cldp_client.c
```

---

Sure! The code snippet you provided is constructing an IPv4 header manually in C. Let me break it down step by step.

---

## **Understanding the Code**

### **1. Declaring and Casting the IP Header**
```c
struct iphdr *ip_hdr = (struct iphdr *)buffer;
```
- `buffer` is assumed to be a memory block where the IP header will be stored.
- The `struct iphdr` is a standard Linux structure representing an IPv4 header (found in `<linux/ip.h>` or `<netinet/ip.h>`).
- The `(struct iphdr *)buffer` casts the memory location to treat it as an `iphdr` structure.

---

### **2. Setting IP Header Fields**

#### **Internet Header Length (`ihl`)**
```c
ip_hdr->ihl = 5;
```
- `ihl` stands for **Internet Header Length**.
- The value `5` means **5 words of 4 bytes each**, totaling **20 bytes** (the standard IPv4 header size).
- `ihl` is measured in **32-bit words**, so `5 * 4 = 20` bytes.

#### **IP Version (`version`)**
```c
ip_hdr->version = 4;
```
- The IP version is **IPv4**.

#### **Type of Service (`tos`)**
```c
ip_hdr->tos = 0;
```
- `tos` (Type of Service) is used for Quality of Service (QoS).
- `0` means default service (no priority handling).

#### **Total Length (`tot_len`)**
```c
int total_len = sizeof(struct iphdr) + sizeof(cldp_header_t) + payload_len;
ip_hdr->tot_len = htons(total_len);
```
- The **total length** includes:
  - The IPv4 header (`sizeof(struct iphdr)`)
  - A **custom protocol header** (`sizeof(cldp_header_t)`, which is not defined here)
  - The **payload size** (`payload_len`).
- `htons()` ensures the value is converted to **network byte order** (big-endian).

#### **Identification (`id`)**
```c
ip_hdr->id = htons(rand() % 65535);
```
- `id` is used to identify fragmented packets.
- `rand() % 65535` assigns a random **packet identifier**.
- `htons()` converts it to **network byte order**.

#### **Fragmentation Offset (`frag_off`)**
```c
ip_hdr->frag_off = 0;
```
- This means **no fragmentation** (DF = 0, MF = 0).
- If fragmenting, this field would hold fragment offsets.

#### **Time to Live (`ttl`)**
```c
ip_hdr->ttl = 64;
```
- **TTL (Time to Live)** controls how many hops (routers) the packet can traverse before being discarded.
- `64` is a common default.

#### **Protocol (`protocol`)**
```c
ip_hdr->protocol = PROTOCOL_NUM;
```
- This specifies the **next protocol** (e.g., TCP, UDP, ICMP).
- `PROTOCOL_NUM` is a placeholder, which should be replaced by actual protocol values like:
  - `6` for TCP
  - `17` for UDP
  - `1` for ICMP

#### **Source Address (`saddr`)**
```c
ip_hdr->saddr = INADDR_ANY;
```
- The **source IP address** is set to `INADDR_ANY` (0.0.0.0).
- Normally, you would use a real source IP (`inet_addr("192.168.1.1")`).

#### **Destination Address (`daddr`)**
```c
ip_hdr->daddr = dest->sin_addr.s_addr;
```
- The destination IP (`daddr`) is copied from the `dest` struct (`struct sockaddr_in`).
- `dest->sin_addr.s_addr` holds the actual IP address.

---

### **3. Calculating the IP Header Checksum**
```c
ip_hdr->check = 0;
ip_hdr->check = checksum((unsigned short *)ip_hdr, ip_hdr->ihl * 2);
```
- The **checksum** is required for IPv4 headers.
- It first sets `check = 0` to clear any previous value.
- Then it calculates the checksum using the function `checksum()`.
- The checksum is computed over the **IP header only** (`ip_hdr->ihl * 2` because `ihl` is in 32-bit words).

---

## **Final Thoughts**
This code constructs a **raw IPv4 header** for **custom networking applications**, such as:
- **Raw socket programming** (sending custom packets).
- **Creating a new protocol** on top of IP.
- **Packet crafting and analysis** (e.g., in security testing).

Let me know if you need more details! 🚀