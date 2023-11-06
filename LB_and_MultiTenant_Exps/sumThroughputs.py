inputFile = open('MTSQ-instantThroughput-T2-0.1.dat', 'r')
Lines = inputFile.readlines()
outputFile = open('MTSQ-instantThroughput-T2-0.1-Out.txt', 'w')
count = 0
numFlowsTenant = 80
tempSum = float(0)


for line in Lines:
    lineSplit = line.split()
    flowID = lineSplit[0]
    time = lineSplit[1]
    throughput = lineSplit[2]

    tempSum = tempSum + float(throughput)
    count = count + 1
    if (count == numFlowsTenant):
        outputFile.write(time + " " + str(tempSum)+"\n")
        tempSum = 0
        count = 0

outputFile.close()

