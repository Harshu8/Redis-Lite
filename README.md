# Redis-Lite

A Redis-inspired in-memory key-value datastore built in modern C++ with support for:

- TCP socket server
- Concurrent client handling
- TTL expiration
- LRU eviction
- Append-Only File (AOF) persistence
- Crash recovery
- Thread pool architecture

This project was built to explore backend systems engineering concepts including networking, concurrency, synchronization, persistence, and cache/database internals.

---

# Features

## Core Commands

```txt
SET key value
GET key
DELETE key
EXIT
```

---

## TTL Expiration

Supports automatic key expiration.

Example:

```txt
SET otp 1234 EX 10
```

Key expires after 10 seconds.

---

## LRU Eviction

Implements Least Recently Used eviction when capacity is exceeded.

Uses:
- `unordered_map`
- doubly linked list (`std::list`)

---

## TCP Socket Server

Clients connect using:
- `telnet`
- `netcat`
- custom TCP clients

Runs on:

```txt
localhost:6379
```

---

## Multithreaded Thread Pool

Implements:
- producer-consumer architecture
- worker thread pool
- mutex synchronization
- condition variables

Avoids:
- thread-per-client overhead
- excessive context switching
- high memory consumption

---

## AOF Persistence

Supports Append-Only File persistence.

All mutating commands are logged to:

```txt
appendonly.aof
```

Example:

```txt
SET name Harsh
DELETE city
SET otp 1234 EX 10
```

On restart:
- commands are replayed
- memory state is recovered

---

# Architecture

```txt
                +-------------------+
Client -------->| TCP Socket Server |
                +---------+---------+
                          |
                          v
                +-------------------+
                | Thread Pool       |
                +-------------------+
                          |
                          v
                +-------------------+
                | Command Processor |
                +-------------------+
                          |
             +------------+------------+
             |                         |
             v                         v
+----------------------+    +----------------------+
| In-memory KV Store   |    | Append Only File     |
| unordered_map + LRU  |    | appendonly.aof       |
+----------------------+    +----------------------+
```

---

# Technologies Used

- C++17
- POSIX sockets
- std::thread
- mutex
- condition_variable
- unordered_map
- chrono
- file I/O
- CMake

---

# Build Instructions

## Clone Repository

```bash
git clone <your_repo_url>
cd redis-lite
```

---

## Build

```bash
mkdir -p build
cd build
cmake ..
make
```

---

## Run Server

```bash
./redis_lite
```

Expected output:

```txt
Recovered 0 commands from appendonly.aof
Redis-lite server running on port 6379...
AOF persistence enabled: appendonly.aof
```

---

# Client Testing

Open another terminal:

```bash
nc localhost 6379
```

or

```bash
telnet localhost 6379
```

---

# Supported Commands

## SET

```txt
SET name Harsh
```

Response:

```txt
OK
```

---

## GET

```txt
GET name
```

Response:

```txt
Harsh
```

---

## DELETE

```txt
DELETE name
```

Response:

```txt
Deleted
```

---

## TTL Example

```txt
SET otp 1234 EX 5
```

Wait 5 seconds:

```txt
GET otp
```

Response:

```txt
(nil)
```

---

# Persistence Example

## Step 1

```txt
SET city Delhi
SET user Harsh
```

---

## Step 2

Stop server.

---

## Step 3

Restart server.

---

## Step 4

```txt
GET city
GET user
```

Recovered automatically from:

```txt
appendonly.aof
```