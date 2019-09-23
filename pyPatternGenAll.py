import pyPatternGen

def main():
    roads_in_each = []
    for half in ['T','B']:
        for charge in ['p','m']:
            roads = pyPatternGen.main(half,charge)
            roads_in_each.append([half,charge,roads])

    with open("pyGenAll_results.txt","w+") as fp:
        for data in roads_in_each:
            fp.write("half: " + str(data[0]) + ", charge: " + str(data[1]) + ", num roads: " + str(data[2]))
            fp.write('\n')
#python is dumb
if __name__ == "__main__":
    main()
