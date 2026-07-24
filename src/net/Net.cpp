#include "Net.h"
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  #define NET_CLOSE(s) closesocket(s)
  #define NET_ERRNO   WSAGetLastError()
  #define NET_WOULDBLOCK WSAEWOULDBLOCK
  static bool wsaInit(){ static bool done=false; if(!done){ WSADATA w; WSAStartup(MAKEWORD(2,2),&w); done=true; } return true; }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #define NET_CLOSE(s) ::close(s)
  #define NET_ERRNO   errno
  #define NET_WOULDBLOCK EWOULDBLOCK
  static bool wsaInit(){ return true; }
#endif

Net::~Net(){ close(); }

void Net::setNonBlocking(int fd){
#if defined(_WIN32)
    u_long m=1; ioctlsocket(fd,FIONBIO,&m);
#else
    int fl=fcntl(fd,F_GETFL,0); fcntl(fd,F_SETFL,fl|O_NONBLOCK);
#endif
    int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,(const char*)&one,sizeof(one));
}

bool Net::host(uint16_t port){
    wsaInit(); close();
    listener=(int)socket(AF_INET,SOCK_STREAM,0);
    if(listener<0){ statusMsg="socket failed"; return false; }
    int one=1; setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,(const char*)&one,sizeof(one));
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY; a.sin_port=htons(port);
    if(bind(listener,(sockaddr*)&a,sizeof(a))<0){ statusMsg="bind failed"; NET_CLOSE(listener); listener=-1; return false; }
    if(listen(listener,8)<0){ statusMsg="listen failed"; NET_CLOSE(listener); listener=-1; return false; }
    setNonBlocking(listener); hosting=true;
    char buf[48]; snprintf(buf,sizeof(buf),"hosting on port %d — waiting", port); statusMsg=buf;
    return true;
}

bool Net::join(const std::string& ip, uint16_t port){
    wsaInit(); close();
    int fd=(int)socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){ statusMsg="socket failed"; return false; }
    sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(port);
    if(inet_pton(AF_INET,ip.c_str(),&a.sin_addr)<=0){ statusMsg="bad IP"; NET_CLOSE(fd); return false; }
    if(connect(fd,(sockaddr*)&a,sizeof(a))<0){ statusMsg="connect failed"; NET_CLOSE(fd); return false; }
    setNonBlocking(fd); hosting=false;
    conns.clear(); Conn c; c.fd=fd; conns.push_back(std::move(c));
    statusMsg="connected to host";
    return true;
}

void Net::close(){
    for(auto&c:conns) if(c.fd>=0) NET_CLOSE(c.fd);
    conns.clear();
    if(listener>=0){ NET_CLOSE(listener); listener=-1; }
    hosting=false; inbox.clear(); statusMsg="offline";
}

bool Net::connected() const { return !conns.empty(); }
int  Net::peerCount() const { return (int)conns.size(); }

void Net::sendFrame(int fd, const std::vector<uint8_t>& msg){
    if(fd<0) return;
    uint32_t len=(uint32_t)msg.size();
    uint8_t hdr[4]={ (uint8_t)(len>>24),(uint8_t)(len>>16),(uint8_t)(len>>8),(uint8_t)len };
    ::send(fd,(const char*)hdr,4,0);
    if(len) ::send(fd,(const char*)msg.data(),(int)len,0);
}

// Read available bytes for conns[idx] and pull out every complete frame.
// Returns true if the connection is still alive; false if it closed.
void Net::pump(size_t idx, std::vector<std::vector<uint8_t>>& framesOut){
    Conn& c=conns[idx];
    uint8_t tmp[4096];
    for(;;){
        int n=(int)::recv(c.fd,(char*)tmp,sizeof(tmp),0);
        if(n>0){ c.rx.insert(c.rx.end(),tmp,tmp+n); }
        else if(n==0){ NET_CLOSE(c.fd); c.fd=-1; break; }          // peer left
        else { if(NET_ERRNO!=NET_WOULDBLOCK){ NET_CLOSE(c.fd); c.fd=-1; } break; }
    }
    // Extract as many whole frames as we have.
    size_t off=0;
    while(c.rx.size()-off>=4){
        uint32_t len=((uint32_t)c.rx[off]<<24)|((uint32_t)c.rx[off+1]<<16)|((uint32_t)c.rx[off+2]<<8)|c.rx[off+3];
        if(c.rx.size()-off<4+(size_t)len) break;
        framesOut.emplace_back(c.rx.begin()+off+4, c.rx.begin()+off+4+len);
        off+=4+len;
    }
    if(off) c.rx.erase(c.rx.begin(), c.rx.begin()+off);
}

void Net::poll(){
    // Host: accept every pending client.
    if(hosting && listener>=0){
        for(;;){
            int cfd=(int)accept(listener,nullptr,nullptr);
            if(cfd<0) break;
            setNonBlocking(cfd); Conn c; c.fd=cfd; conns.push_back(std::move(c));
            char buf[48]; snprintf(buf,sizeof(buf),"%d player(s) connected",(int)conns.size()); statusMsg=buf;
        }
    }
    // Pump each connection, collecting frames tagged with their source index so
    // the host can relay them to everyone else.
    for(size_t i=0;i<conns.size();i++){
        if(conns[i].fd<0) continue;
        std::vector<std::vector<uint8_t>> frames;
        pump(i, frames);
        for(auto& f:frames){
            inbox.push_back(f);                        // deliver locally
            if(hosting){                                // relay to the other clients
                for(size_t j=0;j<conns.size();j++)
                    if(j!=i && conns[j].fd>=0) sendFrame(conns[j].fd,f);
            }
        }
    }
    // Drop dead connections.
    for(size_t i=conns.size();i-->0;)
        if(conns[i].fd<0) conns.erase(conns.begin()+i);
    if(conns.empty() && !hosting) statusMsg="disconnected";
}

void Net::broadcast(const std::vector<uint8_t>& msg){
    for(auto& c:conns) if(c.fd>=0) sendFrame(c.fd,msg);
}

bool Net::recv(std::vector<uint8_t>& out){
    if(inbox.empty()) return false;
    out=std::move(inbox.front());
    inbox.erase(inbox.begin());
    return true;
}
