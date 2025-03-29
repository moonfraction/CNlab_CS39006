## Build and run instructions

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
### 1. CLDP server
```plaintext
gcc -Wall -o s cldp_server.c
sudo ./s
+++ CLDP Server running...
<== Broadcast HELLO sent.
<-- Sent RESPONSE to 127.0.0.1 (trans_id 2824)
<== Broadcast HELLO sent.
<== Broadcast HELLO sent.
<-- Sent RESPONSE to 127.0.0.1 (trans_id 18260)
<== Broadcast HELLO sent.
```

### 2. CLDP client
```plaintext
gcc -Wall -o c cldp_client.c
sudo ./c
+++ CLDP Client running...
~~~ Listening for HELLO messages: 6 seconds remaining....
==> Received HELLO from 127.0.0.1
+++ Added new server: 127.0.0.1
~~~ Listening for HELLO messages: 1 seconds remaining...
+++ Found 1 new servers during HELLO listening.
Querying 1 active servers...
>>> Select metadata to request (enter y/n for each option):
Request hostname? (y/n): y
Request system time? (y/n): y
Request CPU load? (y/n): y
<-- Sent QUERY (trans_id 2824) to 127.0.0.1

--> Received RESPONSE from 127.0.0.1:
Hostname: moonserver
Time: 2025-03-29 15:05:48
CPU Load: 0.43

:D Query complete. [1/1] servers responded.

Press Enter to repeat the process or type 'exit' to quit: 
~~~ Listening for HELLO messages: 10 seconds remaining...
==> Received HELLO from 127.0.0.1
~~~ Listening for HELLO messages: 4 seconds remaining....
==> Received HELLO from 127.0.0.1
~~~ Listening for HELLO messages: 1 seconds remaining...
`` `No new servers found during HELLO listening.
Querying 1 active servers...
>>> Select metadata to request (enter y/n for each option):
Request hostname? (y/n): y
Request system time? (y/n): y
Request CPU load? (y/n): n
<-- Sent QUERY (trans_id 18260) to 127.0.0.1

--> Received RESPONSE from 127.0.0.1:
Hostname: moonserver
Time: 2025-03-29 15:06:08

:D Query complete. [1/1] servers responded.

Press Enter to repeat the process or type 'exit' to quit: exit
```
