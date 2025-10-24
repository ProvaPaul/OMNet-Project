package prioritydhcp;

channel P2P extends ned.DatarateChannel
{
    datarate = 100Mbps;
    delay = 0.2ms;
}

simple DHCP
{
    parameters:
        string pcPrefix       = default("2001:db8:pc::/64");
        string mobilePrefix   = default("2001:db8:mob::/64");
        string printerPrefix  = default("2001:db8:prn::/64");
        string vipPrefix      = default("2001:db8:vip::/64");

        double fastResponseDelay   @unit(s) = default(0.05s);
        double normalResponseDelay @unit(s) = default(uniform(0.1s,0.3s));
        int    vipPriorityCutoff   = default(9);

        // NEW: Lease time parameters
        double vipLeaseTime    @unit(s) = default(60s);
        double normalLeaseTime @unit(s) = default(30s);

        @display("i=block/process");
    gates:
        inout pppg[];
}

simple Device
{
    parameters:
        string type;
        string name;
        int    priority = default(1);
        double startJitter @unit(s) = default(uniform(0.1s, 0.5s));
        @display("i=device/laptop");
    gates:
        inout ppp;
}

network DeviceTypeDhcpNet
{
    @display("bgb=760,460");

    submodules:
        dhcp: DHCP {
            @display("p=520,230;i=block/process");
            gates:
                pppg[5];
        }

        server1: Device {
            parameters:
                type = "server";
                name = "Server_1";
                priority = 10;
                @display("p=160,120;i=device/server");
        }
        router1: Device {
            parameters:
                type = "router";
                name = "Router_1";
                priority = 9;
                @display("p=160,200;i=device/router");
        }
        pc1: Device {
            parameters:
                type = "pc";
                name = "PC_1";
                priority = 3;
                @display("p=160,280;i=device/laptop");
        }
        mobile1: Device {
            parameters:
                type = "mobile";
                name = "Mobile_1";
                priority = 2;
                @display("p=160,340;i=device/smartphone");
        }
        printer1: Device {
            parameters:
                type = "printer";
                name = "Printer_1";
                priority = 1;
                @display("p=160,400;i=device/printer");
        }

    connections allowunconnected:
        server1.ppp   <--> P2P <--> dhcp.pppg[0];
        router1.ppp   <--> P2P <--> dhcp.pppg[1];
        pc1.ppp       <--> P2P <--> dhcp.pppg[2];
        mobile1.ppp   <--> P2P <--> dhcp.pppg[3];
        printer1.ppp  <--> P2P <--> dhcp.pppg[4];
}