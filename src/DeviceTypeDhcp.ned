package prioritydhcp;

channel P2P extends ned.DatarateChannel
{
    datarate = 1Gbps;
    delay = 0.1ms;
}

simple Switch
{
    parameters:
        @display("i=block/switch");
    gates:
        inout port[];
}

simple DHCP
{
    parameters:
        string pcPrefix       = default("2001:db8:pc::/64");
        string mobilePrefix   = default("2001:db8:mob::/64");
        string printerPrefix  = default("2001:db8:prn::/64");
        string vipPrefix      = default("2001:db8:vip::/64");

        double fastResponseDelay   @unit(s) = default(0.01s);
        double normalResponseDelay @unit(s) = default(0.02s);
        int    vipPriorityCutoff   = default(9);

        bool   isPrimary = default(true);
        string partnerName = default("");
        double syncInterval @unit(s) = default(0.5s);
        double failoverTimeout @unit(s) = default(1.5s);
        double failureTime @unit(s) = default(-1s);

        @display("i=block/process");
    gates:
        inout ppp;
        output syncOut;
        input syncIn;
}

simple Device
{
    parameters:
        string type;
        string name;
        int    priority = default(1);
        double startJitter @unit(s) = default(uniform(0.01s, 0.05s));
        @display("i=device/laptop");
    gates:
        inout ppp;
}

network DeviceTypeDhcpNet
{
    @display("bgb=760,520");

    submodules:
        switch: Switch {
            @display("p=320,230;i=block/switch");
        }

        dhcp_main: DHCP {
            parameters:
                isPrimary = true;
                partnerName = "dhcp_backup";
                @display("p=520,150;i=block/process");
        }

        dhcp_backup: DHCP {
            parameters:
                isPrimary = false;
                partnerName = "dhcp_main";
                @display("p=520,310;i=block/process");
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
        pc2: Device {
            parameters:
                type = "pc";
                name = "PC_2_Failover";
                priority = 4;
                @display("p=160,460;i=device/laptop2");
        }

    connections:
        // Connect devices to switch
        server1.ppp <--> P2P <--> switch.port++;
        router1.ppp <--> P2P <--> switch.port++;
        pc1.ppp <--> P2P <--> switch.port++;
        mobile1.ppp <--> P2P <--> switch.port++;
        printer1.ppp <--> P2P <--> switch.port++;
        pc2.ppp <--> P2P <--> switch.port++;

        // Connect DHCP servers to switch
        dhcp_main.ppp <--> P2P <--> switch.port++;
        dhcp_backup.ppp <--> P2P <--> switch.port++;

        // Direct sync connection between servers
        dhcp_main.syncOut --> P2P --> dhcp_backup.syncIn;
        dhcp_backup.syncOut --> P2P --> dhcp_main.syncIn;
}