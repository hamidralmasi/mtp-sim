#!/bin/bash
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=16 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log16_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=32 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log32_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=64 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log64_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=128 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log128_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=256 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log256_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=512 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log512_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=1024 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log1024_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=2048 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log2048_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=4096 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log4096_100ms_hdctcp.out 2>&1
NS_LOG="Hdctcp" ./waf --run "hdctcp --simulationEndTime=0.1 --timePeriod=8192 --senderToSwitchBW=100Gbps --senderToSwitchDelay=1us --switchFirstPathBW=100Gbps --switchFirstPathDelay=1us --switchSecondPathBW=10Gbps --switchSecondPathDelay=1.5us --minTh=50 --maxTh=150 --bufferSize=2666p --tracing=false" > log8192_100ms_hdctcp.out 2>&1