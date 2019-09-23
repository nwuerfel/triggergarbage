/******************************************************************************
*
* v1495trashLib.c  -  Driver library for operation of v1495 general purpose vme
* board using a VxWorks 5.4 or later based Single Board computer. 
*
*  Author: Shiuan-Hal, Shiu
*          Randall Evan McClellan
*          trashman
*          
*          May 2019
*
*  Revision  1.0 - Initial Revision
*  Revision  1.1 - Modify some function in 2011
*  Revision  1.2 - Shift User R/W area up to 0x1000 and above (REM - 2013-03-04)
*  Revision  1.2.01 -> helper functions to probe memory on ded board
*  Revision  1.3 - trashlib Noah Wuerfel 5/22/19 remove unnecessary garbage
*/

#include "v1495trashLib.h"

extern unsigned short v1495TDCcount (unsigned id);
extern STATUS v1495Run (unsigned id);
extern unsigned short v1495TimewindowRead (unsigned id);
extern void dummydelay (unsigned id);
extern STATUS v1495ciptag (unsigned id);
extern int g1 (int id, int row);

/* Import external Functions */
IMPORT STATUS sysBusToLocalAdrs (int, char *, char **);
IMPORT STATUS intDisconnect (int);
IMPORT STATUS sysIntEnable (int);
IMPORT STATUS sysIntDisable (int);

// helper for input checking
STATUS checkId(unsigned id){
    // 0 indexed 
    printf("checkID(id = %u)\n",id);
    if(id > MAX_NUM_1495s - 1){
      printf("ERROR: please give a valid board_id from 0 to 8...\n");
        return ERROR;
    }
    if(v1495[id] == NULL){
        printf("ERROR: no board initialized with that id...\n");
        return ERROR;
    }
    return OK;
}

/*******************************************************************************
* v1495Init - Initialize v1495 Library. 
*
* RETURNS: OK, or ERROR if the address is invalid or board is not present.
* trashman- rewritten 5/23/19 to eliminate a bug where boards would be properly
* initialized but function returns error. Also made all variables much more
* obvious and did some needed cleaning.
*
* takes base address of the first board to be initialized (default 0x04000000)
* as well as the increment between each board (by default this is 0x00100000
* such that board 0 is 0x04000000 and board 1 is 0x04100000).
*
* baddr is base address 
********************************************************************************/

STATUS v1495Init (UINT32 first_addr, UINT32 addr_inc, int num_boards) {
    int res, rdata = 0;
    // will hold translated physical addrs to userland and caen specced memories
    unsigned num_boards_initialized = 0;
    unsigned i;
    unsigned int bus_userland_baddr;
    unsigned int bus_caenland_baddr;
    // ptrs to base of arrays of userland readout arrays
    unsigned int bus_mem_ptr;
    unsigned int bus_tdc_ptr;
    unsigned int bus_pulse_ptr;  
    unsigned int bus_delay_ptr;  
    // holds virtual addresses
    unsigned int virt_userland_baddr = first_addr + USERLAND_OFFSET;
    unsigned int virt_caenland_baddr = first_addr + CAENLAND_OFFSET;
    unsigned int virt_mem_ptr = first_addr + USER_MEMREAD_OFFSET;
    unsigned int virt_tdc_ptr = first_addr + USER_TDC_OFFSET;
    unsigned int virt_pulse_ptr = first_addr + USER_PULSEMEM_OFFSET;     
    unsigned int virt_delay_ptr = first_addr + TIMING_ADDRESS_OFFSET;

    printf("v1495Init starting...\n");
    printf("Number of v1495 boards: %d\n", num_boards);
    printf("First board address: 0x%x\n", first_addr);
    printf("Address increment: 0x%x\n", addr_inc);

    // check inputs
    if(first_addr == 0 || first_addr > 0xffffffff){
        printf("ERROR: Init failed... Please specify a Bus (VME-based A24/32) address for the v1495\n");        
        return ERROR;
    }
    if(num_boards > 1 && !addr_inc){
        printf("ERROR: Init failed... Multiple boards but no address increment\n");
        return ERROR;
    }
    if(addr_inc > 0x00100000){
        printf("WARNING: standard address increment is 0x00100000, you chose larger...\n");
    }

    // get bus addresses from virtual offsets
    // 0x39 is A32 non privileged data access this func is specced in sysLib.h
    res = sysBusToLocalAdrs (0x39, (char *) virt_userland_baddr, (char **) &bus_userland_baddr);
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for userland baddr\n");
        return ERROR;
    }

    res = sysBusToLocalAdrs (0x39, (char *) virt_caenland_baddr, (char **) &bus_caenland_baddr);	
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for caenland baddr\n");
        return ERROR;
    }

    res = sysBusToLocalAdrs (0x39, (char *) virt_mem_ptr, (char **) &bus_mem_ptr);
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for mem pointer\n");
        return ERROR;
    }

    res = sysBusToLocalAdrs (0x39, (char *) virt_tdc_ptr, (char **) &bus_tdc_ptr);
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for tdc pointer\n");
        return ERROR;
    }

    res = sysBusToLocalAdrs (0x39, (char *) virt_pulse_ptr, (char **) &bus_pulse_ptr);
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for pulse pattern mem pointer\n");
        return ERROR;
    }
    res = sysBusToLocalAdrs (0x39, (char *) virt_delay_ptr, (char **) &bus_delay_ptr);
    if(res != OK){
        printf("ERROR: Init failed... bad virt to bus conversion for timing delay mem pointer\n");
        return ERROR;
    }


    // setup memory mappings
    for (i = 0; i < num_boards; i++){
        v1495 [i] = (struct v1495_data *) (bus_userland_baddr + i * addr_inc);
        v1495s[i] = (struct v1495_vmestruct *) (bus_caenland_baddr + i * addr_inc);
        v1495m[i] = (struct v1495_memreadout *) (bus_mem_ptr + i * addr_inc);
        v1495t[i] = (struct v1495_tdcreadout *) (bus_tdc_ptr + i * addr_inc);
        v1495p[i] = (struct v1495_pulse *) (bus_pulse_ptr + i * addr_inc);
        v1495d[i] = (struct v1495_delay *) (bus_delay_ptr + i * addr_inc);

        // sanity check read out memories though IDK why we need to check all 
        // seems arbitrary which ones are read anyway
        res = vxMemProbe ((char *) &(v1495[i]->revision), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad revision read at addr=0x%x board_id:\
            %u\n", (UINT32) v1495[i]->revision, i);
            return ERROR;
        }

        res = vxMemProbe ((char *) &(v1495s[i]->scratch16), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad scratch read at addr=0x%x board_id:\
            %d\n", (UINT32) v1495s[i]->scratch16, i);
            return ERROR;
        }

        res = vxMemProbe ((char *) &(v1495m[i]->mem[0]), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad memreadout read at addr=0x%x board_id:\
            %d\n", (UINT32) v1495m[i]->mem[0], i);
            return ERROR;
        }

        res = vxMemProbe ((char *) &(v1495t[i]->tdc[0]), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad tdc read at addr=0x%x board_id:
            %d\n", (UINT32) v1495t[i]->tdc[0], i);
            return ERROR;
        }

        res = vxMemProbe ((char *) &(v1495p[i]->pulse1[0]), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad pulse pattern mem block read at\
            addr=0x%x board_id: %d\n", (UINT32) v1495p[i]->pulse1[0], i);
            return ERROR;
        }

        res = vxMemProbe ((char *) &(v1495d[i]->delay[0]), VX_READ, 2, (char *) &rdata);
        if (res != OK){
            printf ("ERROR: Init failed... bad timing delay mem block read at\
            addr=0x%x board_id: %d\n", (UINT32) v1495d[i]->delay[0], i);
            return ERROR;
        }

        num_boards_initialized++;
        printf ("Initialized v1495 ID %d\n", i);
    }

    printf ("v1495Init: %d v1495(s) successfully initialized\n", num_boards_initialized);
    return (OK);
}

/*******************************************************************************
* v1495StatusUSER - print address and data of user fpga registers
********************************************************************************/
STATUS
v1495StatusUSER (int id)
{
  volatile unsigned short* ptr;
  

  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusUSER: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }


  printf ("\nSTATUS for v1495 id %d from address 0x%x \n", id, (UINT32) v1495[id]);
  printf ("------------------------------------------------ \n\n");
  
  /* Get info from Module register */


  ptr = &(v1495[id]-> a_sta_l  ); printf("a_sta_l   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> a_sta_h  ); printf("a_sta_h   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> b_sta_l  ); printf("b_sta_l   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> b_sta_h  ); printf("b_sta_h   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_sta_l  ); printf("c_sta_l   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_sta_h  ); printf("c_sta_h   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> a_mask_l ); printf("a_mask_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> a_mask_h ); printf("a_mask_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> b_mask_l ); printf("b_mask_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> b_mask_h ); printf("b_mask_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_mask_l ); printf("c_mask_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_mask_h ); printf("c_mask_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> gatewidth); printf("gatewidth at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_ctrl_l ); printf("c_ctrl_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> c_ctrl_h ); printf("c_ctrl_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> mode     ); printf("mode      at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> scratch  ); printf("scratch   at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> g_ctrl   ); printf("g_ctrl    at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> d_ctrl_l ); printf("d_ctrl_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> d_ctrl_h ); printf("d_ctrl_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> d_data_l ); printf("d_data_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> d_data_h ); printf("d_data_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> e_ctrl_l ); printf("e_ctrl_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> e_ctrl_h ); printf("e_ctrl_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> e_data_l ); printf("e_data_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> e_data_h ); printf("e_data_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> f_ctrl_l ); printf("f_ctrl_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> f_ctrl_h ); printf("f_ctrl_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> f_data_l ); printf("f_data_l  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> f_data_h ); printf("f_data_h  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> revision ); printf("revision  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> pdl_ctrl ); printf("pdl_ctrl  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> pdl_data ); printf("pdl_data  at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> d_id     ); printf("d_id      at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> e_id     ); printf("e_id      at %p = 0x%04x \n", ptr, *ptr);
  ptr = &(v1495[id]-> f_id     ); printf("f_id      at %p = 0x%04x \n", ptr, *ptr);

  printf ("\n Print USER data done \n");
  return OK;
}

/*******************************************************************************
* v1495StatusCAEN - print address and data of vme fpga registers
********************************************************************************/
STATUS
v1495StatusCAEN (int id)
{
  volatile unsigned short *ptr;
  volatile unsigned int   *ptr_i;

  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusCAEN: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }

  printf ("\nSTATUS for v1495 id %d from address 0x%x \n", id, (UINT32) v1495s[id]);
  printf ("------------------------------------------------ \n\n");

  /* Get info from Module register */

  ptr   = &(v1495s[id]-> ctrlr    ); printf("ctrlr     at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> statusr  ); printf("statusr   at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> int_lv   ); printf("int_lv    at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> int_ID   ); printf("int_ID    at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> geo_add  ); printf("geo_add   at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> mreset   ); printf("mreset    at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> firmware ); printf("firmware  at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> svmefpga ); printf("svmefpga  at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> vmefpga  ); printf("vmefpga   at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> suserfpga); printf("suserfpga at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> userfpga ); printf("userfpga  at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> fpgaconf ); printf("fpgaconf  at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr   = &(v1495s[id]-> scratch16); printf("scratch16 at %p = 0x%04x \n", ptr  , *ptr  ); 
  ptr_i = &(v1495s[id]-> scratch32); printf("scratch32 at %p = 0x%08x \n", ptr_i, *ptr_i); 

  printf ("\n Print CAEN data done \n");
  return OK;
}

/*******************************************************************************
* v1495StatusMEM - print address and data of mem registers
********************************************************************************/
STATUS
v1495StatusMEM (int id, int ch)
{
  volatile unsigned short *ptr;

  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusMEM: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }
 
  ptr = &(v1495m[id]->mem[ch]); 
  printf("mem %03d at %p = 0x%04x \n", ch, ptr, *ptr  ); 

  return OK;
}

/*******************************************************************************
* v1495StatusTDC - print address and data of tdc registers
********************************************************************************/
STATUS
v1495StatusTDC (int id, int ch)
{
  volatile unsigned short *ptr;

  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusTDC: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }

  ptr = &(v1495t[id]->tdc[ch]); 
  printf("tdc %03d at %p = 0x%04x \n", ch, ptr, *ptr  ); 

  return OK;
}

/*******************************************************************************
* v1495StatusPulser - print address and data of tdc registers
********************************************************************************/
STATUS
v1495StatusPulser (int id, int ipulse, int ch)
{
  volatile unsigned short *ptr=NULL;

  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusPulser: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }
  if( !ipulse )
    {
      printf ("v1495StatusPulser: ERROR : valid usage is v1495StatusPulser(id,ipulse = 1-6)");
      return ERROR;
    }

  if( ipulse==1 ){ ptr = &(v1495p[id]->pulse1[ch]); } 
  if( ipulse==2 ){ ptr = &(v1495p[id]->pulse2[ch]); } 
  if( ipulse==3 ){ ptr = &(v1495p[id]->pulse3[ch]); } 
  if( ipulse==4 ){ ptr = &(v1495p[id]->pulse4[ch]); } 
  if( ipulse==5 ){ ptr = &(v1495p[id]->pulse5[ch]); } 
  if( ipulse==6 ){ ptr = &(v1495p[id]->pulse6[ch]); } 
  printf("pulser%d %03d at %p = 0x%04x \n",ipulse, ch, ptr, *ptr  ); 
  
  return OK;
}

/*******************************************************************************
* v1495StatusDelay - print address and data of timing delay registers
********************************************************************************/
STATUS
v1495StatusDelay (int id, int ch)
{
  volatile unsigned short *ptr;
 
  if ((id < 0) || (v1495[id] == NULL))
    {
      printf ("v1495StatusDelay: ERROR : v1495 id %d not initialized \n", id);
      return ERROR;
    }
  
  ptr = &(v1495d[id]->delay[ch]); 
  printf("delay %02d at %p = 0x%04x \n", ch, ptr, *ptr  ); 
  
  return OK;
}


/*******************************************************************************
* v1495TimewindowSet - Set the internal TDC readout time window of v1495
*
* RETURNS: error if the input setting is not correct
********************************************************************************/
STATUS v1495TimewindowSet (int id, unsigned short val){
    
    STATUS id_check;

    id_check = checkId(id);

    if(id_check == ERROR){
        return ERROR;
    }

    // The last 8bits for val is the delay for the time window which can be larger
    // than 64 but it will make decoder confuse so limit it 
    if ((val & 0xff) > 64){
        printf("ERROR: v1495 time window delay set = %d -> should not be set larger than 0x0040\n",id);
        return ERROR;
    }

    // The 9~8bits for val can set the time window width only can be 
    // 0:256ns wide 1:128ns wide 2:64ns wide 3:32ns wide
    // 256ns is bunk -> check firmware to confirm... nw8/13/19
    if (((val >> 8) & 0xf) > 3){
        printf("ERROR: v1495 time window width = %d -> can not be set larger than 0x0300\n",id); 
        return ERROR;
    }

    // TODO why write twice?
    v1495[id]->scratch = val;
    dummydelay (100);
    v1495[id]->scratch = val;
    return OK;
}

/*******************************************************************************
* v1495TimewindowRead - readout the time window setting value
*
* NOT TRUE IT RETURNS THE VALUE DOESNT READ IT OUT
*
* RETURNS: setting value or error if the board is not initialized
********************************************************************************/
unsigned short v1495TimewindowRead (unsigned id){
    int val = 0;
    STATUS id_check;
    id_check = checkId(id);
    if(id_check == ERROR){
        printf("ERROR: invalid id for v1495TimewindowRead\n"); 
        return ERROR;
    }
    val = v1495[id]->scratch;
    return val;
}


/*******************************************************************************
* v1495Timeset - Set the v1495 internal delay and jittering acceptable range
*
* RETURNS: setting value or error if the board is not initialized
*
* In order to set the internal delay, the step should be 
* 1.write the internal address to address:addr 
* 2.write the value to the address:c_ctrl_l 
********************************************************************************/
STATUS v1495Timeset (int num_channels, unsigned id){

    //STATUS id_check;
    char timing_file_name[MAX_FILE_NAME_LENGTH];
    int timing_values[num_channels];
    int channel_num;
    unsigned short channel_timing_addr;
    unsigned short i = 0;
    FILE *fp = NULL;

    STATUS id_check; 
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }


    sprintf(timing_file_name,"time_%d.txt", id);

    fp = fopen((const char*) timing_file_name, "r");

    if(!fp){
        printf("v1495Timeset: ERROR: failed to open timing file: %s\n", timing_file_name);
        return ERROR;
    }

    for (i = 0; i < num_channels; i++){
        channel_timing_addr = TIMING_ADDRESS_OFFSET + i;
        fscanf(fp, "%d:%x\n", &channel_num, timing_values + i);
        if(channel_num != i){
            printf("ERROR in timing file, mismatched timing info for channel:\
            %d, got: %d", i, channel_num);
	    return ERROR;
        }
        // ensure no timing truncation in converting int to short
        if(timing_values[i] != (unsigned short) timing_values[i]){
            printf("Truncation ERROR, ensure timing file values are 16 bit\n");
            return ERROR;
        }

        // TODO clean this up when I understand firmware
        v1495[id]->c_ctrl_l = channel_timing_addr;	//write the addr to c_ctrl_l 
        dummydelay (10);
        v1495[id]->c_ctrl_h = timing_values[i];	// write the str[i] to c_ctrl_l 
        dummydelay (10);
        v1495[id]->c_ctrl_h = timing_values[i];	// write the str[i] to c_ctrl_l 
        // write twice to make sure it will success 
        // ^^^ christ does this fail silently then?....
        dummydelay (10);

        // TODO understand if a_sta_l should reflect c_ctrl_h after adjusting 
        // the timing
        if (v1495[id]->a_sta_l != timing_values[i]){
            printf ("v1495Timeset: ERROR : v1495 id %d in %d timing not\
            successfully set\n", id, i);
            return ERROR;
        }
    }
    fclose(fp);
    return OK;
}

STATUS v1495TimesetQuick (int num_channels, unsigned id){

  unsigned short channel_timing_addr;
    unsigned short i = 0;
  

    STATUS id_check; 
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }


    for (i = 0; i < num_channels; i++){
        channel_timing_addr = TIMING_ADDRESS_OFFSET + i;
   
        v1495[id]->c_ctrl_l = channel_timing_addr;	//write the addr to c_ctrl_l 
        dummydelay (10);
        v1495[id]->c_ctrl_h = 0x0010;
        dummydelay (10);
        v1495[id]->c_ctrl_h = 0x0010;
        dummydelay (10);

        if (v1495[id]->a_sta_l != 0x0010){
	  printf ("v1495Timeset: ERROR : v1495 id %d in %d timing not\
            successfully set\n", id, i);
	  return ERROR;
        }
    }
    return OK;
}


// Activate Pulser (This disables 'data-mode' pass-through of input channels)
// TODO always use dummy delay OR nanosleep, mixing these without any thought is
// a joke
STATUS v1495ActivatePulser(unsigned id){
    struct timespec req;
    STATUS id_check;
    req.tv_sec = SECONDS_WAIT_TIME;
    req.tv_nsec = NANOS_WAIT_TIME;
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }

    nanosleep (&req, NULL);
    v1495[id]->f_data_l = PULSER_WAKEUP_SIGNAL;
    nanosleep (&req, NULL);
    if(v1495[id]->f_data_l != PULSER_WAKEUP_SIGNAL){
        printf ("ERROR: Pulser failed to activate- no value set, guess I'll\
        die...  -> language! -mj \n");
        return ERROR;
    }
    printf ("v1495ActivatePulser(): pulser activated.\n");
    return OK;
}

//Deactivate Pulser (This enables 'data-mode' pass-through of input channels)
STATUS v1495DeactivatePulser(unsigned id){
    struct timespec req;
    STATUS id_check; 
    req.tv_sec = SECONDS_WAIT_TIME;
    req.tv_nsec = NANOS_WAIT_TIME;
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }
    nanosleep (&req, NULL);
    v1495[id]->f_data_l = PULSER_SLEEP_SIGNAL;
    nanosleep (&req, NULL);
    if(v1495[id]->f_data_l != PULSER_SLEEP_SIGNAL){
        printf ("ERROR: Pulser failed to deactivate- no value set, guess I'll\
                die...  -> language! -mj \n");
        return ERROR;
    }
    printf ("v1495DeactivatePulser(): pulser deactivated.\n");
    return OK;
}



/*******************************************************************************
* v1495PatternSet - Set the data for Level0 pulser (and Level2 pulser control)
*
* RETURNS: STATUS OK OR ERROR
* // TODO needs delays or not?
********************************************************************************/
STATUS v1495PatternSet (unsigned level, int id, const char* filename){
    unsigned short str[PULSE_PATTERN_COLUMNS] =
        {0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};
    unsigned short read[PULSE_PATTERN_COLUMNS] =
        {0x0000,0x0000,0x0000,0x0000,0x0000,0x0000};
    unsigned i,j = 0;
    char pattern_file_name[MAX_FILE_NAME_LENGTH];
    STATUS id_check; 
    FILE* fp = NULL;

    // check inputs
    if(level > 2){
        printf("v1495PatternSet: ERROR invalid level: %d, try a value between 0 and 2...\n",level);
        return ERROR;
    }

    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }

    if(level < 2){
        if(*filename == '\0'){
            printf("v1495PatternSet: using default- simple_pattern.txt\n");
            sprintf(pattern_file_name,"simple_pattern.txt");
        }
        else{
            sprintf(pattern_file_name, filename);
        }
    }
    else{
        sprintf(pattern_file_name, "pattern_2.txt");
    }
  
    fp = fopen((const char*) pattern_file_name, "r");
    
    if(fp == NULL){
        printf("v1495PatternSet: ERROR: failed to open pattern file: %s", pattern_file_name);
        return ERROR;
    }

    // level 2 is special 
    if (level == 2){
        for (i = 0; i < PATTERN_2_LINES; i++){
            fscanf(fp, "%hx\n", &str[0]);
            v1495p[id]->pulse1[i] = str[0];
            read[0] = v1495p[id]->pulse1[i];
            if(str[0]!=read[0]){
                printf ("v1495PatternSet: Readback Error: pattern 2 board_ID %d wrote: 0x%x read:\
                    0x%x at line %d\n",id,str[0],read[0],i);
                return ERROR;
            }
        }
    }

    // TODO do I need the delays between writes and reads or not? DUMMY DELAY or
    // nanosleeps?
    else{
        for (i = 0; i < PULSE_PATTERN_LINES; i++){
            fscanf(fp, "%hx,%hx,%hx,%hx,%hx,%hx\n", &str[0], &str[1],
            &str[2], &str[3], &str[4], &str[5]);	            
            v1495p[id]->pulse1[i] = str[0];
            v1495p[id]->pulse2[i] = str[1];
            v1495p[id]->pulse3[i] = str[2];
            v1495p[id]->pulse4[i] = str[3];
            v1495p[id]->pulse5[i] = str[4];
            v1495p[id]->pulse6[i] = str[5];
            read[0] = v1495p[id]->pulse1[i];
            read[1] = v1495p[id]->pulse2[i];
            read[2] = v1495p[id]->pulse3[i];
            read[3] = v1495p[id]->pulse4[i];
            read[4] = v1495p[id]->pulse5[i];
            read[5] = v1495p[id]->pulse6[i];
            for(j = 0; j < PULSE_PATTERN_COLUMNS; j++){
                if(str[j]!=read[j]){
                    printf ("v1495PatternSet: Readback Error: board_ID %d wrote 0x%x read: 0x%x\
                        word #%d of %d\n",id,str[j],read[j],j,
                        PULSE_PATTERN_COLUMNS);
                    return ERROR;
                }
            }
        }
    }
    fclose (fp);
    return OK;
}

// TODO rev checking? ensure this is dummy or lev 1?
// STATUS how to check
void v1495PulserGo(unsigned id){
    int rev = 0x0000;
    rev = v1495[id]->revision;
    v1495[id]->f_data_h = PULSE_ON_SIGNAL;
    dummydelay(1000);
    v1495[id]->f_data_h = PULSE_RESET_SIGNAL;
    printf ("v1495PulserGo: Pulser Start signal sent out from board %x.\n", rev);
}

// assumes 3 board setup with id=level
void L0ContPulse(unsigned id){
    printf("starting continuous pulse mode\n");
    printf("Writing 0x0003 to f_data_l \n");
    v1495[id]->f_data_l = 0x0003;
    printf("hunt me on the oscilliscope... NEWB\n");
}


void L2ContPulse(unsigned id){
    printf("starting continuous pulse mode\n");
    v1495[id]->f_data_h = 0x0002;
    printf("hunt me on the oscilliscope... NEWB\n");
}

void L2SinglePulse(unsigned id){
    printf("writing 0x8000 to f_data_h, then 0x0000\n");
    v1495[id]->f_data_h = 0x8000;
    printf("I'm a delay print!\n");
    v1495[id]->f_data_h = 0x0000;
}

void L2KludgeSinglePulse(unsigned id){
    printf("writing 0x0002 to f_data_h\n");
    v1495[id]->f_data_h = 0x0002;
    printf("tryna shutit down immediately");
    v1495[id]->f_data_h = 0x0000;
}

// Activates the signal sent out from Level 1, to G1 on Levels 0 and 1, to stop
// the TDCs -- REM -- 2013-07-18
STATUS v1495tdcStopSignal(unsigned id){
    STATUS id_check;

    id_check = checkId(id);
    if(id_check == ERROR){
        printf("Bad id for tdc stop, guess I'll die...\n");
    }

    if(v1495[id]->revision != LEVEL_1_REVISION){
        printf("Attempted to send tdc stop from non-level1 board, try\
        again...\n");
        return ERROR;
    }

    v1495[id]->f_data_h = TDC_STOP_SIGNAL;
    if(v1495[id]->f_data_h != TDC_STOP_SIGNAL){
        printf("Failed to read back TDC_STOP_SIG, guess I'll die...\n");
        return ERROR;
    }
    printf ("TDC stop signal sent out from board %d.\n", id);
    return OK;
}

// TODO is it necessary to dummy delay or nanosleep?
STATUS v1495Run (unsigned id) {
    STATUS id_check;
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }
    v1495ciptag (id);
    dummydelay (10);
    v1495[id]->c_ctrl_l = RUN_SIGNAL_START;
    dummydelay (10);
    v1495[id]->c_ctrl_l = RUN_SIGNAL_END;
    return OK;
}

// TODO is it necessary to dummy delay or nanosleep?
STATUS v1495RunForeverMaybe (unsigned id) {
    STATUS id_check;
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }
    v1495ciptag (id);
    dummydelay (10);
    v1495[id]->c_ctrl_l = RUN_SIGNAL_START;
    return OK;
}


// TODO determine if this is useful, how much delay it needs, how much  time it
// takes etc....
void dummydelay (unsigned delay) {
    int j = 0;
    int k = 0;
    for (j = 0; j < delay; j++){
        k = k + 1;
    }
    k = 0;
    return;
}

// fpgaconf is some CAENland thing...
// CAENLAND specs on p22 that writing here generates a configuration reload
// whatever the hell that means
void v1495Reload (unsigned id){
    STATUS id_check;
    id_check = checkId(id);
    if(id_check == ERROR){
        return;
    }
    v1495s[id]->fpgaconf = RELOAD_SIGNAL;
}

// TODO check read write if delay is needed
STATUS v1495ciptag(unsigned id){
    STATUS id_check;
    id_check = checkId(id);
    if(id_check == ERROR){
        return ERROR;
    }
    v1495t[id]->tdc[TDC_BUFFER_DEPTH-1] = TDC_CIP_TAG;
    return OK;
}

// quick function to dump sizeof types becuase I'm not used to the types.h used
// in vxWorks headers
void dumpSize(){
    printf("size of int is: %d\n", sizeof(int));
    printf("size of short int is: %d\n", sizeof(short int));
    printf("size of unsigned int is: %d\n", sizeof(unsigned int));
    printf("size of short is: %d\n", sizeof(short));
    printf("size of long is: %d\n", sizeof(long));
}

// This functional is called during pulser test, but not defined anywhere, so I imported it from evan's, but I don't know how it works -mj 
unsigned short v1495TDCcount (unsigned id){
    unsigned short tmp;
    unsigned short tmp0;
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 100000;

    tmp0 = v1495t[id]->tdc[TDC_BUFFER_DEPTH-1];

    if ((tmp0 >> 8) == 0x81){
        tmp = tmp0 & 0xff;
    }
    else{
        tmp = 0xd1ad;
    }
    return (tmp);
}
