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
