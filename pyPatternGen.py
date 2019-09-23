# stupid program to take roadsets and generate pulse pattern files 
# requires a directory called 'patterns' in this dir 

import math
import sys

# format is [[hodo,inputchan],...] indexed by TDC word
maplines = []
# format is [[roadid,hodo_pattern_word,pxbin,pxmin], ...]
L1roadset = []

def readL1Map(filename):
    print "reading mapping for " + filename
    with open(filename) as fp:
        mapping = fp.readlines()
        mapping = mapping[1:]
        for line in mapping:
            line = line.strip().split(',')
            global maplines
            maplines.append(line[1:3])

# currently only using 1/4 of the roads for a set (P&T because that's what
# my test bench worked with...)
# data is [roadid, hodo1 chan, hodo2 chan, hodo3 chan, hodo4 chan, charge,
# px bin, px min and then crap idk what it is]
def readL1roadset(filename):
    print "reading roadset for " + filename
    with open(filename) as fp:
        roads = fp.readlines() 
        for rawroad in roads: 
            hodo_pattern_word = ''
            rawroad = rawroad.strip().split()
            rawroad = rawroad[0:8]
            roadid = rawroad[0]
            for i in range(1,5):
                hodo_pattern_word += rawroad[i].zfill(2)
            PxBin = rawroad[6]
            PxMin = rawroad[7]
            road = [roadid, hodo_pattern_word, PxBin, PxMin]
            global L1roadset
            L1roadset.append(road)
    return len(L1roadset)
            

# terrible... hardcodes the mapping in but I don't anticipate the mapping
# changing anytime soon. (but if it does we're sunk)
# also bad practice to associate information with array index but here we
# are, the list index ends up giving the tdc channel, and then each element
# in the list is a tuple containing the hodo + input chan
# done this way ebcause memory readout gives us TDC chan info
# need to refactor this
def L1roadToHodo(L1road):
    unmapped = []
    hodo_pattern_word = ''
    print "unmapping assuming pos, top roads ",    
    for word in L1road:
        port = word[0]
        if port == 'D':
            offset = 0
        elif port == 'B':
            offset = 32
        elif port == 'A':
            offset = 64
        chan = int(word[1:])
        tdc_count = chan+offset
        hodo = maplines[tdc_count][0]
        unmapped.append(hodo)
        
    unmapped = sorted(unmapped, key = lambda x: x[1])

    #force to form h1h2h3h4 with 2 dig for each so each road has 8 dig
    #for later comparison
    for word in unmapped[:4]:
        hodo_pattern_word += word[-2:]

    return unmapped,hodo_pattern_word

def chanForHodoHit(hit):
    found = 0
    for mapping in maplines:    
        if mapping[0] == hit:
            found = 1
            port = mapping[1]
            break
    if not found:
        print 'didnt find the pattern word in roadset'
        print mapping[0]
        print hit
    return port

#half is 'T' or 'B', charge is 'p' or 'm'
def main(half, charge):
    patterns = []
    roads = []
    roadct = 0 
    L1roads = []
    scale = 16
    word_len=4

    if(half not in ['T','B']):
        print "please give specced half of det..."
        exit()
    if(charge not in ['p','m']):
        print "please give specced charge"
        exit()
    if(half == 'T'):
        roadhalfname = "top"
        boardmapnum = "460"
    else:
        roadhalfname = "bottom"
        boardmapnum = "470"
    if(charge == 'p'):
        roadcharge = "plus"
    else:
        roadcharge = "minus"

    boardmap = boardmapnum + "mapping.txt"
    roadsetname = 'roads_'+roadcharge+'_'+roadhalfname+'.txt'

    readL1Map(boardmap)
    num_roads = readL1roadset(roadsetname)

    print "programmed by a shmuck..."
    print "mappingfile: " + boardmap + " roadset: " + roadsetname
    print "board: " + str(boardmapnum) + " half: " + roadhalfname + " charge: " + str(roadcharge)

    print "I see " + str(num_roads) + " roads..."

    # convert all roads to patterns and save then we'll write out later in
    # chunks
    for road in L1roadset:        
        pattern = []
        hodohits = []
        L1chans = []
        L0chans = []
        L0chansSorted = []
        hodopattern = road[1]
        #print hodopattern
        for i in range(1,5):
            chan = hodopattern[2*(i-1):2*i]
            hodoword = "H" + str(i) + half
            if i == 4: 
                hodoword = hodoword + "u"
            hodoword = hodoword + str(chan.zfill(2))
            hodohits.append(hodoword)
        # H4 has u and d hodos sucks...
        # corner cases for days
        hodoword = hodoword[0:3] + "d" + str(chan.zfill(2))
        hodohits.append(hodoword)
        #print hodohits

        for hit in hodohits:
            chan = chanForHodoHit(hit)
            L1chans.append(chan)
        #print L1chans

        # garbo code to take L1 inports to L0 outputs 
        for chan in L1chans:
            if chan[0] == 'A':
                L0char = 'E'
            elif chan[0] == 'B':
                L0char = 'F'
            elif chan[0] == 'D':
                L0char = 'C'
            L0num = (int(chan[1:]) + 16) % 32
            L0chan = L0char + str(L0num)
            L0chans.append(L0chan)
        #print L0chans

        # more garbo code to take L0 outputs to patterns
        # not assuming the ordering, patterns need to be E_low, E_High,
        # F_Low, F_high, C_Low, C_HIGH
        # annoying 
        for chan in L0chans:
            if chan[0] == 'E':
                majidx = 0
            elif chan[0] == 'F':
                majidx = 2
            elif chan[0] == 'C':
                majidx = 4
            if int(chan[1:]) > 15:
                idx = majidx + 1
            else: 
                idx = majidx
            L0chansSorted.append([chan,idx])
        
        L0chansSorted = sorted(L0chansSorted, key = lambda x: x[1])
        #print L0chansSorted

        # generate bitwise patterns
        indxpat = []
        for chan in L0chansSorted:
            patword = ''
            num = chan[0]
            num = int(num[1:]) % 16
            idx = chan[1]
            # what a terrible thing I've done to generate bitwise patterns
            for i in range(0,16):
                if i == num:
                    patword = patword + '1'
                else:
                    patword = patword + '0'
            #then need to flip cuz I'm dumb and MSB on LHS
            patword = ''.join(reversed(patword))
            patword = format(int(patword,2),'04x')
            indxpat.append([patword,idx])
        #print indxpat

        # ok with pattern we need fillers and to add patterns with same
        # index...
        for i in range(0,6):
            patword = format(int('0',16),'04x')
            for chan in indxpat:
                if int(chan[1]) == i:
                    patword = format(int(patword,16) + int(chan[0],16),'04x')
            pattern.append(patword)
        #print pattern

        patterns.append(pattern)
        


    #print in chunks of max 10
    num_files = math.ceil(num_roads/10.)
    for i in range(0,int(num_files)):
        lines_written=0
        print 'patterns_'+roadhalfname+'/pattern_'+roadcharge+'_'+ roadhalfname +'_'+str(i).zfill(2)
        with open('patterns_'+roadhalfname+'/pattern_'+roadcharge+'_'+ roadhalfname +'_'+str(i).zfill(2)
            ,'w+') as fp:
            for j in range(i*10,i*10+10):
                if j < num_roads:
                    # all roads get 2 lines then 2 lines of zero buffers
                    # due to clocking...
                    # this is all very clumsy...
                    for k in range(0,2):
                        for word in patterns[j]:
                            fp.write(word)
                            fp.write(',') 
                        fp.seek(-1,1)
                        fp.write('\n')
                    for k in range(0,2):
                        for word in patterns[j]:
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
    del maplines[:]
    del L1roadset[:]
    return num_roads


#python is dumb
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "defaulting to top positive..."
        print "usage: python pyPatternGen <half- 'T' or 'B'> <charge- 'p'or 'm'> "
        half = 'T'
        charge = 'p'
    else:
        half = sys.argv[1]
        charge = sys.argv[2]
        print half, charge
    main(half, charge)

