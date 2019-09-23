#stupid program to choose random  files and 'dose' them with random roads
# to test the purity of the system...

import sys, random, math, os, glob
from shutil import copyfile

#half is 'T' or 'B', charge is 'p' or 'm'
def main(half,num_randoms):

    # figure out how many patterns exist in each half
    num_roads = 0
    num_plus = 0
    num_neg = 0
    with open("pyGenAll_results.txt") as fp:
        lines = fp.readlines()
        for line in lines:
            line = line.strip().split(',')
            road_half = line[0].split(':')
            road_half = road_half[1]
            charge = line[1].split(':')
            charge = charge[1]
            num = line[2].split(':')
            num = int(num[1])
            if road_half == ' ' + half:
                num_roads = num_roads + num
            if charge == ' p':
                num_plus = num
            if charge == ' m':
                num_neg = num

    if(half not in ['T','B']):
        print "please give specced half of det..."
        exit()
    if(half == 'T'):
        roadhalfname = "top"
    else:
        roadhalfname = "bottom"

    num_randoms = int(num_randoms)    

    num_inject = min(num_plus,num_neg)
    print num_inject

    if(num_randoms > num_inject): 
        print "please give less randoms than there are roads..."
        #TODO fix num_roads here
        print "there are: " + str(num_roads) + " in that half, you gave: " + num_randoms
        exit()

    nonroad_file = "non_road_randoms"+"/random_patterns_" +roadhalfname+".txt"

    nonroad_patterns=[]

    road_randoms_replaced=[]
    rand_road_num = -1

    with open(nonroad_file,"r") as fp:
        total_nonrand_roads=sum(1 for line in fp.readlines())
        print "Dosing with random roads..."
        fp.seek(0)
        for i in range(0,num_randoms):
            fp.seek(0)
            while(1):
                rand_road_num = random.randint(0,num_inject-1)
                if rand_road_num not in road_randoms_replaced:
                    break
            road_randoms_replaced.append(rand_road_num)
            rand_nonroad_num = random.randint(0,total_nonrand_roads-1)
            for idx,line in enumerate(fp):
                if idx==rand_nonroad_num:
                    nonroad_patterns.append([rand_road_num,line])
        
    #pick random charge...    
    charges=['minus','plus']


    #slow cuz lots of IO could buffer this for efficiency later but w/e
    # I also don't understand how to replace a line so I write to new file 
    for pattern in nonroad_patterns:
        roadcharge=charges[random.randint(0,1)]
        rand_road_file = pattern[0]
        # way my patterns are split
        rand_road_file = int(math.floor(rand_road_file/10.))
        rand_road_file_line = int(pattern[0] % 10)
        rand_road_file_name = "patterns_" + roadhalfname+'/pattern_'+roadcharge+'_'+roadhalfname+'_'+str(rand_road_file).zfill(2)
        new_file_name = "dosedpatterns_" + roadhalfname+'/dosedpattern_'+roadcharge+'_'+roadhalfname+'_'+str(rand_road_file).zfill(2)
        with open(rand_road_file_name,"r+") as randfp:
            with open(new_file_name,"w+") as newfp:
                for idx,line in enumerate(randfp):
                    randfp.write("junk")
                    if idx == rand_road_file_line*4:
                        newfp.write(pattern[1])
                    elif idx == rand_road_file_line*4 + 1:
                        newfp.write(pattern[1])
                    else:
                        newfp.write(line)

    #copy over other files that didn't get dosed: 
    filedir = os.getcwd()+'/patterns_'+roadhalfname+'/'
    dosedfildir = os.getcwd()+'/dosedpatterns_'+roadhalfname+'/'
    files = [f for f in glob.glob(filedir+'*')]
    dosedfiles = [f for f in glob.glob(dosedfildir+'*')]
    # make list of charge and nums to compare
    # index is 1 to 1 with files and dosed files lists
    orgfilinfo = [[f[-12:-7].strip('_'),f[-2:]] for f in files]
    dosedfilinfo = [[f[-12:-7].strip('_'),f[-2:]] for f in dosedfiles]

    for idx, info in enumerate(orgfilinfo):
        # that file didn't get doesed -> copy it over so the dosed files
        # have full listing.... we're totally blind to the dosing but know
        # exactly how many should fail: random tests
        if info not in dosedfilinfo:
            copyfile(filedir+os.path.basename(files[idx]), \
                dosedfildir+'dosed'+os.path.basename(files[idx]))

    


#python is dumb
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "usage: python pyPatternGen <half- 'T' or 'B'> <num_randoms> "
        exit()
    else:
        half = sys.argv[1]
        num_randoms = sys.argv[2]
    main(half, num_randoms)

