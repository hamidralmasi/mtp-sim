inFiles = ['1-MTSQ-QueueComposition-10-64-1-8-0.1.dat',
           '2-MTSQ-QueueComposition-16-64-1-8-0.1.dat',
           '3-MTSQ-QueueComposition-10-64-10-80-0.1.dat',
           '4-MTSQ-QueueComposition-16-64-10-80-0.1.dat',
           '5-MTSQ-QueueComposition-20-128-1-8-0.1.dat',
           '6-MTSQ-QueueComposition-32-128-1-8-0.1.dat',
           '7-MTSQ-QueueComposition-20-128-10-80-0.1.dat',
           '8-MTSQ-QueueComposition-32-128-10-80-0.1.dat',
           '9-MTSQ-QueueComposition-40-256-1-8-0.1.dat',
           '10-MTSQ-QueueComposition-64-256-1-8-0.1.dat',
           '11-MTSQ-QueueComposition-40-256-10-80-0.1.dat',
           '12-MTSQ-QueueComposition-64-256-10-80-0.1.dat',
           '13-MTSQ-QueueComposition-80-512-1-8-0.1.dat',
           '14-MTSQ-QueueComposition-128-512-1-8-0.1.dat',
           '15-MTSQ-QueueComposition-80-512-10-80-0.1.dat',
           '16-MTSQ-QueueComposition-128-512-10-80-0.1.dat',
           '17-MTSQ-QueueComposition-160-1024-1-8-0.1.dat',
           '18-MTSQ-QueueComposition-256-1024-1-8-0.1.dat',
           '19-MTSQ-QueueComposition-160-1024-10-80-0.1.dat',
           '20-MTSQ-QueueComposition-256-1024-10-80-0.1.dat',
           '21-MTSQ-QueueComposition-320-2048-1-8-0.1.dat',
           '22-MTSQ-QueueComposition-512-2048-1-8-0.1.dat',
           '23-MTSQ-QueueComposition-320-2048-10-80-0.1.dat',
           '24-MTSQ-QueueComposition-512-2048-10-80-0.1.dat',
           '25-MTSQ-QueueComposition-640-4096-1-8-0.1.dat',
           '26-MTSQ-QueueComposition-1024-4096-1-8-0.1.dat',
           '27-MTSQ-QueueComposition-640-4096-10-80-0.1.dat',
           '28-MTSQ-QueueComposition-1024-4096-10-80-0.1.dat']


outFiles = ['1-MTSQ-QueueComposition-10-64-1-8-0.1-Out.txt',
           '2-MTSQ-QueueComposition-16-64-1-8-0.1-Out.txt',
           '3-MTSQ-QueueComposition-10-64-10-80-0.1-Out.txt',
           '4-MTSQ-QueueComposition-16-64-10-80-0.1-Out.txt',
           '5-MTSQ-QueueComposition-20-128-1-8-0.1-Out.txt',
           '6-MTSQ-QueueComposition-32-128-1-8-0.1-Out.txt',
           '7-MTSQ-QueueComposition-20-128-10-80-0.1-Out.txt',
           '8-MTSQ-QueueComposition-32-128-10-80-0.1-Out.txt',
           '9-MTSQ-QueueComposition-40-256-1-8-0.1-Out.txt',
           '10-MTSQ-QueueComposition-64-256-1-8-0.1-Out.txt',
           '11-MTSQ-QueueComposition-40-256-10-80-0.1-Out.txt',
           '12-MTSQ-QueueComposition-64-256-10-80-0.1-Out.txt',
           '13-MTSQ-QueueComposition-80-512-1-8-0.1-Out.txt',
           '14-MTSQ-QueueComposition-128-512-1-8-0.1-Out.txt',
           '15-MTSQ-QueueComposition-80-512-10-80-0.1-Out.txt',
           '16-MTSQ-QueueComposition-128-512-10-80-0.1-Out.txt',
           '17-MTSQ-QueueComposition-160-1024-1-8-0.1-Out.txt',
           '18-MTSQ-QueueComposition-256-1024-1-8-0.1-Out.txt',
           '19-MTSQ-QueueComposition-160-1024-10-80-0.1-Out.txt',
           '20-MTSQ-QueueComposition-256-1024-10-80-0.1-Out.txt',
           '21-MTSQ-QueueComposition-320-2048-1-8-0.1-Out.txt',
           '22-MTSQ-QueueComposition-512-2048-1-8-0.1-Out.txt',
           '23-MTSQ-QueueComposition-320-2048-10-80-0.1-Out.txt',
           '24-MTSQ-QueueComposition-512-2048-10-80-0.1-Out.txt',
           '25-MTSQ-QueueComposition-640-4096-1-8-0.1-Out.txt',
           '26-MTSQ-QueueComposition-1024-4096-1-8-0.1-Out.txt',
           '27-MTSQ-QueueComposition-640-4096-10-80-0.1-Out.txt',
           '28-MTSQ-QueueComposition-1024-4096-10-80-0.1-Out.txt']



f_count = 0
QueueLenStep = 100


for f in inFiles:
    inputFile = open(f, 'r')
    Lines = inputFile.readlines()
    outputFile = open(outFiles[f_count], 'w')
    f_count = f_count + 1
    count = 0
    tempSum_t1 = float(0)
    tempSum_t2 = float(0)
    for line in Lines:
        lineSplit = line.split()
        time = lineSplit[0]
        t1_packets = lineSplit[1]
        t2_packets = lineSplit[2]

        tempSum_t1 = tempSum_t1 + float(t1_packets)
        tempSum_t2 = tempSum_t2 + float(t2_packets)

        count = count + 1
        if (count == QueueLenStep):
            # outputFile.write(time + " " + str(tempSum_t1)+ " " + str(tempSum_t2) +"\n")
            if ( (tempSum_t1 + tempSum_t2) > 0):
                outputFile.write(str(tempSum_t1/(tempSum_t1 + tempSum_t2))+ "\n")
            tempSum_t1 = 0
            tempSum_t2 = 0
            count = 0

    outputFile.close()

