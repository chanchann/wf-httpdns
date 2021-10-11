#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>   
#include <arpa/inet.h>
#include <string>

static const int INET_ADDR_LEN = 128;  // 暂时只支持ipv4

std::string ipBinToString(void* sa_sin_addr) {
   struct sockaddr_in sa;
   char str[INET_ADDR_LEN];

   inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);

   return str;
}
