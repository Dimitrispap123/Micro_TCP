# Micro_TCP - Custom Networking Protocol

## 📖 About the Project
**micro_tcp** is a lightweight **custom TCP-like protocol** implemented from scratch. It enables reliable **data transmission** over a network without using the standard POSIX TCP stack. The project simulates **packet handling, connection management, and error control** mechanisms.

This project was built as part of **Networking & Systems Programming coursework** to better understand low-level **network communication** and transport-layer protocols.

---

## 🔧 Features
- **Reliable Data Transmission**: Implements basic **acknowledgment (ACK)** and **retransmission** mechanisms.
- **Connection Management**: Handles **handshake**, **connection establishment**, and **teardown**.
- **Packet Segmentation & Reassembly**: Ensures correct delivery of fragmented packets.
- **Error Handling**: Detects packet loss and implements **timeout-based retransmission**.
- **Multi-Threaded Design**: Uses `pthread` for handling multiple clients simultaneously.
- **Socket Programming**: Implements **server and client communication** using raw sockets.

---

## 🛠️ Technologies Used
- **Language:** C
- **Networking:** Sockets, Custom Transport Layer Implementation
- **Concurrency:** pthreads (POSIX Threads)
- **Error Handling:** Timeout-based retransmission
- **Testing Tools:** Wireshark (for packet inspection)

---

## 🚀 Getting Started

### 📥 Prerequisites
Ensure you have:
- A **Linux-based system** (Ubuntu, Debian, etc.)
- **GCC Compiler** installed
- **Basic understanding of sockets and networking**

### 🛠️ Installation & Compilation
1. **Clone the repository:**
   ```sh
   git clone https://github.com/Dimitrispap123/micro_tcp.git
