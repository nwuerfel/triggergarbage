#include "v1495trashLib.h"
/*
 *
 *
 * Test functions for the roadtests -> after 9/7/19 values changed for the
 * 5 board setup....
 *
 *
 *
 */ 

#define NUM_BOARDS 5

/* Import external Functions */
IMPORT STATUS sysBusToLocalAdrs (int, char *, char **);
IMPORT STATUS intDisconnect (int);
IMPORT STATUS sysIntEnable (int);
IMPORT STATUS sysIntDisable (int);

/*
 *
 *  Function to test the output of the L0 pulser memory without any
 *  coordination signal from the L2 board. This just writes 0x0003 to the
 *  f_data_l userland register. This signal is really 0x0001 which sets the
 *  muxes to use pulser output rather than passing A,B,D inputs straight
 *  through, plus 0x0002 which is the contiuous run command for the pulsers
 *  so they keep repeating output -> I can find it on an oscilliscope
 *
 * -NW 8/8/19
 *
 */

void L0lonelyPulse(int id0, int win0){

    STATUS err;

    printf("Beginning L0 independent pulse mem test...\n");
    printf("Lonely because no L2 coordnation or friends... \n");

    // initialize all boards
    printf("Initializing boards...\n");
    v1495Init(0x04000000,0x00100000,NUM_BOARDS);
    printf("All boards initialized...\n");

    //Set delays for all 3 boards
    printf("Setting delays...\n");
    err = v1495Timeset(96, id0);
    if(err == ERROR){
        printf("Failed to timeset 0\n");
    }

    printf("setting gross delays and time windows...\n");
    err = v1495TimewindowSet(id0, win0);
    if(err == ERROR){
        printf("Failed to timewindowset 0\n");
    }

    printf ("v1495RoadPulser(): Delays and Windows set for boards %d.\n", id0);

    //Set Level 0 pattern
    printf("setting simple pattern in level 0...\n");
    err = v1495PatternSet(0,id0,"");
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
    }

    //Start Pulser
    printf("setting f_data_l to 0x0003 should be cont run mode...\n");
    L0ContPulse(0);
    return;
}

/*
 *
 * function to test the start and stop signal generation from the L2 output
 * to F port which coordinates the start pulse @ L0 and stop tdc signal
 * which emulates the trigger interface 'common stop' @ L1
 *
 *
 * -NW 8/7/19
 *
 */

void L2startStopTest(int id0,  int id2, int win0, int win2 ){

    STATUS err;

    printf("Beginning Start/Stop signal Test...\n");

    // initialize all boards
    printf("Initializing boards...\n");
    v1495Init(0x04000000,0x00100000,3);
    printf("All boards initialized...\n");

    //Set delays for all 3 boards
    printf("Setting delays...\n");
    err = v1495Timeset(96, id0);
    if(err == ERROR){
        printf("Failed to timeset 0\n");
    }

    err = v1495Timeset(96, id2);
    if(err == ERROR){
        printf("Failed to timeset 2\n");
    }

    printf("setting gross delays and time windows...\n");
    err = v1495TimewindowSet(id0, win0);
    if(err == ERROR){
        printf("Failed to timewindowset 0\n");
    }

    err = v1495TimewindowSet(id2, win2);
    if(err == ERROR){
        printf("Failed to timewindowset 2\n");
    }

    printf ("v1495RoadPulser(): Delays and Windows set for boards %d, \
        and %d.\n", id0, id2);

    //Set Level 2 Pattern
    printf("setting start and stop pattern in level 2...\n");
    v1495PatternSet(2,id2,"");

    //Start Pulser
    printf("setting f_data_h to 0x0002 should be cont run mode...\n");
    L2ContPulse(id2);
    return;
}

/*
 *
 * coordinates the L2 and L0 boards to show proof of concept, signal sent
 * AND received -> memory dump on the L0. Basically the above plus a single
 * line which is repetative and dumb coding, but w/e it's good to have them
 * all separate so that one can clearly see the steps to each part of this
 * monster...
 *
 * again, assumes a 2 board setup where id's are the same as the levels
 *
 * -NW 8/8/19
 *
 */

void L2L0startTest(int id0, int id2, int win0, int win2 ){

    STATUS err;

    printf("Beginning Start coordination signal Test...\n");
    printf("expect to see start from L2, then mem dump from L0");

    // initialize all boards
    printf("Initializing boards...\n");
    v1495Init(0x04000000,0x00100000,3);
    printf("All boards initialized...\n");

    //Set delays for L0 and L2 
    printf("Setting delays...\n");
    err = v1495Timeset(96, id0);
    if(err == ERROR){
        printf("Failed to timeset 0\n");
    }
    err = v1495Timeset(96, id2);
    if(err == ERROR){
        printf("Failed to timeset 2\n");
    }

    // set time windows for L0 and L2
    printf("setting gross delays and time windows...\n");
    err = v1495TimewindowSet(id0, win0);
    if(err == ERROR){
        printf("Failed to timewindowset 0\n");
    }
    err = v1495TimewindowSet(id2, win2);
    if(err == ERROR){
        printf("Failed to timewindowset 2\n");
    }

    printf ("v1495RoadPulser(): Delays and Windows set for \
        boards %d and %d.\n", id0, id2);

    //Set Level 2 Pattern
    printf("setting start and stop pattern in level 2...\n");
    err = v1495PatternSet(2,id2,"");
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
    }

    // Set L0 pattern
    printf("setting simple pattern in level 0...\n");
    err = v1495PatternSet(0,id0,"");
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
       
    }

    // tell Mux to use pulser output rather than pass through in L0
    // TODO fix the stupid v1495ActivatePulser function... it introduces a
    // nanosleep which is extra time to account for on the oscilliscope 
    // for now doing it brute force way
    v1495[id0]->f_data_l = PULSER_WAKEUP_SIGNAL;

    // send start pulser signal
    printf("setting f_data_h to 0x0002 should be cont run mode...\n");
    L2ContPulse(id2);
    return;
}

// dump util for debugging tdc during pulser test
void v1495TDCdump (unsigned id){

    unsigned i;
    unsigned short read;
    char dump_file_name[MAX_FILE_NAME_LENGTH];
    FILE* dump_file;

     // 0 indexed 
    if(id > MAX_NUM_1495s - 1){
        printf("ERROR: please give a valid board_id from 0 to 8...\n");
        return;
    }
    if(v1495[id] == NULL){
        printf("ERROR: no board initialized with that id...\n");
        return;
    }
    if(v1495t[id] == NULL){
        printf("ERROR: no tdc buff initialized with that id...\n");
        return;
    }

    sprintf(dump_file_name,"board_%d_tdc.dump",id);

    dump_file = fopen(dump_file_name, "a+");

    if(dump_file == NULL){
        printf("wtf I suck at opening files...\n");
        return;
    }

    fprintf(dump_file,"HEADER\n");
    for(i = 0; i < TDC_BUFFER_DEPTH; i++){
        read = v1495t[id]->tdc[i];
        fprintf(dump_file,"0x%x\n",read);
    }
    fprintf(dump_file,"FOOTER\n");

    printf("closing tdcdump file and dying... finally\n");
    fclose(dump_file);
}

// noah road test for 3 boards -> 400, 460, 480 ids are (0, 2, 4) 
//                             -> 410, 470, 480 ids are (1, 3, 4)
void L2RoadTest(int id0, int id1, int id2, int win0, int win1, int win2,
    const char* pattern_filename){

    STATUS err;

    printf("Beginning Road Test...\n");
    printf("expect to see start from L2, then mem dump from L0");

    // initialize all boards
    printf("Initializing boards...\n");
    v1495Init(0x04000000,0x00100000,NUM_BOARDS);
    printf("All boards initialized...\n");

    //Set delays for L0 and L2 
    printf("Setting delays...\n");
    err = v1495Timeset(96, id0);
    if(err == ERROR){
        printf("Failed to timeset %d\n", id0);
    }

    err = v1495Timeset(96, id1);
    if(err == ERROR){
        printf("Failed to timeset %d\n", id1);
    }

    err = v1495Timeset(96, id2);
    if(err == ERROR){
        printf("Failed to timeset %d\n", id2);
    }

    // set time windows for L0 and L2
    printf("setting gross delays and time windows...\n");
    err = v1495TimewindowSet(id0, win0);
    if(err == ERROR){
        printf("Failed to timewindowset %d\n", id0);
    }

    err = v1495TimewindowSet(id1, win1);
    if(err == ERROR){
        printf("Failed to timewindowset %d\n", id1);
    }

    err = v1495TimewindowSet(id2, win2);
    if(err == ERROR){
        printf("Failed to timewindowset %d\n", id2);
    }

    printf ("v1495RoadPulser(): Delays and Windows set for \
        boards %d and %d.\n", id0, id2);

    //Set Level 2 Pattern
    printf("setting start and stop pattern in level 2...\n");
    err = v1495PatternSet(2,id2,"");
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
    }

    // Set L0 pattern
    printf("setting simple pattern in level 0...\n");
    err = v1495PatternSet(0,id0,pattern_filename);
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
       
    }

    // choose mux output for L0 to send pulse instead of hodo feedthru
    v1495[id0]->f_data_l = PULSER_WAKEUP_SIGNAL;

    // start TDC
    v1495Run(id1);
    //v1495Run(id2);
    printf("I'm a delay print after running!\n");

    // send start pulser signal
    L2SinglePulse(id2);
    printf("I'm a print delay before dumping TDC to file!\n");
    //v1495TDCdump(1);
    return;
}


// same as above but 5 boards, full test...
void allBoardRoadTest(int win0, int win1, int win2, int win3, int win4,
    const char* pattern_filename_0, const char* pattern_filename_1){

    STATUS err;
    int i;
    int windows[NUM_BOARDS] = {win0,win1,win2,win3,win4};

    printf("Beginning Full, 5 board, Road Test...\n");

    // initialize all boards
    v1495Init(0x04000000,0x00100000,NUM_BOARDS);

    //Set delays 
    printf("Setting delays...\n");
    for(i=0; i<NUM_BOARDS;i++){
        err = v1495Timeset(96, i);
        if(err == ERROR){
            printf("Failed to timeset board %d\n", i);
        }
    }
    for(i=0;i<NUM_BOARDS;i++){
        err = v1495TimewindowSet(i, windows[i]);
        printf("set board id: %d with window %d\n",i,windows[i]);
        if(err == ERROR){
            printf("Failed to timewindowset %d\n", i);
        }
    }

    //Set Level 2 Pattern
    //printf("setting start and stop pattern in level 2...\n");
    err = v1495PatternSet(2,4,"");
    if(err == ERROR){
        printf("failed to set pattern...\n");
        return;
    }

    // Set L0 pattern
    printf("setting pattern in A boards...\n");
    err = v1495PatternSet(0,0,pattern_filename_0);
       if(err == ERROR){
            printf("failed to set pattern...\n");
            return;
       }
    v1495[0]->f_data_l = PULSER_WAKEUP_SIGNAL;
    err = v1495PatternSet(0,1,pattern_filename_1);
       if(err == ERROR){
            printf("failed to set pattern...\n");
            return;
       }
    v1495[1]->f_data_l = PULSER_WAKEUP_SIGNAL;


    // start TDC
    printf("starting TDC in B boards...\n");
    for(i=2;i<4;i++){ 
        v1495Run(i);
        // this print NEEDS TO BE IN THE LOOP BETWEEN RUN CALLS
        printf("I'm a delay print after running!\n");
    }
    printf("I'm another delay....\n");

    // send start pulser signal
    printf("setting f_data_h to 0x8000 should be single run mode...\n");
    L2SinglePulse(4);
    return;
} 
/*
 *
 * quick nonsense to run the pulse test with simple pattern a bunch of
 * times and dumping to a file
 *
 */ 

/*
void RoadTestRepeat(unsigned reps){
    unsigned i;
    for(i=0;i<reps;i++){
        allBoardRoadTest(0,0,4377,4377,4355,"");
    }
}
*/



// runs purely random roads for purity test...
void randomRoadTest(unsigned num_files){
    
    char pattern_file_name[MAX_FILE_NAME_LENGTH];
    unsigned i=0;

    for(i=0;i<num_files;i++){
        sprintf(pattern_file_name,"../pyTDC/randompatterns_top/randompattern_%02d",i);
        printf("%s\n",pattern_file_name);
        allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name,pattern_file_name);
    }
    printf("rand %d random roads, should see NO output\n",num_files*10);
}

void repeatRandomRoadTest(int numreps){
    int i=0;
    int numfiles = 50;
    for(i=0;i<numreps;i++){
        randomRoadTest(50);
    }
    printf("reps: %d and files: %d\n",numreps,numfiles);
}

void FPGA1Test(unsigned num_files){

    char pattern_file_name_0[MAX_FILE_NAME_LENGTH];
    char pattern_file_name_1[MAX_FILE_NAME_LENGTH];
    unsigned i=0;

    for(i=0;i<num_files;i++){
        sprintf(pattern_file_name_0,"../pyTDC/patterns_top/pattern_plus_top_%02d",i);
        sprintf(pattern_file_name_1,"../pyTDC/patterns_bottom/pattern_minus_bottom_%02d",i);
        printf("%s\n",pattern_file_name_0);
        printf("%s\n",pattern_file_name_1);
        allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name_0,pattern_file_name_1);
    }
    for(i=0;i<num_files;i++){
        sprintf(pattern_file_name_0,"../pyTDC/patterns_top/pattern_minus_top_%02d",i);
        sprintf(pattern_file_name_1,"../pyTDC/patterns_bottom/pattern_plus_bottom_%02d",i);
        printf("%s\n",pattern_file_name_0);
        printf("%s\n",pattern_file_name_1);
        allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name_0,pattern_file_name_1);
    }

    printf("first did pt_mb, then mt_pb, expect FPGA1 output %d, if all files full...\n",
        num_files * 2 * 10);

}

// top or bot set here cuz i'm dumb
void randomTestFilenum(unsigned num){
    char pattern_file_name[MAX_FILE_NAME_LENGTH];
    unsigned i=0;

    //sprintf(pattern_file_name,"../pyTDC/randompatterns_top/randompattern_%02d",num);
    sprintf(pattern_file_name,"../pyTDC/testdrawr/%01d.txt",num);
    printf("%s\n",pattern_file_name);
    allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name,pattern_file_name);
}

// LOTS of random files to search through.... want to sample
void randomTestFilenumRange(unsigned min, unsigned max){

    char pattern_file_name[MAX_FILE_NAME_LENGTH];
    unsigned i=0;

    for(i=min; i<max; i++){
        sprintf(pattern_file_name,"../pyTDC/allrandompatterns_top/randompattern_%02d",i);
        printf("%s\n",pattern_file_name);
        allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name,pattern_file_name);
    }
   

}

void FPGA1TestFilenum(unsigned num_0, unsigned num_1){

    char pattern_file_name_0[MAX_FILE_NAME_LENGTH];
    char pattern_file_name_1[MAX_FILE_NAME_LENGTH];

    sprintf(pattern_file_name_0,"../pyTDC/patterns_top/pattern_plus_top_%02d",num_0);
    sprintf(pattern_file_name_1,"../pyTDC/patterns_bottom/pattern_minus_bottom_%02d",num_1);
    printf("%s\n",pattern_file_name_0);
    printf("%s\n",pattern_file_name_1);
    allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name_0,pattern_file_name_1);

}

void dosedFPGA1Test(){

    int num_mb,num_mt,num_pb,num_pt = 0;
    int num_0, num_1,num_0_remainder,num_1_remainder = 0;
    int max_roads,expected_FPGA1,expected_FPGA4 = 0;
    int offset = 0;
    int i = 0;
    int j = 0;
    char pattern_file_name_0[MAX_FILE_NAME_LENGTH];
    char pattern_file_name_1[MAX_FILE_NAME_LENGTH];

    FILE* fp;

    printf("%d\n",num_mb);
    fp = fopen("roadinfo.txt","r");
    if(fp==NULL){
        printf("bunk fp\n");
        exit(0);
    }
    fscanf(fp, "%d,%d,%d,%d\n",&num_mb,&num_mt,&num_pb,&num_pt);
    printf("first, mb and pt....\n");
    num_0 = (int)ceil((double) num_pt/10.0);
    num_0_remainder = num_pt%10;
    num_1 = (int)ceil((double) num_mb/10.0);
    num_1_remainder = num_mb%10;
    printf("numroads is: pt-%d, mb-%d\n",num_pt,num_mb);
    printf("numfiles is: pt-%d, mb-%d\n",num_0,num_1);
    printf("because files have max 10 roads each -> expectation for FPGA1 and FPGA4 \
        isn't simple\n");
    // expected outputs are tricky to calculate because each file has 10
    // roads MAX, but the last file is usually not full. Running each FILE
    // against each other file doesn't really run every road against every
    // other, it runs sets of (max) 10 roads against all other sets of 10
    // roads. 
    expected_FPGA4 = num_0 * num_mb;
    for(i=0;i<num_0;i++){
        for(j=0;j<num_1;j++)
        sprintf(pattern_file_name_0,"../pyTDC/dosedpatterns_top/dosedpattern_plus_top_%02d",i);
        sprintf(pattern_file_name_1,"../pyTDC/dosedpatterns_bottom/dosedpattern_minus_bottom_%02d",j);
        printf("%s\n",pattern_file_name_0);
        printf("%s\n",pattern_file_name_1);
        allBoardRoadTest(0,0,4377,4377,4358,pattern_file_name_0,pattern_file_name_1);
    }
    printf("expected fpga4 hits: %d\n",expected_FPGA4);
    printf("had 145 randoms in top, 40 in bottom\n");
}

void dosedFPGA4Test(char half,int nump,int numm){

    int num_mb,num_mt,num_pb,num_pt,num = 0;
    int id0,id1=0;
    int i = 0;
    int dosed_top = 145;
    int dosed_bottom = 40;
    char digit[2];
    char pattern_file_base_p[MAX_FILE_NAME_LENGTH];
    char pattern_file_base_m[MAX_FILE_NAME_LENGTH];
    char pattern_file_name[MAX_FILE_NAME_LENGTH];
    FILE* fp;

    fp = fopen("roadinfo.txt","r");
    if(fp==NULL){
        printf("bunk fp\n");
    }
    fscanf(fp, "%d,%d,%d,%d\n",&num_mb,&num_mt,&num_pb,&num_pt);
    if(half=='T'){
        sprintf(pattern_file_base_p,"../pyTDC/dosedpatterns_top/dosedpattern_plus_top_");
        sprintf(pattern_file_base_m,"../pyTDC/dosedpatterns_top/dosedpattern_minus_top_");
        id0=0;
        id1=2;
        num=num_pt+num_pb;
    }
    else{
        sprintf(pattern_file_base_p,"../pyTDC/dosedpatterns_top/dosedpattern_plus_bottom_");
        sprintf(pattern_file_base_m,"../pyTDC/dosedpatterns_top/dosedpattern_minus_bottom_");
        id0=1;
        id1=3;
        num=num_mt+num_mb;
    }

    // do plus
    for(i=0;i<nump;i++){
        sprintf(digit,"%02d",i);
        strcpy(pattern_file_name,(const char*)pattern_file_base_p);
        strcat(pattern_file_name, digit);
        printf(pattern_file_name);
        L2RoadTest(id0,id1,4,0,4377,4355,pattern_file_name);
    }

    // do minus
    for(i=0;i<numm;i++){
        sprintf(digit,"%02d",i);
        strcpy(pattern_file_name,(const char*)pattern_file_base_m);
        strcat(pattern_file_name, digit);
        L2RoadTest(id0,id1,4,0,4377,4355,pattern_file_name);
    }
    printf("you should see: %d on the scalar for FPGA4\n",num);
    printf("MINUS %d in top or %d in bottom\n",dosed_top,dosed_bottom);
}

void bustL2(unsigned numreps,int id0,int id1){
    unsigned i=0;
    for(i=0;i<numreps;i++){
        L2RoadTest(id0,id1,4,0,4377,4355,"single_road.txt");
    }
    printf("you should see: %d on the scalar for FPGA4\n",numreps*10);    
} 
