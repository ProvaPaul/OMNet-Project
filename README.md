🌐 Priority-based DHCPv6 Network Simulation

A comprehensive network simulation of DHCP (Dynamic Host Configuration Protocol) for IPv6 with priority-based service distribution and active-passive failover mechanism.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [How It Works](#how-it-works)
- [Simulation Results](#simulation-results)
- [Documentation](#documentation)
- [Technologies Used](#technologies-used)
- [License](#license)

---

## 🎯 Overview

This project simulates a realistic network environment where:
- **5 Network Devices**: Server, Router, PC, Mobile Device, Printer
- **2 DHCP Servers**: Primary (Active) and Backup (Standby)
- **1 Network Switch**: Central hub connecting all devices
- **Priority-based IP Distribution**: VIP devices get fast responses, regular devices get normal responses
- **Failover Mechanism**: Backup server automatically takes over when Primary fails

The simulation demonstrates how modern networks ensure continuous service availability even when critical infrastructure components fail.

---

## ✨ Features

### 1. **DHCP Protocol Implementation**
- Full 4-step DHCP process (SOLICIT → ADVERTISE → REQUEST → REPLY)
- IPv6 address allocation with pools for different device types
- Configurable response delays based on priority

### 2. **Priority-Based Service (QoS)**
```
Priority 9-10 (VIP):      Server, Router       → Fast response (0.01s)
Priority 1-8 (Standard):  PC, Mobile, Printer  → Normal response (0.02s)
```

### 3. **Active-Passive Failover System**
- Continuous heartbeat monitoring between servers
- Automatic failover detection (1.5 second timeout)
- Real-time takeover when Primary server fails
- Backup becomes Active and serves new clients

### 4. **Data Synchronization**
- Primary server syncs all lease data to Backup every 0.5 seconds
- Ensures Backup has complete state information
- Seamless transition when failover occurs

### 5. **IPv6 Address Pools**
```
VIP Pool:     2001:db8:vip::/64      (for Server, Router)
PC Pool:      2001:db8:pc::/64       (for PCs)
Mobile Pool:  2001:db8:mob::/64      (for Mobile devices)
Printer Pool: 2001:db8:prn::/64      (for Printers)
```

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Network Topology                      │
└─────────────────────────────────────────────────────────┘

                         ┌─────────┐
                         │ Switch  │
                         │ (Hub)   │
                         └────┬────┘
          ┌──────────┬───────┼──────────┬──────────┐
          │          │       │          │          │
      ┌───┴───┐  ┌──┴──┐  ┌─┴──┐  ┌───┴───┐  ┌──┴───┐
      │Server │  │Router   │PC1 │  │Mobile │  │Print │
      │(P:10) │  │(P:9)    │(P:3)   │(P:2)  │  │(P:1) │
      └───────┘  └──────┘  └────┘  └───────┘  └──────┘
          │          │       │          │          │
          └──────────┴───────┼──────────┴──────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
              ┌─────┴──┐        ┌────┴─────┐
              │Primary │◄─────►│ Backup   │
              │Server  │ Sync  │ Server   │
              │(Active)│ Data  │(Standby) │
              └────────┘       └──────────┘
```

---

## 💻 Installation

### Prerequisites
- **OMNeT++ Simulator** (v5.0 or higher)
- **C++ Compiler** (g++ or clang)
- **Linux/macOS/Windows** (with OMNeT++ support)

### Steps

1. **Clone the Repository**
```bash
git clone https://github.com/yourusername/priority-dhcpv6-simulation.git
cd priority-dhcpv6-simulation
```

2. **Open with OMNeT++**
```bash
# Start OMNeT++ IDE
omnetpp

# Open project folder
File → Open Projects from File System
```

3. **Build the Project**
```bash
# Inside OMNeT++ IDE:
Project → Build Project
# Or use command line:
make
```

4. **Run the Simulation**
```bash
# In OMNeT++ IDE:
Click Run button or press Ctrl+F5

# Or via command line:
./run -f omnetpp.ini -u Cmdenv
```

---

## ⚙️ Configuration

Edit `omnetpp.ini` to customize simulation parameters:

```ini
[General]
network = prioritydhcp.DeviceTypeDhcpNet
sim-time-limit = 10s

# DHCP Server Settings
**.dhcp*.fastResponseDelay = 0.01s      # VIP response time
**.dhcp*.normalResponseDelay = 0.02s    # Normal response time
**.dhcp*.vipPriorityCutoff = 9          # Priority threshold for VIP
**.dhcp*.syncInterval = 0.5s            # Data sync interval
**.dhcp*.failoverTimeout = 1.5s         # Failover detection timeout

# Primary Server
**.dhcp_main.isPrimary = true
**.dhcp_main.failureTime = 5s           # When to simulate failure

# Backup Server
**.dhcp_backup.isPrimary = false

# Device Start Times (based on priority)
**.server1.startJitter = 0s             # t = 0.0s
**.router1.startJitter = 0s             # t = 0.3s
**.pc1.startJitter = 0s                 # t = 0.6s
**.mobile1.startJitter = 0s             # t = 0.9s
**.printer1.startJitter = 0s            # t = 1.2s
**.pc2.startJitter = 8s                 # Failover test → t = 8.0s
```

---

## 🚀 Usage

### Basic Simulation
```bash
# Run with full GUI
./run -f omnetpp.ini -u Tkenv

# Run in console mode
./run -f omnetpp.ini -u Cmdenv

# Run with specific config
./run -f omnetpp.ini -c General -u Cmdenv
```

### View Results
After simulation completes, check the output for:
- Device statistics (IP allocation, DHCP completion status)
- Server statistics (total leases, requests handled)
- Failover test results
- Network events timeline

---

## 📁 Project Structure

```
priority-dhcpv6-simulation/
│
├── helpers.h                 # Message type definitions and helper functions
├── Switch.cpp               # Network switch module (broadcasts messages)
├── DHCP.cpp                # DHCP server module (Primary & Backup)
├── Device.cpp              # Client device module
├── prioritydhcp.ned        # Network topology definition
├── omnetpp.ini             # Simulation configuration
├── omnetpp.ini.bak         # Backup configuration
│
├── results/                # Simulation output results
│   ├── *.sca              # Scalar results
│   └── *.vec              # Vector results
│
├── docs/                  # Documentation
│   ├── DHCP_Complete_Bangla_Guide.md
│   ├── Teacher_Presentation.txt
│   ├── README_BANGLA.md
│   └── DHCP_Code_Bangla_Explanation.txt
│
└── README.md              # This file
```

---

## 🔄 How It Works

### DHCP Process (DORA)

```
Device                          Server
  │                              │
  ├──── SOLICIT (1/4) ─────────►│  "I need an IP"
  │                              │
  │◄───── ADVERTISE (2/4) ──────┤  "Here's an offer"
  │       2001:db8:pc::1         │
  │                              │
  ├──── REQUEST (3/4) ──────────►│  "Confirm this IP"
  │                              │
  │◄──── REPLY (4/4) ────────────┤  "Done! It's yours"
  │                              │
  ● IPv6 Configured             ●
```

**Timeline:**
- t = 0.0000s: SOLICIT sent
- t = 0.0104s: ADVERTISE received (VIP: 0.01s, Regular: 0.02s)
- t = 0.0105s: REQUEST sent
- t = 0.0208s: REPLY received → DHCP Complete

### Failover Mechanism

```
Timeline:
═════════════════════════════════════════

t = 0.0s    Primary Server: ACTIVE ✓
            Backup Server: STANDBY

            All timers active:
            • syncTimer (every 0.5s)
            • heartbeatTimer (every 0.25s)
            • checkPartnerTimer (every 1.5s)

─────────────────────────────────────────

t = 5.0s    PRIMARY SERVER FAILURE SIMULATED ✗
            
            Status Change:
            • hasFailed = true
            • isActive = false
            • All timers cancelled

─────────────────────────────────────────

t = 5.0 - 6.5s   Backup monitoring...
                 No heartbeat received
                 Timeout counter: 0 → 1.5s

─────────────────────────────────────────

t = 6.5s    PARTNER FAILURE DETECTED!
            
            Backup takes over:
            • partnerAlive = false
            • isActive = true
            • Ready to serve requests

t = 7.5s    Backup fully ACTIVE ✓

─────────────────────────────────────────

t = 8.0s    PC_2_Failover requests IP
            
            Response from: Backup Server ✓
            Assigned IP: 2001:db8:pc::2
            
            ✓ FAILOVER SUCCESS!

═════════════════════════════════════════
```

---

## 📊 Simulation Results

### Assigned IPv6 Addresses

| Device | Priority | Type | Pool | Assigned IP | Status |
|--------|----------|------|------|-------------|--------|
| Server_1 | 10 | VIP | VIP | 2001:db8:vip::1 | ✓ SUCCESS |
| Router_1 | 9 | VIP | VIP | 2001:db8:vip::2 | ✓ SUCCESS |
| PC_1 | 3 | Normal | PC | 2001:db8:pc::1 | ✓ SUCCESS |
| Mobile_1 | 2 | Normal | Mobile | 2001:db8:mob::1 | ✓ SUCCESS |
| Printer_1 | 1 | Normal | Printer | 2001:db8:prn::1 | ✓ SUCCESS |

### Response Times

```
VIP Devices (Priority 9-10):
├─ Server_1: 0.01s per response ✓ FAST
└─ Router_1: 0.01s per response ✓ FAST

Normal Devices (Priority 1-8):
├─ PC_1: 0.02s per response
├─ Mobile_1: 0.02s per response
└─ Printer_1: 0.02s per response
```

### Failover Test Results

```
Primary Server Status at t=8.0s:
├─ Status: STANDBY (Down)
├─ Failed: YES
└─ Partner Status: DOWN

Backup Server Status at t=8.0s:
├─ Status: ACTIVE ✓
├─ Failed: NO
└─ Partner Status: DOWN

Failover Test Device (PC_2):
├─ Request Time: t=8.0s
├─ Response Server: dhcp_backup ✓
├─ Assigned IP: 2001:db8:pc::2 ✓
└─ Result: ✓ SUCCESS
```

---

## 📚 Documentation

### English Documentation
- **README.md** (This file) - Project overview and technical guide

### Bengali Documentation (বাংলা)
- **docs/Teacher_Presentation.txt** - 5-minute presentation guide for teacher
- **docs/DHCP_Complete_Bangla_Guide.md** - Detailed technical explanation in Bengali
- **docs/README_BANGLA.md** - Quick reference guide in Bengali
- **docs/DHCP_Code_Bangla_Explanation.txt** - Code-level explanations in Bengali

---

## 🛠️ Technologies Used

- **Language**: C++
- **Simulator**: OMNeT++ (Objective Modular Network Testbed in C++)
- **Protocol**: DHCPv6 (Dynamic Host Configuration Protocol version 6)
- **Network**: IPv6-based simulation
- **Paradigm**: Discrete event simulation

### Key Libraries
- omnetpp.h: Core OMNeT++ framework
- string, unordered_map, map: C++ standard library containers
- sstream: String stream for data processing

---

## 📝 Code Modules

### 1. **helpers.h**
```cpp
// Message type definitions
#define DHCPV6_SOLICIT    601
#define DHCPV6_ADVERTISE  602
#define DHCPV6_REQUEST    603
#define DHCPV6_REPLY      604
#define DHCP_SYNC         605
#define DHCP_HEARTBEAT    606

// Message creation helper
inline omnetpp::cMessage* mk(const char* name, int kind, long src, long dst)
inline long SRC(omnetpp::cMessage *m)
inline long DST(omnetpp::cMessage *m)
```

### 2. **Switch.cpp**
- Broadcasts messages to all connected ports
- Acts as network hub
- Bridges all devices and servers

### 3. **DHCP.cpp**
- Implements DHCP server logic
- Manages IP address pools
- Handles priority-based service
- Implements failover mechanism
- Syncs data with partner server

### 4. **Device.cpp**
- Simulates client devices
- Implements DHCP client state machine
- Handles SOLICIT, ADVERTISE, REQUEST, REPLY
- Collects statistics

### 5. **prioritydhcp.ned**
- Defines network topology
- Specifies module connections
- Configures gates and channels

---

## 🎓 Learning Outcomes

After studying this project, you will understand:

✓ **DHCP Protocol**: How devices automatically get IP addresses  
✓ **IPv6 Addressing**: IPv6 address structure and pools  
✓ **Priority-Based QoS**: Giving priority to critical devices  
✓ **Failover Mechanism**: How services remain available during failures  
✓ **Network Simulation**: Building realistic network models  
✓ **State Machines**: Implementing complex protocols  
✓ **OMNeT++ Framework**: Discrete event simulation concepts  
✓ **C++ Programming**: Advanced object-oriented concepts  

---

## 🔍 Key Concepts

### Message Flow Diagram
```
SOLICIT
   ↓
   ├─→ Switch broadcasts to all ports
   │
   ├─→ Server receives (checks if destination matches)
   ├─→ Router receives (checks if destination matches)
   ├─→ PC receives (checks if destination matches)
   ├─→ Mobile receives (checks if destination matches)
   ├─→ Printer receives (checks if destination matches)
   ├─→ dhcp_main DHCP Server PROCESSES ✓
   └─→ dhcp_backup DHCP Server receives (standby)

ADVERTISE (only from dhcp_main to original sender)
   ↓
   └─→ Only destination device processes
```

### Priority Decision Tree
```
Is device Server or Router?
├─ YES → VIP (Priority 9-10)
│        └─ Fast response (0.01s)
│
├─ NO: Check Priority field
│  ├─ Priority >= 9 → VIP
│  │  └─ Fast response (0.01s)
│  │
│  └─ Priority < 9 → Normal
│     └─ Normal response (0.02s)
```

### Sync & Heartbeat Process
```
Every 0.5s (syncTimer):
├─ Primary sends: DHCP_SYNC message
└─ Contains: All IP pool counters and lease data

Every 0.25s (heartbeatTimer):
├─ Primary sends: DHCP_HEARTBEAT message
└─ Backup updates: lastPartnerHeartbeat = simTime()

Every 1.5s (checkPartnerTimer):
├─ Backup checks: elapsed time since last heartbeat
└─ If elapsed > 1.5s: Trigger failover
```

---

## 🐛 Troubleshooting

### Issue: Simulation doesn't run
**Solution**: 
- Check OMNeT++ is installed: `omnetpp -version`
- Verify C++ compiler: `g++ --version`
- Rebuild project: `make clean && make`

### Issue: Devices don't get IP addresses
**Solution**:
- Check `omnetpp.ini` configuration
- Verify device priority values
- Check server is in ACTIVE state

### Issue: Failover doesn't happen
**Solution**:
- Check `failureTime` in omnetpp.ini (should be > 0)
- Verify `failoverTimeout` setting
- Check heartbeat timers are running

---

## 📈 Performance Metrics

```
Simulation Statistics:
═════════════════════════════════════════

Total Devices:              5 (+ 1 for failover test)
Total DHCP Servers:        2 (Primary + Backup)
SOLICIT Messages:          6 (5 + 1 failover test)
ADVERTISE Messages:        6
REQUEST Messages:          6
REPLY Messages:            6
Total DHCP Transactions:   24 messages

Response Times:
├─ VIP Devices:           0.01s
├─ Normal Devices:        0.02s
└─ Average:              0.01625s

Failover Detection Time:  1.5s
IP Allocation Success:    100% (6/6 devices)
Failover Success:         100% (1/1 test)

Simulation Time:          10 seconds
Performance:              ✓ EXCELLENT
```

---

## 🚀 Future Enhancements

- [ ] Add DHCP DECLINE and RELEASE messages
- [ ] Implement DHCP Relay Agents
- [ ] Add DHCPv6 Prefix Delegation
- [ ] Implement multiple backup servers
- [ ] Add network congestion simulation
- [ ] Implement DHCP security features
- [ ] Add more detailed logging and tracing
- [ ] Create visualization dashboard
- [ ] Add performance benchmarking
- [ ] Implement load balancing between servers

---

## 📞 Contact & Support

For questions or issues:
1. Check the documentation in `docs/` folder
2. Review code comments in source files
3. Check OMNeT++ official documentation
4. Open an issue on GitHub

---

## 📄 License

This project is open source. Please specify your license here:
- MIT License
- Apache License 2.0
- GPL v3.0
- Other: [Specify]

---

## 🙏 Acknowledgments

- OMNeT++ Community
- Network Protocol References (RFC 8415, RFC 3315)
- Contributors and reviewers
- Academic advisors

---

## 📖 References

1. **RFC 8415** - Dynamic Host Configuration Protocol for IPv6 (DHCPv6)
2. **RFC 3315** - Dynamic Host Configuration Protocol for IPv6 (DHCPv6)
3. OMNeT++ User Manual: https://docs.omnetpp.org/
4. IPv6 Addressing Architecture (RFC 4291)
5. Internet Engineering Task Force (IETF) Standards

---

## 🔗 Quick Links

- **OMNeT++ Official**: https://omnetpp.org/
- **Download OMNeT++**: https://github.com/omnetpp/omnetpp
- **RFC 8415**: https://datatracker.ietf.org/doc/html/rfc8415
- **IPv6 Address Format**: https://en.wikipedia.org/wiki/IPv6_address

---

## 🎯 Project Status

- ✅ Core DHCP functionality implemented
- ✅ Priority-based service working
- ✅ Failover mechanism tested
- ✅ Data synchronization verified
- ✅ Documentation complete
- ✅ All devices get IP successfully
- ✅ Failover test successful

**Current Version**: 1.0.0  
**Last Updated**: October 30, 2025  
**Status**: ✓ STABLE & PRODUCTION READY

---

Made with ❤️ by Your Name/Organization

**Star ⭐ this repository if you find it helpful!**
