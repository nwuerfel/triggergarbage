# stupid program that generates roads NOT in the roadset to provide a set
# from which to choose randomly to check the purity of the trigger....
# I suck at writing code so I copy pasted from pyPattern instead of making
# a library and importing because I didn't think thruogh things very
# well... I can always clean up and refactor all of this later...

#NOTE THE MAPPING FOR 420 -> 460 and 430 -> 470 is WRONG
# doesn't count the flip, so we can't just read L1 

#dif from L1 because I want the output port not input for this app
# [[hodoname,outputport],...]
def readL1Map(filename):
    maplines = []
    print "reading mapping for " + filename
    with open(filename) as fp: 
        mapping = fp.readlines()
        mapping = mapping[1:]
        for line in mapping:
            line = line.strip().split(',')
            maplines.append(line[1:3])
    return maplines


def readL1roadset(filename):
    L1roadset = []
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
            L1roadset.append(road)

    return L1roadset

# L0 mapping was incorrect so I cluged this.... chan for Hodoword now gives
# the L1 PORT NAME (A B D) but the L0 output pin (what we need for pattern) 
# only significant if you go poking around in here....
def hodoWordsToPatterns(hodo_code_set, half):
    if half == 'T':
        mappingfile = "460mapping.txt"
    else:
        mappingfile = "470mapping.txt"

    maplines = readL1Map(mappingfile)
    hodohits = []
    channel_set = []
    channels = []
    chans_sorted = []
    pattern_set = []

    for code in hodo_code_set:
        #code to channels
        del channels[:]
        del hodohits[:]
        del chans_sorted[:]
        pattern = []
        for i in range(1,5):
            chan = code[2*(i-1):2*i]
            hodoword = "H" + str(i) + half
            if i == 4: 
                hodoword = hodoword + "u"
            hodoword = hodoword + str(chan.zfill(2))
            hodohits.append(hodoword)
        # H4 has u and d hodos sucks...
        # corner cases for days
        hodoword = hodoword[0:3] + "d" + str(chan.zfill(2))
        hodohits.append(hodoword) 
        for word in hodohits:
            channel = chanForHodoWord(word,maplines)
            channels.append(channel)

        # channels to patterns
        for chan in channels:
            if chan[0] == 'A':
                majidx = 0 
            elif chan[0] == 'B':
                majidx = 2 
            elif chan[0] == 'D':
                majidx = 4 
            if int(chan[1:]) > 15: 
                idx = majidx + 1 
            else: 
                idx = majidx
            chans_sorted.append([chan,idx])
            
        chans_sorted = sorted(chans_sorted, key = lambda x: x[1])

        # generate bitwise patterns
        indxpat = []
        for chan in chans_sorted:
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
        pattern_set.append(pattern)
 
    return pattern_set
    
# searches L0 mapping for hodoword to get chan 
# NOTE BIG FLIP IN CHAN BECAUSE mapping from L0 to L1 is WRONG
def chanForHodoWord(hodoword,maplines):
    found = 0 
    for mapping in maplines:        
        if mapping[0] == hodoword:
            found = 1 
            port = mapping[1]
            chan_num = (int(port[1:]) + 16) % 32
            port = port[0] + str(chan_num)
            break
    if not found:
        print 'didnt find the pattern word in roadset'
        print mapping[0]
        print hodoword 
    return port    

def main():
    
    # lazy but memory is cheap now
    non_road_randoms_top = []
    non_road_randoms_bot = []
    all_hodo_words = []
    top_roads = []
    bot_roads = []

    # be lazy, read all roads and then just filter hodowords
    road_set = readL1roadset("roads_plus_top.txt")
    top_roads = top_roads+road_set
    road_set = readL1roadset("roads_minus_top.txt")
    top_roads = top_roads+road_set
    road_set = readL1roadset("roads_plus_bottom.txt")
    bot_roads = bot_roads+road_set
    road_set = readL1roadset("roads_minus_bottom.txt")
    bot_roads = bot_roads+road_set

    # just take hodowords 
    all_top_roadwords = [x[1] for x in top_roads]
    all_bot_roadwords = [x[1] for x in bot_roads]
    
    print str(len(all_top_roadwords))
    print str(len(all_bot_roadwords))

    # we'll generate all possible hodowords and then kill them if they're
    # valid roads so we end up with only non road patterns
    # h1 runs from 01 to 023, h2,3,4 all have 01 to 16
    # 23* 16 * 16 *16 = 94208 patterns to gen and loop through * 4 bytes
    # per 'int' rep = 376kb max - like 1000 roads so we have a big array at
    # the end of the day but who cares when memory is cheap and big
    for h1_idx in range(1,24):
        for h2_idx in range(1,17):
            for h3_idx in range(1,17):
                for h4_idx in range(1,17):
                    word = str(h1_idx).zfill(2) + str(h2_idx).zfill(2) + \
                        str(h3_idx).zfill(2) + str(h4_idx).zfill(2) 
                    if word not in all_top_roadwords:
                        non_road_randoms_top.append(word)                    
                    if word not in all_bot_roadwords:
                        non_road_randoms_bot.append(word)

    # convert words to patterns

    # in top
    # rather than go frmo L1 to L0 and all that jazz just read L0 mapping
    top_pattern_set = hodoWordsToPatterns(non_road_randoms_top,'T')
    bot_pattern_set = hodoWordsToPatterns(non_road_randoms_bot,'B')
   
    with open("random_patterns_top.txt",'w+') as fp:
        for pattern in top_pattern_set:
            for word in pattern:
                fp.write(word)
                fp.write(',')
            fp.seek(-1,1)
            fp.write('\n')

    with open("random_patterns_bottom.txt", 'w+') as fp:
        for pattern in bot_pattern_set:
            for word in pattern:
                fp.write(word)
                fp.write(',')
            fp.seek(-1,1)
            fp.write('\n')

if __name__ == "__main__":
    main()
