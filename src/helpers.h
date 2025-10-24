#pragma once
#include <omnetpp.h>

#define DHCPV6_SOLICIT    601
#define DHCPV6_ADVERTISE  602
#define DHCPV6_REQUEST    603
#define DHCPV6_REPLY      604
#define DHCPV6_RENEW      605

inline omnetpp::cMessage* mk(const char* name, int kind, long src, long dst) {
    auto *m = new omnetpp::cMessage(name, kind);
    m->addPar("src").setLongValue(src);
    m->addPar("dst").setLongValue(dst);
    return m;
}
inline long SRC(omnetpp::cMessage *m) { return m->par("src").longValue(); }
inline long DST(omnetpp::cMessage *m) { return m->par("dst").longValue(); }
