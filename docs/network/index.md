# Network

What is on the bus, and how each drive is configured when the master boots it, is not
written in application code. It is described once, in **`bus.yml`**, and turned into the
master's configuration at build time.

```text
bus.yml + epos4.eds ──dcfgen──▶ master.dcf   what the master knows: nodes, PDO layouts, SYNC
                                node_N.bin   what the master writes to node N at boot
```

- **[The network description (bus.yml)](bus-yml.md)** - every key, for the master and for
  each drive.
- **[PDO mapping](pdo-mapping.md)** - what the example mapping carries, how to change it,
  and how to check that both ends agree.
- **[SYNC and heartbeat](sync-heartbeat.md)** - the two timings of the network, and how
  they relate to the drive's interpolation period and fault reaction.
- **[Multiple drives](multi-drive.md)** - a network for a rover drivetrain, bus load, and
  node-ID planning.
