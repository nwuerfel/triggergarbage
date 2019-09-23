# another stupid program that takes a pulse pattern and tells you what the
# chans are...
# should be combined with pattern gen but I don't think far ahead or write
# good code...

import sys

#again stupid but format is [[hodo,inputchan],...] indexed by TDC word
maplines = []
# stupid but w/e format of data is [[roadid,hodo_pattern_word,pxbin,pxmin],
# ...]
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

# currently only does positive top roads, need to make more general...
# take input pattern and make it 8digit as h1h2h3h4 to compare with roadset
def HodoToPxBin(hodo_pattern_word):
    found = 0
    PxBin = -1
    PxMin = -1
    for idx, road in enumerate(L1roadset):
        if hodo_pattern_word == road[1]:
            found = 1      
            PxBin = road[2]
            PxMin = road[3]
            break
    if not found:
        print 'didnt find the pattern word in roadset'
    return PxBin,PxMin

def main(half, charge):

    hexroads = []
    roads = []
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

    with open('pattern.txt') as fp:
        lines = fp.readlines()
        num_lines = len(lines)
        for line in lines:
            hexroad = []
            road = []
            L1road = []
            line = line.split(',')
            line = [x.strip() for x in line]
            hexroad.extend(line)
            hexroads.append(hexroad)
            for idx,word in enumerate(line):
                # disgusting
                if idx == 0 or idx == 1:
                    char = "E" 
                    L1char = "A"
                elif idx == 2 or idx == 3:
                    char = "F"
                    L1char = "B"
                elif idx == 4 or idx == 5:
                    char = "C"
                    L1char = "D"
                if idx%2:
                    offset = 16
                else:
                    offset = 0

                binrep = format(int(word,16),'016b')
                high_bits = []
                for bitdx,bit in enumerate(binrep):
                    if int(bit) == 1:
                        high_bits.append(15-bitdx); 
                for bit in high_bits:
                    road.append(str(char + str(int(bit) + offset)))
                    L1road.append(str(L1char + str((int(bit)+offset + 16)%32)))
            roads.append(road)
            L1roads.append(L1road)
                
    for i in range(0,num_lines,2):
        if roads[i]:
            print 
            # each road runs for 4 lines (2 on 2 off...)
            print "ROAD #" + str(i/4)
            print "~~~~~~~~"
            print 'Hexmem:',
            for word in hexroads[i]:
                print word,
            print
            print 'L0 Output Port: ',
            for word in roads[i]:
                print word,
            print
            print 'L1 Input Port: ',
            for word in L1roads[i]:
                print word,
            print
            print 'Hodoscope hit: ',
            #sorted h1 -> h4
            hodo_chans,patternword = L1roadToHodo(L1roads[i])
            for word in hodo_chans:
                print word,
           # print
           # print 'PxBin, PxMin: ', 
           # PxBin, PxMin = HodoToPxBin(patternword)
           # print PxBin + ', ' + PxMin,
           # print 
           # print

    del maplines[:]
    del L1roadset[:]
            

#python is dumb
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print "defaulting to top positive..."
        print "usage: python pyPattern <half- 'T' or 'B'> <charge- 'p'or 'm'> "
        half = 'T'
        charge = 'p'
    else:
        half = sys.argv[1]
        charge = sys.argv[2]
        print half, charge
    main(half, charge)
