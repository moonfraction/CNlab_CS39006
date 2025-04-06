## 🧩 `EAGAIN` and `EWOULDBLOCK`

### 🔸 **What are they?**
Both are **error codes** returned by system calls (like `recv()`, `accept()`, `read()`, etc.) when:

> **The operation would block**, but the socket is **non-blocking**.

---

### 📌 **When do they occur?**

If you set a socket to **non-blocking mode**, and:

- `accept()` is called but no incoming connection is ready  
- `recv()` is called but no data is available  
- `send()` is called but the send buffer is full  

Then, instead of blocking, the function returns **-1** and sets `errno` to `EAGAIN` or `EWOULDBLOCK`.

---

### 🤝 Are they the same?

Yes — on most systems:

```c
#define EWOULDBLOCK EAGAIN
```

But technically:
- `EAGAIN` originally meant "try again later"
- `EWOULDBLOCK` was introduced to clearly mean “this would block”

---

### 🧠 How to handle them?

```c
ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);
if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No data available now; try again later
    } else {
        perror("recv");
    }
}
```

You **don’t treat this as an error** — just **retry later** (often using `select()`, `poll()`, or `epoll`).

---

## ⚠️ Other Important Socket-Related Error Codes

| **Error Code**     | **Meaning** |
|--------------------|-------------|
| `ECONNREFUSED`     | Connection attempt was refused by the server |
| `ETIMEDOUT`        | Connection timed out |
| `ENOTCONN`         | Socket is not connected (e.g., calling `send()` without `connect()`) |
| `EPIPE`            | Trying to write to a closed socket |
| `ECONNRESET`       | Connection was forcibly closed by the peer |
| `EADDRINUSE`       | Address/port is already in use (common during `bind()`) |
| `EADDRNOTAVAIL`    | Invalid IP address or interface |
| `ENETUNREACH`      | Network unreachable |
| `EHOSTUNREACH`     | Host unreachable |

---

## ⚙️ Useful Socket Options (`setsockopt()`)

| **Option**             | **Level**         | **Description** |
|------------------------|------------------|------------------|
| `SO_REUSEADDR`         | `SOL_SOCKET`     | Reuse local address (helps avoid "Address already in use" errors) |
| `SO_KEEPALIVE`         | `SOL_SOCKET`     | Enable TCP keepalive messages |
| `SO_RCVBUF` / `SO_SNDBUF` | `SOL_SOCKET` | Set receive/send buffer sizes |
| `SO_LINGER`            | `SOL_SOCKET`     | Control socket behavior on `close()` |
| `SO_BROADCAST`         | `SOL_SOCKET`     | Allow sending to broadcast addresses (for UDP) |
| `TCP_NODELAY`          | `IPPROTO_TCP`    | Disable Nagle’s algorithm (send small packets immediately) |
| `O_NONBLOCK`           | via `fcntl()`    | Make socket non-blocking |

---

## ✅ Example: Setting a socket non-blocking

```c
#include <fcntl.h>

int flags = fcntl(sockfd, F_GETFL, 0);
fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
```

Now you can handle `EAGAIN` or `EWOULDBLOCK` properly without blocking your app.

---

Let me know if you want a cheat sheet or example code using `select()` or `epoll` to avoid blocking!