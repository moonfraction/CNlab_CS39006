# mysmtp

A simple SMTP-like mail transfer system implemented in C using sockets and threads. This project includes both a client (`mysmtp_client.c`) and a server (`mysmtp_server.c`).

---

## 📡 Communication Protocol

The protocol between the client and server follows a simplified version of SMTP. Communication is line-based, and the client sends commands to the server which responds with status codes and messages. The major commands supported are:

- `HELO <domain>`: Start session by identifying the client’s domain.
- `MAIL FROM: <email>`: Specify sender's email. The domain must match the one given in `HELO`.
- `RCPT TO: <email>`: Specify recipient's email.
- `DATA <message>`: Send message content. The message is saved to the recipient's mailbox.
- `LIST <email>`: List all emails sent to a recipient.
- `GET_MAIL <email> <id>`: Retrieve a specific email by ID for a recipient from the mailbox.
- `QUIT`: Ends the session.

Each server response follows the pattern:
```
<status_code> <message>
```

---

## Client States

The client’s session with the server transitions through several states:

1. **Initial (state = 0)**: Awaiting `HELO`. No other command is accepted until `HELO` is received.
2. **HELO received (state = 1)**: Now expects `MAIL FROM`.
3. **MAIL FROM received (state = 2)**: Now expects `RCPT TO`.
4. **RCPT TO received (state = 3)**: Now expects `DATA`.
5. **After DATA (reset to state = 0)**: Server resets the state; a new message flow begins with `HELO`.

Each state enforces an order, ensuring that the mail flow adheres to protocol rules.

---

## 🖥️ Server Architecture

- The server is **multi-threaded**, able to handle multiple clients simultaneously using `pthread`.
- On a new client connection, a dedicated thread is spawned to handle communication for that client.
- All incoming commands are parsed, and appropriate actions are taken based on the session state and input validation.

### Server Responsibilities:

1. **Session Management**:
   - Manages per-client session states.
   - Ensures protocol compliance and sequencing.
   
2. **Email Storage**:
   - Emails are stored in the `mailbox/` directory.
   - Each recipient has a separate file (`<recipient>.txt`) containing all received messages.

3. **Command Handling**:
   - `LIST` reads stored mails and shows metadata (sender and date).
   - `GET_MAIL` retrieves a specific email by ID.
   - `DATA` writes a new email to file after `RCPT TO`.

4. **Directory Initialization**:
   - On startup, ensures `mailbox/` directory exists.

5. **Response Format**:
   - Sends status code + message for every client request.

---

## How to Compile and Run

1. **Build**:
   ```bash
   make
   ```

2. **Run Server**:
   ```bash
   make rs
   # or manually:
   ./s <port>
   ```

3. **Run Client (in another terminal)**:
   ```bash
   make rc
   # or manually:
   ./c <server_ip> <port>
   ```

4. **Clean Build Artifacts**:
   ```bash
   make clean
   ```

5. **Clean All (including mailboxes)**:
   ```bash
   make deepclean
   ```
