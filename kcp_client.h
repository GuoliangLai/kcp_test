#ifndef KCP_CLIENT_H
#define KCP_CLIENT_H

#include "ikcp.h"
#include "kcp_inc.h"

class kcp_client
{ 
public:
    kcp_client(char *serIp, uint16_t serPort, uint16_t localPort);

    int sendData(const char *buffer, int len);
    bool m_isLoop;
//private:
    UdpDef *pUdpDef;
    ikcpcb *pkcp;
};

#endif // KCP_CLIENT_H