#include <omnetpp.h>
#include <string>
#include "helpers.h"

using namespace omnetpp;
using std::string;

class Device : public cSimpleModule {
  private:
    string devType;
    string devName;
    int    priority = 1;
    string ip6;
    long   chosenServerId = 0;
    cMessage *startEvt = nullptr;

    // Statistics
    int solicitsSent = 0;
    int advertisesReceived = 0;
    int requestsSent = 0;
    int repliesReceived = 0;

    // For sequential processing
    bool dhcpCompleted = false;
    int deviceOrder = 0;  // Order in which device should start

  protected:
    virtual void initialize() override {
        devType  = par("type").stringValue();
        devName  = par("name").stringValue();
        priority = par("priority").intValue();

        // Determine device order based on priority (highest priority first)
        deviceOrder = 11 - priority;  // Server(10)=1, Router(9)=2, PC(3)=8, Mobile(2)=9, Printer(1)=10

        startEvt = new cMessage("start");

        // Sequential start: each device waits for previous to complete
        // Assume each DHCP transaction takes ~0.2s, add buffer
        double startDelay = (deviceOrder - 1) * 0.3;  // 0.3s between each device
        scheduleAt(simTime() + startDelay, startEvt);

        EV << "INFO: [" << simTime() << "] "
           << devName << " (" << devType << ", prio=" << priority
           << ", order=" << deviceOrder << ") ready. Will start at t="
           << (simTime() + startDelay) << "s\n";
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg->isSelfMessage()) {
            EV << "\n>>> [" << simTime() << "] " << devName
               << " STARTING DHCP PROCESS <<<\n";

            auto *sol = mk("DHCPV6_SOLICIT", DHCPV6_SOLICIT, getId(), 0);
            sol->addPar("type").setStringValue(devType.c_str());
            sol->addPar("priority").setLongValue(priority);
            send(sol, "ppp$o");
            solicitsSent++;

            EV << "INFO: [" << simTime() << "] " << devName
               << " sent SOLICIT (1/4)\n";

            delete msg;
            return;
        }

        long dst = DST(msg);
        if (dst != 0 && dst != getId()) {
            delete msg;
            return;
        }

        switch (msg->getKind()) {
            case DHCPV6_ADVERTISE: {
                advertisesReceived++;
                string offer = msg->par("ip6").stringValue();
                string serverName = msg->hasPar("serverName") ?
                                   msg->par("serverName").stringValue() : "unknown";

                if (msg->hasPar("serverId")) {
                    chosenServerId = msg->par("serverId").longValue();
                } else {
                    chosenServerId = SRC(msg);
                }

                EV << "INFO: [" << simTime() << "] " << devName
                   << " received ADVERTISE: " << offer
                   << " from " << serverName << " (2/4)\n";

                auto *req = mk("DHCPV6_REQUEST", DHCPV6_REQUEST, getId(), chosenServerId);
                req->addPar("ip6").setStringValue(offer.c_str());
                req->addPar("priority").setLongValue(priority);
                send(req, "ppp$o");
                requestsSent++;

                EV << "INFO: [" << simTime() << "] " << devName
                   << " sent REQUEST for " << offer << " (3/4)\n";
                break;
            }
            case DHCPV6_REPLY: {
                repliesReceived++;
                ip6 = msg->par("ip6").stringValue();
                string serverName = msg->hasPar("serverName") ?
                                   msg->par("serverName").stringValue() : "unknown";

                EV << "INFO: [" << simTime() << "] " << devName
                   << " received REPLY and configured IPv6: " << ip6
                   << " from " << serverName << " (4/4)\n";

                dhcpCompleted = true;

                EV << ">>> [" << simTime() << "] " << devName
                   << " DHCP PROCESS COMPLETED <<<\n\n";
                break;
            }
            default:
                break;
        }
        delete msg;
    }

    virtual void finish() override {
        if (startEvt && startEvt->isScheduled()) cancelAndDelete(startEvt);

        EV << "\n";
        EV << "========================================\n";
        EV << "DEVICE STATISTICS: " << devName << "\n";
        EV << "========================================\n";
        EV << "Type             : " << devType << "\n";
        EV << "Priority         : " << priority << "\n";
        EV << "Device Order     : " << deviceOrder << "\n";
        EV << "Assigned IPv6    : " << (ip6.empty() ? "NONE" : ip6) << "\n";
        EV << "----------------------------------------\n";
        EV << "SOLICIT sent     : " << solicitsSent << "\n";
        EV << "ADVERTISE recv   : " << advertisesReceived << "\n";
        EV << "REQUEST sent     : " << requestsSent << "\n";
        EV << "REPLY received   : " << repliesReceived << "\n";
        EV << "DHCP Completed   : " << (dhcpCompleted ? "YES" : "NO") << "\n";
        EV << "Status           : " << (ip6.empty() ? "FAILED" : "SUCCESS") << "\n";
        EV << "========================================\n";
        EV << "\n";
    }
};

Define_Module(Device);
