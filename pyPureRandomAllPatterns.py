#generate patterns which are purely random -> not dosed files because
#efficiency is low enough that a mixed purity tset has been a bit
#conflated... generates all the patterns in a row because sampling doesn't
# seem useful when there are certain bad patterns that DETERMINISTICALLY
# FIRE THE TRIGGER

import sys, random, math, os, glob

#half is 'T' or 'B'
def main(half):

    if(half not in ['T','B']):
        print "please give specced half of det..."
        exit()
    if(half == 'T'):
        roadhalfname = "top"
    else:
        roadhalfname = "bottom"

    nonroad_file = "non_road_randoms"+"/random_patterns_" +roadhalfname+".txt"

    nonroad_patterns = []

    with open(nonroad_file,"r") as fp:
        for idx,line in enumerate(fp):
            nonroad_patterns.append(line)

    #print in chunks of max 10
    num_files = math.ceil(len(nonroad_patterns)/10.)
    for i in range(0,int(num_files)):
        lines_written=0
        randomfilename='allrandompatterns_'+roadhalfname+'/randompattern_'+str(i).zfill(2)
        print randomfilename
        with open(randomfilename,'w+') as fp:
            for j in range(i*10,i*10+10):
                if j < len(nonroad_patterns):
                    # all roads get 2 lines then 2 lines of zero buffers
                    # due to clocking...
                    # this is all very clumsy...
                    fp.write(nonroad_patterns[j])
                    fp.write(nonroad_patterns[j])
                    for k in range(0,2):
                        for l in range(0,6):
                            fp.write('0000')
                            fp.write(',') 
                        fp.seek(-1,1)
                        fp.write('\n')
                    lines_written = lines_written+4
                else:
                    break
            #clunky way to say fill rest of file...
            for k in range(lines_written,256):
                for h in range(0,6):
                    fp.write('0000')
                    fp.write(',') 
                fp.seek(-1,1)
                fp.write('\n')

#python is dumb
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "usage: python pyPatternGen <half- 'T' or 'B'> "
        exit()
    else:
        half = sys.argv[1]
    main(half)

