<div style="display: flex; justify-content: space-between; margin-bottom: 20px;">
  <div style="font-size: 24px; font-weight: bold;">Chandransh Singh</div>
  <div style="font-size: 24px; font-weight: bold;">22CS30017</div>
</div>


# CLDP (Custom Lightweight Discovery Protocol)


## Build and run instructions
- I have provided a makefile
1. to run cldp_server in a terminal
```bash
make rs
```
2. to run cldp_client in other terminal
```bash
make rc
```
> We are using raw sockets and need root(`sudo`) privileges
---

## Demo output: 
### 1. CLDP server 1
```plaintext
gcc -Wall -o s cldp_server.c
> sudo ./s
+++ CLDP Server running...
<== Broadcast HELLO sent.
<-- Sent RESPONSE to 192.168.64.8 (trans_id 34683)
<== Broadcast HELLO sent.
<== Broadcast HELLO sent.
<-- Sent RESPONSE to 192.168.64.8 (trans_id 17901)
<== Broadcast HELLO sent.
```

### 2. CLDP server 2
```plaintext
gcc -Wall -o s cldp_server.c
> sudo ./s
+++ CLDP Server running...
<== Broadcast HELLO sent.
<-- Sent RESPONSE to 192.168.64.8 (trans_id 53624)
<== Broadcast HELLO sent.
<== Broadcast HELLO sent.
```

### 3. CLDP client
```plaintext
gcc -Wall -o c cldp_client.c
> sudo ./c
+++ CLDP Client running...
~~~ Listening for HELLO messages: 7 seconds remaining....
==> Received HELLO from 192.168.64.8
+++ Added new server: 192.168.64.8
~~~ Listening for HELLO messages: 1 seconds remaining...
+++ Found 1 new servers during HELLO listening.
Querying 1 active servers...
>>> Select metadata to request (enter y/n for each option):
Request hostname? (y/n): y
Request system time? (y/n): y
Request CPU load? (y/n): n
<-- Sent QUERY (trans_id 34683) to 192.168.64.8

--> Received RESPONSE from 192.168.64.8:
Hostname: moonserver
Time: 2025-03-31 17:49:53

:D Query complete. [1/1] servers responded.

Press Enter to repeat the process or type 'exit' to quit: 
~~~ Listening for HELLO messages: 10 seconds remaining...
==> Received HELLO from 192.168.64.8
~~~ Listening for HELLO messages: 3 seconds remaining....
==> Received HELLO from 192.168.64.8
==> Received HELLO from 192.168.132.4
+++ Added new server: 192.168.64.8
~~~ Listening for HELLO messages: 1 seconds remaining...
+++ Found 1 new servers during HELLO listening.
Querying 2 active servers...
>>> Select metadata to request (enter y/n for each option):
Request hostname? (y/n): y
Request system time? (y/n): y
Request CPU load? (y/n): y
<-- Sent QUERY (trans_id 17901) to 192.168.64.8

<-- Sent QUERY (trans_id 53624) to 192.168.132.4

--> Received RESPONSE from 192.168.64.8:
Hostname: moonserver
Time: 2025-03-31 17:50:08
CPU Load: 0.24

--> Received RESPONSE from 192.168.132.4:
Hostname: gaurav-roy-HP-Laptop-15-da0xxx
Time: 2025-03-31 17:50:08
CPU Load: 0.80

:D Query complete. [2/2] servers responded.

Press Enter to repeat the process or type 'exit' to quit: exit
~/Desktop >
```
