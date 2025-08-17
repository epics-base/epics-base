# casw

    casw [-i <interest level>]

## Description

CA server "beacon anomaly" logging.

CA server beacon anomalies occur when a new server joins the network, a
server is rebooted, network connectivity to a server is reestablished,
or if a server's CPU exits a CPU load saturated state.

CA clients with unresolved channels reset their search request
scheduling timers whenever they see a beacon anomaly.

This program can be used to detect situations where there are too many
beacon anomalies. IP routing configuration problems may result in false
beacon anomalies that might cause CA clients to use unnecessary
additional network bandwidth and server CPU load when searching for
unresolved channels.

If there are no new CA servers appearing on the network, and network
connectivity remains constant, then casw should print no messages at
all. At higher interest levels the program prints a message for every
beacon that is received, and anomalous entries are flagged with a star.

