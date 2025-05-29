# 🧠 **see-plus-plus** — C++ Sockets to ASCII Faces

> Built with pain, sockets, and a deep distrust of abstractions.
> 
> FOR HIRING MANAGERS, THIS PROJECT TEACHES WEB SOCKETS. THANK YOU. 😭

---

## 🚀 What is this?

**see-plus-plus** is what happens when you:
- get mad at Electron,
- hate Node.js,
- and want to understand the web by rebuilding it from scratch in C++.

It captures your webcam with `ffmpeg`, converts it to ASCII in real-time, and streams it through handcrafted WebSocket connections — directly into your browser.

---

## 🧱 Features (Eventually)

- Raw TCP server using POSIX sockets
- Fully hand-rolled HTTP request parsing
- WebSocket upgrade + frame parsing (no libraries)
- ASCII webcam frames sent over WebSockets
- Multi-client broadcasting
- Real-time chat
- Room-based isolation
- Terminal-style frontend (HTML + JS)

---

## 📁 Project Structure

```bash
.
├── server/           # C++ source files (sockets, HTTP, WS)
├── web/              # Static HTML + JS frontend
├── ascii/            # ASCII renderer + ffmpeg pipe reader
├── build/            # Build artifacts
├── README.md         # You're looking at it
└── LICENSE           # Just put MIT bro trust me
```

---

## 💻 How to Run (Eventually)

```bash
g++ server/main.cpp -o see-plus-plus
./see-plus-plus
```

Open browser to:  
```http://localhost:8080```

See your face, in ASCII, crying.

---

## 🧠 Why tho?

- Because you never really *get* WebSockets until you shift bits and parse frame lengths by hand.
- Because using ffmpeg as a subprocess to pipe video data is CHAD behaviour.
- Because doing this in C++ instead of Go or Node is ✨ deranged ✨ and we like that.
- I'm a masochist

---

## 🛣️ Roadmap

- [x] TCP server (echo)
- [ ] HTTP request parsing
- [ ] Static file serving
- [ ] WebSocket handshake
- [ ] Frame parser
- [ ] Chat broadcast
- [ ] ASCII video stream
- [ ] Frontend to display chaos
- [ ] Room/channel support
- [ ] Deployment script
- [ ] Monetize this??? (jk... unless?)

---

## ⚠️ Disclaimer

Made by [@mush1e](https://github.com/mush1e)  
Fueled by Monster, burned neurons, and a spite-fueled hatred of abstraction.

> [!CAUTION]
> Project may cause:
> - Carpal Tunnel Syndrome
> - Crippling Depression
> - Irrational desire to build a browser from scratch
