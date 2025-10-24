#include <omnetpp.h>
#include <string>
#include "helpers.h"

using namespace omnetpp;
using std::string;

class Device : public cSimpleModule {
  private:
    string devType;
    string devName;
    int    priority;
    string ip6;
    double leaseTime;
    cMessage *startEvt;
    cMessage *renewEvt;

  protected:
    virtual void initialize() override {
        devType  = par("type").stringValue();
        devName  = par("name").stringValue();
        priority = par("priority").intValue();
        startEvt = nullptr;
        renewEvt = nullptr;
        ip6 = "";
        leaseTime = 0;

        startEvt = new cMessage("start");
        scheduleAt(simTime() + par("startJitter").doubleValue(), startEvt);

        EV_INFO << "[" << simTime() << "] "
                << devName << " (" << devType << ", prio=" << priority << ") ready.\n";
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg == startEvt) {
            auto *sol = mk("DHCPV6_SOLICIT", DHCPV6_SOLICIT, getId(), 0);
            sol->addPar("type").setStringValue(devType.c_str());
            sol->addPar("priority").setLongValue(priority);
            send(sol, "ppp$o");
        }
        else if (msg == renewEvt) {
            EV_INFO << "[" << simTime() << "] " << devName
                    << " lease expired, sending RENEW for " << ip6 << "\n";
            auto *ren = mk("DHCPV6_RENEW", DHCPV6_RENEW, getId(), 0);
            ren->addPar("ip6").setStringValue(ip6.c_str());
            ren->addPar("priority").setLongValue(priority);
            send(ren, "ppp$o");
        }
        else {
            switch (msg->getKind()) {
                case DHCPV6_ADVERTISE: {
                    string offer = msg->par("ip6").stringValue();
                    leaseTime = msg->par("leaseTime").doubleValue();
                    EV_INFO << "[" << simTime() << "] " << devName
                            << " got ADVERTISE: " << offer
                            << " (lease: " << leaseTime << "s)\n";
                    auto *req = mk("DHCPV6_REQUEST", DHCPV6_REQUEST, getId(), 0);
                    req->addPar("ip6").setStringValue(offer.c_str());
                    req->addPar("priority").setLongValue(priority);
                    send(req, "ppp$o");
                    break;
                }
                case DHCPV6_REPLY: {
                    ip6 = msg->par("ip6").stringValue();
                    leaseTime = msg->par("leaseTime").doubleValue();
                    EV_INFO << "[" << simTime() << "] " << devName
                            << " configured IPv6: " << ip6
                            << " (lease: " << leaseTime << "s)\n";

                    // Schedule lease renewal before expiry
                    if (!renewEvt) {
                        renewEvt = new cMessage("renew");
                    }
                    scheduleAt(simTime() + leaseTime, renewEvt);
                    break;
                }
                default:
                    EV_WARN << devName << " unexpected kind=" << msg->getKind() << "\n";
                    break;
            }
        }
        if (msg != startEvt && msg != renewEvt) {
            delete msg;
        }
    }

    virtual void finish() override {
        if (startEvt) {
            if (startEvt->isScheduled())
                cancelEvent(startEvt);
            delete startEvt;
            startEvt = nullptr;
        }
        if (renewEvt) {
            if (renewEvt->isScheduled())
                cancelEvent(renewEvt);
            delete renewEvt;
            renewEvt = nullptr;
        }
        EV_INFO << devName << " final IPv6 = " << ip6 << "\n";
    }
};

Define_Module(Device);
