#include <omnetpp.h>
#include <string>
#include <unordered_map>
#include <map>
#include <queue>
#include <vector>
#include "helpers.h"

using namespace omnetpp;
using std::string;
using std::unordered_map;
using std::map;

struct PendingRequest {
    cMessage *msg;
    int gateIndex;
    int priority;
    simtime_t arrivalTime;

    bool operator<(const PendingRequest& other) const {
        if (priority != other.priority)
            return priority < other.priority;
        return arrivalTime > other.arrivalTime;
    }
};

class DHCP : public cSimpleModule {
  private:
    string pcPrefix, mobilePrefix, printerPrefix, vipPrefix;
    double fastResponseDelay;
    double normalResponseDelay;
    int    vipPriorityCutoff;
    double vipLeaseTime;
    double normalLeaseTime;

    map<string, uint64_t> nextIdForPool;
    unordered_map<long, string> addrTable;

    std::priority_queue<PendingRequest> solicitQueue;
    std::priority_queue<PendingRequest> requestQueue;

    cMessage *processSolicitEvent;
    cMessage *processRequestEvent;

    bool processingSolicit;
    bool processingRequest;

    // Statistics
    long solicitCount;
    long advertiseCount;
    long requestCount;
    long replyCount;
    long renewCount;
    simtime_t totalResponseTime;
    long responseCount;

    simsignal_t solicitSignal;
    simsignal_t advertiseSignal;
    simsignal_t requestSignal;
    simsignal_t replySignal;
    simsignal_t responseTimeSignal;

  protected:
    virtual void initialize() override {
        pcPrefix       = par("pcPrefix").stringValue();
        mobilePrefix   = par("mobilePrefix").stringValue();
        printerPrefix  = par("printerPrefix").stringValue();
        vipPrefix      = par("vipPrefix").stringValue();
        fastResponseDelay   = par("fastResponseDelay").doubleValue();
        normalResponseDelay = par("normalResponseDelay").doubleValue();
        vipPriorityCutoff   = par("vipPriorityCutoff").intValue();
        vipLeaseTime        = par("vipLeaseTime").doubleValue();
        normalLeaseTime     = par("normalLeaseTime").doubleValue();

        nextIdForPool[pcPrefix]      = 1;
        nextIdForPool[mobilePrefix]  = 1;
        nextIdForPool[printerPrefix] = 1;
        nextIdForPool[vipPrefix]     = 1;

        processSolicitEvent = new cMessage("processSolicit");
        processRequestEvent = new cMessage("processRequest");

        processingSolicit = false;
        processingRequest = false;

        // Initialize statistics
        solicitCount = 0;
        advertiseCount = 0;
        requestCount = 0;
        replyCount = 0;
        renewCount = 0;
        totalResponseTime = 0;
        responseCount = 0;

        // Register signals for statistics
        solicitSignal = registerSignal("solicitReceived");
        advertiseSignal = registerSignal("advertiseSent");
        requestSignal = registerSignal("requestReceived");
        replySignal = registerSignal("replySent");
        responseTimeSignal = registerSignal("responseTime");

        EV_INFO << "DHCPv6 server up with pools:\n"
                << "  pc      : " << pcPrefix << "\n"
                << "  mobile  : " << mobilePrefix << "\n"
                << "  printer : " << printerPrefix << "\n"
                << "  VIP     : " << vipPrefix << "\n"
                << "  VIP Lease Time: " << vipLeaseTime << "s\n"
                << "  Normal Lease Time: " << normalLeaseTime << "s\n";
    }

    virtual void handleMessage(cMessage *msg) override {
        if (msg == processSolicitEvent) {
            processingSolicit = false;
            processNextSolicit();
        }
        else if (msg == processRequestEvent) {
            processingRequest = false;
            processNextRequest();
        }
        else {
            int inIx = msg->getArrivalGate()->getIndex();
            int prio = (msg->hasPar("priority") ? (int)msg->par("priority").longValue() : 1);

            if (msg->getKind() == DHCPV6_SOLICIT) {
                solicitCount++;
                emit(solicitSignal, 1L);

                PendingRequest req;
                req.msg = msg;
                req.gateIndex = inIx;
                req.priority = prio;
                req.arrivalTime = simTime();
                solicitQueue.push(req);

                if (!processingSolicit) {
                    processNextSolicit();
                }
            }
            else if (msg->getKind() == DHCPV6_REQUEST || msg->getKind() == DHCPV6_RENEW) {
                if (msg->getKind() == DHCPV6_REQUEST) {
                    requestCount++;
                    emit(requestSignal, 1L);
                } else {
                    renewCount++;
                }

                PendingRequest req;
                req.msg = msg;
                req.gateIndex = inIx;
                req.priority = prio;
                req.arrivalTime = simTime();
                requestQueue.push(req);

                if (!processingRequest) {
                    processNextRequest();
                }
            }
            else {
                EV_WARN << "Unexpected message kind=" << msg->getKind() << "\n";
                delete msg;
            }
        }
    }

    void processNextSolicit() {
        if (solicitQueue.empty()) {
            return;
        }

        PendingRequest req = solicitQueue.top();
        solicitQueue.pop();

        cMessage *msg = req.msg;
        int inIx = req.gateIndex;

        long dev = SRC(msg);
        string devType = (msg->hasPar("type") ? msg->par("type").stringValue() : "pc");
        int prio = (msg->hasPar("priority") ? (int)msg->par("priority").longValue() : 1);

        bool isVip = isVipClient(devType, prio);
        string prefix = pickPrefix(devType, prio);
        string offer  = makeAddress(prefix, nextIdForPool[prefix]);
        double lease = isVip ? vipLeaseTime : normalLeaseTime;

        nextIdForPool[prefix]++;

        advertiseCount++;
        emit(advertiseSignal, 1L);

        EV_INFO << "[" << simTime() << "] SOLICIT from devId=" << dev
                << " type=" << devType << " prio=" << prio
                << " -> ADVERTISE " << offer
                << (isVip ? " (VIP)" : " (normal)")
                << " lease=" << lease << "s\n";

        auto *adv = mk("DHCPV6_ADVERTISE", DHCPV6_ADVERTISE, 0, dev);
        adv->addPar("ip6").setStringValue(offer.c_str());
        adv->addPar("priority").setLongValue(prio);
        adv->addPar("leaseTime").setDoubleValue(lease);
        simtime_t d = isVip ? fastResponseDelay : normalResponseDelay;
        sendDelayed(adv, d, "pppg$o", inIx);

        // Track response time
        simtime_t respTime = simTime() - req.arrivalTime + d;
        totalResponseTime += respTime;
        responseCount++;
        emit(responseTimeSignal, respTime);

        delete msg;

        if (!solicitQueue.empty()) {
            processingSolicit = true;
            scheduleAt(simTime() + 0.00001, processSolicitEvent);
        }
    }

    void processNextRequest() {
        if (requestQueue.empty()) {
            return;
        }

        PendingRequest req = requestQueue.top();
        requestQueue.pop();

        cMessage *msg = req.msg;
        int inIx = req.gateIndex;

        long dev = SRC(msg);
        string ip6 = msg->par("ip6").stringValue();
        int prio = (msg->hasPar("priority") ? (int)msg->par("priority").longValue() : 1);

        addrTable[dev] = ip6;

        string vipBase = vipPrefix.substr(0, vipPrefix.find('/'));
        bool isVip = (ip6.find(vipBase) == 0);
        double lease = isVip ? vipLeaseTime : normalLeaseTime;

        replyCount++;
        emit(replySignal, 1L);

        string msgType = (msg->getKind() == DHCPV6_RENEW) ? "RENEW" : "REQUEST";
        EV_INFO << "[" << simTime() << "] " << msgType << " from devId=" << dev
                << " prio=" << prio << " for " << ip6
                << " -> REPLY " << (isVip ? "(VIP)" : "(normal)")
                << " lease=" << lease << "s\n";

        auto *rep = mk("DHCPV6_REPLY", DHCPV6_REPLY, 0, dev);
        rep->addPar("ip6").setStringValue(ip6.c_str());
        rep->addPar("leaseTime").setDoubleValue(lease);
        simtime_t d = isVip ? fastResponseDelay : normalResponseDelay;
        sendDelayed(rep, d, "pppg$o", inIx);

        delete msg;

        if (!requestQueue.empty()) {
            processingRequest = true;
            scheduleAt(simTime() + 0.00001, processRequestEvent);
        }
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

    virtual void finish() override {
        if (processSolicitEvent) {
            if (processSolicitEvent->isScheduled())
                cancelEvent(processSolicitEvent);
            delete processSolicitEvent;
            processSolicitEvent = nullptr;
        }

        if (processRequestEvent) {
            if (processRequestEvent->isScheduled())
                cancelEvent(processRequestEvent);
            delete processRequestEvent;
            processRequestEvent = nullptr;
        }

        while (!solicitQueue.empty()) {
            delete solicitQueue.top().msg;
            solicitQueue.pop();
        }
        while (!requestQueue.empty()) {
            delete requestQueue.top().msg;
            requestQueue.pop();
        }

        // Print statistics
        EV_INFO << "\n=== DHCPv6 Server Statistics ===\n";
        EV_INFO << "SOLICIT messages received: " << solicitCount << "\n";
        EV_INFO << "ADVERTISE messages sent: " << advertiseCount << "\n";
        EV_INFO << "REQUEST messages received: " << requestCount << "\n";
        EV_INFO << "RENEW messages received: " << renewCount << "\n";
        EV_INFO << "REPLY messages sent: " << replyCount << "\n";
        EV_INFO << "Total messages processed: " << (solicitCount + requestCount + renewCount) << "\n";
        if (responseCount > 0) {
            EV_INFO << "Average response time: " << (totalResponseTime / responseCount) << "s\n";
        }
        EV_INFO << "Active leases: " << addrTable.size() << "\n";
        EV_INFO << "================================\n";

        // Record statistics
        recordScalar("solicitCount", solicitCount);
        recordScalar("advertiseCount", advertiseCount);
        recordScalar("requestCount", requestCount);
        recordScalar("renewCount", renewCount);
        recordScalar("replyCount", replyCount);
        recordScalar("activeLeases", (long)addrTable.size());
        if (responseCount > 0) {
            recordScalar("avgResponseTime", totalResponseTime / responseCount);
        }
    }
};

Define_Module(DHCP);
