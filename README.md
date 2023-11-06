MTP
================================
This repository contains the implementation of the [MTP](https://dl.acm.org/doi/10.1145/3484266.3487382) on NS-3.

## Table of Contents:


1) [Building ns-3](#building-ns-3)
2) [Running multi-path congestion control scenario](#running-multi-path-congestion-control-scenario)
3) [Running load and request-aware load balancers scenario](#running-load-and-request-aware-load-balancers-scenario)
4) [Running per-entity isolation scenario](#running-per-entity-isolation-scenario)

## Building ns-3

The code for the framework and the default models provided
by ns-3 is built as a set of libraries. User simulations
are expected to be written as simple programs that make
use of these ns-3 libraries.

To build the set of default libraries and the example
programs included in this package, you need to use the
tool 'waf'. Detailed information on how to use waf is
included in the file doc/build.txt

However, the real quick and dirty way to get started is to
type the command
```shell
./waf configure --enable-examples
```

followed by

```shell
./waf
```

in the directory which contains this README file. The files
built will be copied in the build/ directory.


## Running multi-path congestion control scenario
Simulation files for this scenario can be found in [`ns-3-dev/scratch`](https://github.com/hamidralmasi/mtp-sim/tree/master/ns-3-dev/scratch).

To run MTP, you can pass the desired evaluation arguments (see the table below) and issue:

```shell
NS_LOG="Hybrid" ./waf --run "hybrid \
--simulationEndTime=0.1 --timePeriod=384 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us \
--switchFirstPathBW=100Gbps --switchFirstPathDelay=1us \
--switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us \
--minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log384_100ms_hybrid.out 2>&1

```
inside a terminal at the [`ns-3-dev`](https://github.com/hamidralmasi/mtp-sim/tree/master/ns-3-dev/) folder.

Similarly, to run DCTCP you can use:

```shell
NS_LOG="Hdctcp" ./waf --run "hdctcp \
--simulationEndTime=0.1 --timePeriod=384 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us \
--switchFirstPathBW=100Gbps --switchFirstPathDelay=1us \
--switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us \
--minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log384_100ms_hybrid.out 2>&1
```

| Argument               | Description                                      |
|------------------------|--------------------------------------------------|
| `timePeriod`           | Time period before a path switch                 |
| `senderToSwitchBW`     | Sender to switch bandwidth                       |
| `senderToSwitchDelay`  | Sender to switch delay                           |
| `switchFirstPathBW`    | Switch to receiver first path bandwidth          |
| `switchFirstPathDelay` | Switch to receiver first path delay              |
| `switchSecondPathBW`   | Switch to receiver second path bandwidth         |
| `switchSecondPathDelay`| Switch to second path delay                      |
| `minTh`                | Red Min Threshold                                |
| `maxTh`                | Red Max Threshold                                |
| `bufferSize`           | Switch buffer size                               |
| `tracing`              | Flag to enable/disable Ascii and Pcap tracing    |
| `maxBytes`             | Total number of bytes for application to send    |
| `useEcn`               | Flag to enable/disable ECN                       |
| `useQueueDisc`         | Flag to enable/disable queue disc on bottleneck  |
| `simulationEndTime`    | Simulation end time                              |


## Running load and request-aware load balancers scenario
The simulation file for this scenario can be found in [`LB_and_MultiTenant_Exps/examples/rtt-variations`](https://github.com/hamidralmasi/mtp-sim/tree/master/LB_and_MultiTenant_Exps/examples/rtt-variations/loadbalance.cc).

To run each load balancing scheme, you can pass the desired evaluation arguments (see the table below) and issue:

```bash
NS_LOG="Loadbalance" ./waf --run "loadbalance --ID=LB_Exp_ID  --load=0.6 --mode=LB_Scheme \
--simulationEndTime=1 --timePeriod=384 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us \
--switchFirstPathBW=50Gbps --switchFirstPathDelay=5us \
--switchSecondPathBW=50Gbps --switchSecondPathDelay=1us \
--switchToReceiverBW=100Gbps --switchToReceiverDelay=1us \
--minTh=5 --maxTh=5 --bufferSize=16p --tracing=false \
--cdfFileName=examples/rtt-variations/VL2_CDF.txt \
--requestNum=1000" > LB_Scheme_128RcvBuf_4DD_L60_VL2CDF_1s.txt 2>&1
```

where mode can be `ECMP`, `PSpray` (for packet spraying), or `ReqLA` (for request and load-aware)

| Argument               | Description                                   |
|------------------------|-----------------------------------------------|
| `ID`                   | Running ID                                    |
| `mode`                 | Load balancing scheme                         |
| `timePeriod`           | Time period before a path switch              |
| `senderToSwitchBW`     | Sender to switch bandwidth                    |
| `senderToSwitchDelay`  | Sender to switch delay                        |
| `switchFirstPathBW`    | Switch to receiver first path bandwidth       |
| `switchFirstPathDelay` | Switch to receiver first path delay           |
| `switchSecondPathBW`   | Switch to receiver second path bandwidth      |
| `switchSecondPathDelay`| Switch to second path delay                   |
| `minTh`                | Red Min Threshold                             |
| `maxTh`                | Red Max Threshold                             |
| `bufferSize`           | Switch buffer size                            |
| `load`                 | Load                                          |
| `tracing`              | Flag to enable/disable Ascii and Pcap tracing |
| `maxBytes`             | Total number of bytes for application to send |
| `useEcn`               | Flag to enable/disable ECN                    |
| `useQueueDisc`         | Flag to enable/disable queue disc on bottleneck |
| `cdfFileName`          | CDF of the workload                           |
| `requestNum`           | Number of requests                            |
| `simulationEndTime`    | Simulation end time                           |


## Running per-entity isolation scenario

Simulation files for this scenario can be found in [`LB_and_MultiTenant_Exps/scratch`](https://github.com/hamidralmasi/mtp-sim/tree/master/LB_and_MultiTenant_Exps/scratch).

To run the shared queueing scheme, you can pass the desired evaluation arguments (see the table below) and issue:

```bash
NS_LOG="MTSQ" ./waf --run "mtsq --id=20 \
--T1SwitchBW=100Gbps --T1SwitchDelay=10us --T2SwitchBW=100Gbps --T2SwitchDelay=10us \
--switchReceiverBW=100Gbps --switchReceiverDelay=10us \
--minTh=256 --maxTh=256 --bufferSize=1024 \
--numFlowsT1=10 --numFlowsT2=80 --tracing=false --endTime=0.1" > 20.txt 2>&1
```

| Argument            | Description                                       |
|---------------------|---------------------------------------------------|
| `id`                | The running ID                                    |
| `transportProt`     | Transport protocol to use: Tcp, DcTcp             |
| `AQM`               | AQM to use: RED                                   |
| `endTime`           | Simulation end time                               |
| `randomSeed`        | Random seed, 0 for random generated               |
| `T1SwitchBW`        | Tenant 1 to switch bandwidth                      |
| `T1SwitchDelay`     | Tenant 1 to switch delay                          |
| `T2SwitchBW`        | Tenant 2 to switch bandwidth                      |
| `T2SwitchDelay`     | Tenant 2 to switch delay                          |
| `switchReceiverBW`  | Switch to receiver second path bandwidth          |
| `switchReceiverDelay`| Switch to second path delay                      |
| `bufferSize`        | Switch buffer size                                |
| `minTh`             | Red Min Threshold                                 |
| `maxTh`             | Red Max Threshold                                 |
| `numFlowsT1`        | Number of flows generated from Tenant 1           |
| `numFlowsT2`        | Number of flows generated from Tenant 2           |
| `tracing`           | Flag to enable/disable Ascii and Pcap tracing     |


Running the separate queueing scheme is similar. We use `mtmq` in the command above.

## Citation

```bib
@inproceedings{10.1145/3484266.3487382,
author = {Stephens, Brent E. and Grassi, Darius and Almasi, Hamidreza and Ji, Tao and Vamanan, Balajee and Akella, Aditya},
title = {TCP is Harmful to In-Network Computing: Designing a Message Transport Protocol (MTP)},
year = {2021},
isbn = {9781450390873},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3484266.3487382},
doi = {10.1145/3484266.3487382},
booktitle = {Proceedings of the Twentieth ACM Workshop on Hot Topics in Networks},
pages = {61–68},
numpages = {8},
location = {Virtual Event, United Kingdom},
series = {HotNets '21}
}
```
