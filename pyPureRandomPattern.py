#generate patterns which are purely random -> not dosed files because
#efficiency is low enough that a mixed purity tset has been a bit
#conflated...

import sys, random, math, os, glob

#half is 'T' or 'B', charge is 'p' or 'm'
def main(half,num_randoms):

    if(half not in ['T','B']):
        print "please give specced half of det..."
        exit()
    if(half == 'T'):
        roadhalfname = "top"
    else:
        roadhalfname = "bottom"

    num_randoms = int(num_randoms)    

    nonroad_file = "non_road_randoms"+"/random_patterns_" +roadhalfname+".txt"

    nonroad_patterns = []

    with open(nonroad_file,"r") as fp:
        total_nonrand_roads=sum(1 for line in fp.readlines())
        fp.seek(0)
        for i in range(0,num_randoms):
            fp.seek(0)
            rand_nonroad_num = random.randint(0,total_nonrand_roads-1)
            for idx,line in enumerate(fp):
                if idx==rand_nonroad_num:
                    nonroad_patterns.append(line)

    #print in chunks of max 10
    num_files = math.ceil(len(nonroad_patterns)/10.)
    for i in range(0,int(num_files)):
        lines_written=0
        randomfilename='randompatterns_'+roadhalfname+'/randompattern_'+str(i).zfill(2)
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
    if len(sys.argv) < 3:
        print "usage: python pyPatternGen <half- 'T' or 'B'> <num_randoms> "
        exit()
    else:
        half = sys.argv[1]
        num_randoms = sys.argv[2]
    main(half, num_randoms)

