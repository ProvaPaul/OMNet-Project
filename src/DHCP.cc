#include <omnetpp.h>
#include <string>
#include <unordered_map>
#include <map>
#include <sstream>
#include <iomanip>
#include "helpers.h"

using namespace omnetpp;
using std::string;
using std::unordered_map;
using std::map;

// ============================================================================
// SWITCH
// ============================================================================
class Switch : public cSimpleModule {
  protected:
    virtual void handleMessage(cMessage *msg) override {
        int arrivalPort = msg->getArrivalGate()->getIndex();
        int portCount = gateSize("port");

        for (int i = 0; i < portCount; i++) {
            if (i != arrivalPort && gate("port$o", i)->isConnected()) {
                send(msg->dup(), "port$o", i);
            }
        }
        delete msg;
    }
};

Define_Module(Switch);

// ============================================================================
// DHCP SERVER
// ============================================================================
class DHCP : public cSimpleModule {
  private:
    string pcPrefix, mobilePrefix, printerPrefix, vipPrefix;
    double fastResponseDelay = 0.01;
    double normalResponseDelay = 0.02;
    int    vipPriorityCutoff = 9;

    map<string, uint64_t> nextIdForPool;
    unordered_map<long, string> addrTable;

    bool isPrimary;
    string partnerName;
    double syncInterval;
    double failoverTimeout;
    double failureTime;

    bool isActive = false;
    bool partnerAlive = true;
    simtime_t lastPartnerHeartbeat;

    cMessage *syncTimer = nullptr;
    cMessage *heartbeatTimer = nullptr;
    cMessage *checkPartnerTimer = nullptr;
    cMessage *failureEvent = nullptr;

    bool hasFailed = false;

    // Statistics
    int solicitsReceived = 0;
    int advertiseSent = 0;
    int requestsReceived = 0;
    int repliesSent = 0;

  protected:
    virtual void initialize() override {
        pcPrefix       = par("pcPrefix").stringValue();
        mobilePrefix   = par("mobilePrefix").stringValue();
        printerPrefix  = par("printerPrefix").stringValue();
        vipPrefix      = par("vipPrefix").stringValue();
        fastResponseDelay   = par("fastResponseDelay").doubleValue();
        normalResponseDelay = par("normalResponseDelay").doubleValue();
        vipPriorityCutoff   = par("vipPriorityCutoff").intValue();

        isPrimary = par("isPrimary").boolValue();
        partnerName = par("partnerName").stringValue();
        syncInterval = par("syncInterval").doubleValue();
        failoverTimeout = par("failoverTimeout").doubleValue();
        failureTime = par("failureTime").doubleValue();

        nextIdForPool[pcPrefix]      = 1;
        nextIdForPool[mobilePrefix]  = 1;
        nextIdForPool[printerPrefix] = 1;
        nextIdForPool[vipPrefix]     = 1;

        isActive = isPrimary;
        lastPartnerHeartbeat = simTime();

        syncTimer = new cMessage("syncTimer");
        heartbeatTimer = new cMessage("heartbeatTimer");
        checkPartnerTimer = new cMessage("checkPartnerTimer");

        scheduleAt(simTime() + syncInterval, syncTimer);
        scheduleAt(simTime() + 0.25, heartbeatTimer);
        scheduleAt(simTime() + failoverTimeout, checkPartnerTimer);

        if (failureTime > 0) {
            failureEvent = new cMessage("failureEvent");
            scheduleAt(simTime() + failureTime, failureEvent);
        }

        EV << "INFO: " << getFullName() << " initialized as "
           << (isPrimary ? "PRIMARY" : "BACKUP")
           << " server (active=" << isActive << ")\n";
        EV << "INFO:   Pools: pc=" << pcPrefix
           << ", mobile=" << mobilePrefix
           << ", printer=" << printerPrefix
           << ", VIP=" << vipPrefix << "\n";
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg == syncTimer) {
            if (!hasFailed) sendSync();
            scheduleAt(simTime() + syncInterval, syncTimer);
            return;
        }

        if (msg == heartbeatTimer) {
            if (!hasFailed) sendHeartbeat();
            scheduleAt(simTime() + 0.25, heartbeatTimer);
            return;
        }

        if (msg == checkPartnerTimer) {
            checkPartnerStatus();
            scheduleAt(simTime() + failoverTimeout, checkPartnerTimer);
            return;
        }

        if (msg == failureEvent) {
            simulateFailure();
            return;
        }

        if (hasFailed) {
            delete msg;
            return;
        }

        if (msg->getKind() == DHCP_HEARTBEAT) {
            lastPartnerHeartbeat = simTime();
            if (!partnerAlive) {
                partnerAlive = true;
            }
            delete msg;
            return;
        }

        if (msg->getKind() == DHCP_SYNC) {
            receiveSync(msg);
            delete msg;
            return;
        }

        if (msg->arrivedOn("ppp$i")) {
            handleDHCPMessage(msg);
        } else {
            delete msg;
        }
    }

    void handleDHCPMessage(cMessage *msg) {
        long dst = DST(msg);
        if (dst != 0 && dst != getId()) {
            delete msg;
            return;
        }

        if (!isActive) {
            delete msg;
            return;
        }

        if (msg->getKind() == DHCPV6_SOLICIT) {
            solicitsReceived++;
            long dev = SRC(msg);
            string devType = (msg->hasPar("type") ? msg->par("type").stringValue() : "pc");
            int prio = (msg->hasPar("priority") ? (int)msg->par("priority").longValue() : 1);

            bool isVip = isVipClient(devType, prio);
            string prefix = pickPrefix(devType, prio);
            string offer  = makeAddress(prefix, nextIdForPool[prefix]);

            nextIdForPool[prefix]++;

            EV << "INFO: [" << simTime() << "] " << getFullName()
               << " SOLICIT from devId=" << dev
               << " type=" << devType << " prio=" << prio
               << " -> ADVERTISE " << offer
               << (isVip ? " (VIP)" : " (normal)") << "\n";

            auto *adv = mk("DHCPV6_ADVERTISE", DHCPV6_ADVERTISE, getId(), dev);
            adv->addPar("ip6").setStringValue(offer.c_str());
            adv->addPar("serverName").setStringValue(getFullName());
            adv->addPar("serverId").setLongValue(getId());
            simtime_t d = isVip ? fastResponseDelay : normalResponseDelay;
            sendDelayed(adv, d, "ppp$o");
            advertiseSent++;
        }
        else if (msg->getKind() == DHCPV6_REQUEST) {
            requestsReceived++;
            long dev = SRC(msg);
            string ip6 = msg->par("ip6").stringValue();
            int prio = (msg->hasPar("priority") ? (int)msg->par("priority").longValue() : 1);

            addrTable[dev] = ip6;

            string key = poolKeyFrom(ip6);
            bool isVip = (key.find("vip::") != string::npos || key == vipPrefix);

            EV << "INFO: [" << simTime() << "] " << getFullName()
               << " REQUEST from devId=" << dev
               << " prio=" << prio << " for " << ip6
               << " -> REPLY " << (isVip ? "(VIP)" : "(normal)") << "\n";

            auto *rep = mk("DHCPV6_REPLY", DHCPV6_REPLY, getId(), dev);
            rep->addPar("ip6").setStringValue(ip6.c_str());
            rep->addPar("serverName").setStringValue(getFullName());
            simtime_t d = isVip ? fastResponseDelay : normalResponseDelay;
            sendDelayed(rep, d, "ppp$o");
            repliesSent++;
        }
        delete msg;
    }

    void sendSync() {
        if (!gate("syncOut")->isConnected()) return;

        auto *sync = new cMessage("DHCP_SYNC", DHCP_SYNC);
        sync->addPar("pcNext").setLongValue(nextIdForPool[pcPrefix]);
        sync->addPar("mobileNext").setLongValue(nextIdForPool[mobilePrefix]);
        sync->addPar("printerNext").setLongValue(nextIdForPool[printerPrefix]);
        sync->addPar("vipNext").setLongValue(nextIdForPool[vipPrefix]);
        sync->addPar("isActive").setBoolValue(isActive);

        std::stringstream leaseData;
        for (const auto& entry : addrTable) {
            leaseData << entry.first << ":" << entry.second << ";";
        }
        sync->addPar("leases").setStringValue(leaseData.str().c_str());

        send(sync, "syncOut");
    }

    void receiveSync(cMessage *msg) {
        uint64_t pcNext = msg->par("pcNext").longValue();
        uint64_t mobNext = msg->par("mobileNext").longValue();
        uint64_t prnNext = msg->par("printerNext").longValue();
        uint64_t vipNext = msg->par("vipNext").longValue();

        if (pcNext > nextIdForPool[pcPrefix])
            nextIdForPool[pcPrefix] = pcNext;
        if (mobNext > nextIdForPool[mobilePrefix])
            nextIdForPool[mobilePrefix] = mobNext;
        if (prnNext > nextIdForPool[printerPrefix])
            nextIdForPool[printerPrefix] = prnNext;
        if (vipNext > nextIdForPool[vipPrefix])
            nextIdForPool[vipPrefix] = vipNext;

        if (msg->hasPar("leases")) {
            string leaseStr = msg->par("leases").stringValue();
            if (!leaseStr.empty()) {
                std::stringstream ss(leaseStr);
                string entry;
                while (std::getline(ss, entry, ';')) {
                    if (entry.empty()) continue;
                    size_t colon = entry.find(':');
                    if (colon != string::npos) {
                        long devId = std::stol(entry.substr(0, colon));
                        string addr = entry.substr(colon + 1);
                        addrTable[devId] = addr;
                    }
                }
            }
        }
    }

    void sendHeartbeat() {
        if (!gate("syncOut")->isConnected()) return;
        auto *hb = new cMessage("DHCP_HEARTBEAT", DHCP_HEARTBEAT);
        send(hb, "syncOut");
    }

    void checkPartnerStatus() {
        simtime_t elapsed = simTime() - lastPartnerHeartbeat;

        if (elapsed > failoverTimeout && partnerAlive) {
            EV << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
            EV << "WARN: [" << simTime() << "] " << getFullName()
               << " PARTNER FAILURE DETECTED!\n";
            EV << "      Last heartbeat was " << elapsed << "s ago\n";
            EV << "      Taking over as ACTIVE server...\n";
            EV << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n";
            partnerAlive = false;

            if (!isActive) {
                isActive = true;
                EV << "===================================================\n";
                EV << "INFO: [" << simTime() << "] " << getFullName()
                   << " is now ACTIVE\n";
                EV << "      Failover complete - ready to serve requests\n";
                EV << "===================================================\n\n";
            }
        }
    }

    void simulateFailure() {
        EV << "\n###################################################\n";
        EV << "WARN: [" << simTime() << "] *** " << getFullName()
           << " SIMULATING SERVER FAILURE ***\n";
        EV << "      Server is going DOWN\n";
        EV << "      Backup should take over within " << failoverTimeout << "s\n";
        EV << "###################################################\n\n";
        hasFailed = true;
        isActive = false;

        cancelEvent(syncTimer);
        cancelEvent(heartbeatTimer);
        cancelEvent(checkPartnerTimer);
    }

    bool isVipClient(const string& type, int prio) const {
        if (type == "server" || type == "router") return true;
        if (prio >= vipPriorityCutoff) return true;
        return false;
    }

    string pickPrefix(const string& type, int prio) const {
        if (isVipClient(type, prio)) return vipPrefix;
        if (type == "pc")      return pcPrefix;
        if (type == "mobile")  return mobilePrefix;
        if (type == "printer") return printerPrefix;
        return pcPrefix;
    }

    string makeAddress(const string& prefixWithLen, uint64_t counter) const {
        auto slash = prefixWithLen.find('/');
        string pref = (slash==string::npos) ? prefixWithLen : prefixWithLen.substr(0, slash);
        if (pref.size() < 2 || pref.substr(pref.size()-2) != "::") {
            if (!pref.empty() && pref.back() == ':') pref += ":";
            else pref += "::";
        }
        return pref + std::to_string(counter);
    }

    string poolKeyFrom(const string& ip6) const {
        auto hasPref = [&](const string& pfx) -> bool {
            auto slash = pfx.find('/');
            string base = (slash==string::npos) ? pfx : pfx.substr(0, slash);
            return ip6.rfind(base, 0) == 0;
        };
        if (hasPref(vipPrefix))     return vipPrefix;
        if (hasPref(pcPrefix))      return pcPrefix;
        if (hasPref(mobilePrefix))  return mobilePrefix;
        if (hasPref(printerPrefix)) return printerPrefix;
        return string();
    }

    virtual void finish() override {
        if (syncTimer) cancelAndDelete(syncTimer);
        if (heartbeatTimer) cancelAndDelete(heartbeatTimer);
        if (checkPartnerTimer) cancelAndDelete(checkPartnerTimer);
        if (failureEvent && failureEvent->isScheduled())
            cancelAndDelete(failureEvent);

        EV << "\n";
        EV << "========================================\n";
        EV << "DHCP SERVER STATISTICS: " << getFullName() << "\n";
        EV << "========================================\n";
        EV << "Status           : " << (isActive ? "ACTIVE" : "STANDBY") << "\n";
        EV << "Failed           : " << (hasFailed ? "YES" : "NO") << "\n";
        EV << "Partner Status   : " << (partnerAlive ? "ALIVE" : "DOWN") << "\n";
        EV << "----------------------------------------\n";
        EV << "SOLICIT received : " << solicitsReceived << "\n";
        EV << "ADVERTISE sent   : " << advertiseSent << "\n";
        EV << "REQUEST received : " << requestsReceived << "\n";
        EV << "REPLY sent       : " << repliesSent << "\n";
        EV << "Total Leases     : " << addrTable.size() << "\n";
        EV << "========================================\n";

        // Show assigned IPs
        if (!addrTable.empty()) {
            EV << "ASSIGNED IP ADDRESSES:\n";
            for (const auto& entry : addrTable) {
                EV << "  DeviceID " << entry.first << " -> " << entry.second << "\n";
            }
            EV << "========================================\n";
        }
        EV << "\n";
    }
};

Define_Module(DHCP);
