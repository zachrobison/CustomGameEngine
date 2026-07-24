#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Cross-platform TCP helper for LAN play — increment 3: N-player star relay.
// One host listens and accepts many clients; every frame it forwards to all the
// OTHER clients, so each player receives everyone's messages (a full mesh over a
// star). Messages are length-prefixed byte blobs and self-contained (they carry
// their own sender id), so the transport stays dumb — it never inspects payloads.
//
// Free-for-all model (host relay, peer-simulated): each player simulates their
// own units and broadcasts a small snapshot; damage is addressed by player id.
class Net {
public:
    ~Net();
    bool host(uint16_t port);                        // start listening
    bool join(const std::string& ip, uint16_t port); // connect to a host
    void close();

    bool isHost()    const { return hosting; }
    bool connected() const;                          // at least one live link
    int  peerCount() const;                          // live connections
    std::string status() const { return statusMsg; }

    void poll();                                     // accept + pump + relay
    void broadcast(const std::vector<uint8_t>& msg);  // frame + send to all peers
    bool recv(std::vector<uint8_t>& out);            // next inbound message, if any

    static std::string localIP();                    // this machine's LAN IP (for others to join)

private:
    struct Conn { int fd = -1; std::vector<uint8_t> rx; };
    bool hosting = false;
    int  listener = -1;
    std::vector<Conn> conns;                         // host: clients; client: [host]
    std::vector<std::vector<uint8_t>> inbox;         // decoded inbound messages
    std::string statusMsg = "offline";
    void setNonBlocking(int fd);
    void sendFrame(int fd, const std::vector<uint8_t>& msg);
    void pump(size_t idx, std::vector<std::vector<uint8_t>>& framesOut);
};
