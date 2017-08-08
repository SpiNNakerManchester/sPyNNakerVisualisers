// SpiNNaker Real-Time Visualisation Software (VisRT)
// Cameron Patterson
//
// compile with:  g++ visualiser.cpp -o visualiser -lGLU -lGL -lglut -lpthread -lconfig
//
// NOTE: you will need to have GLUT / freeglut / libconfig-dev libraries installed to use this software.
//
//
// usage:
//   visualiser [OPTIONS...]
//     [-c configfile]
//          // visualisation settings, if omitted looks for visparam.ini and if not found is a 48-chip heatmap
//     [-replay savedspinnfile [replaymultiplier]]
//          // instead of live data you can replay a previous saved .spinn file.  Speed of playback may also
//          //   be chosen.  e.g. 0.25 = quarter original speed, 1 = original speed, 10 = ten times faster
//     [-l2g localtoglobalmapfile]
//     [-g2l globaltolocalmapfile]
//       // these options are used together to map split neural populations to aggregated ones (from PACMAN)
//     [-ip source machine]
//          //  specify IP address of machine you want to listen to (if omitted first packet received is source dynamically)
//
// --------------------------------------------------------------------------------------------------
//
// Full Versioning information found at the tail of the file.
//
// Current Version:
// ----------------
// 4th Sep 2013-     CP incorporated visualiser for the Cochlea from Qian Lui
// 2nd Sep 2013-     CP rewrote the cpu temperature routine to calculate locally, tabs replaced by 3 spaces, added INITZERO option for data starting at zero rather than undefined.
// 29th Aug 2013-    CP immediate_data became a float (for better decay)
// 28th Aug 2013-    CP made decay proportion independent of visualisation type, comment out debug printf, blackbackground only applied on tiled displays
// 19th Aug 2013-    CP added BLACKBACKGROUND option (defaults to not used)
//                     also worked around libconfig issue on Ubuntu12.04 using incorrect long type rather than int
// 10th May 2013-    CP corrected bug with replay caused by the below, and removed lots of unused comments
// 4th-7th May 2013- CP added command line options for selective board IP/hostname
// 22nd April 2013 - CP added command line options [-c,-replay,-l2g,-g2l] described above
// 19th April 2013 - FG added the retina2 visualisation type
// 17th April 2013 - freeing of all mallocs now correct, undefined data and ranges now parameterised
// 16th April 2013 - fix x & y coordinate label overlapping problem in tiled mode
// 15-16 April 2013- now uses the visparam.ini file in local directory to specify setup
//                     no longer requiring a recompile (if no file defaults to 48-chip heatdemo)
//
// -----------------------------------------------------------------------------------------------------
//  Select your simulation via a specific or visparam.ini file in the local directory
// -----------------------------------------------------------------------------------------------------
//
// general, Ethernet and threading includes:
#include <iostream>
#include <GL/glut.h>
#include <GL/freeglut.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>  // included for Fedora 17 Fedora17  28th September 2012 - CP
#include <libconfig.h> // included 14/04/13 for file based parameter parsing, (needs libconfig-dev(el))
using namespace std;

// --------------------------------------------------------------------------------------------------
// select your visualisation via a specific configuration file (-c filename) or the visparam.ini file
// --------------------------------------------------------------------------------------------------


// MACRO for testing if data is undefined or not
#define THISISVALIDDATA(value)      (value>(NOTDEFINEDFLOAT+1))

// --------------------------------------------------------------------------------------------------
// This section checks to see whether the compilation is on a 32- or a 64-bit architecture.

#if defined(__i386__)
#define MACHINEBITS 32
#elif defined(__x86_64__)
#define MACHINEBITS 64
#else
#define MACHINEBITS 0
#endif


// --------------------------------------------------------------------------------------------------
// These defines used to represent max/min and invalid data values for checking out of range etc.
//    as data is typically all floating point, these INT versions are rarely (if ever) used.

#define MAXDATAINT 65535
#define MAXDATAFLOAT 65535.0

#define MINDATAINT -65535
#define MINDATAFLOAT -65535.0

#define NOTDEFINEDINT -66666
#define NOTDEFINEDFLOAT -66666.0


// --------------------------------------------------------------------------------------------------
// Initialise global variables, and to set some sensible defaults (can all be overriden in visparam.ini)
// USERS SHOULD NOT EDIT THIS SECTION - use visparam.ini please!


int SIMULATION=1;                                                   // defaults to HEATDEMO (1)
// params below set this to a 48-chip HEATDEMO

char TITLE[50];                                                     // title used at the top of the screen

int STARTMODE=1, STARTCOLOUR=1;                                     // start with requested mode (default tiled, multicolour)
int displaymode = STARTMODE, colourused = STARTCOLOUR;
int BLACKBACKGROUND=0;
int INITZERO=0;                                                     
int XDIMENSIONS=32,YDIMENSIONS=32,EACHCHIPX=4,EACHCHIPY=4;          // canvas size defaults
int XCHIPS=(XDIMENSIONS/EACHCHIPX),YCHIPS=(XDIMENSIONS/EACHCHIPX);  // defaults to a chipwise display

double TIMEWINDOW = 3.5;                                            // default time width of a window
float displayWindow = TIMEWINDOW;
int HISTORYSIZE=3500,MAXRASTERISEDNEURONS=1024;                     // data set sizes

int WINBORDER = 110, WINHEIGHT = 700, WINWIDTH = 850;               // defaults for window sizing
int DISPLAYKEY = 1, KEYWIDTH = 50;
int DISPLAYXLABELS = 1, DISPLAYYLABELS = 1;                         // gives options to suspend printing of x and y axis labels if required
int keyWidth=KEYWIDTH;
int windowBorder=WINBORDER, windowHeight=WINHEIGHT, windowWidth=WINWIDTH+keyWidth; // startup for window sizing

double HIWATER=10, LOWATER=0;                                       // default hi and lo water
double lowwatermark=HIWATER,highwatermark=LOWATER;
int DYNAMICSCALE=1;                                                 // default to permit dynamic scaling
int PERCENTAGESCALE=0;                                              // set this to 1 if you don't care about values, only relative %ages

int LABELBYCHIP=0;                                                  // just print one label per chip and not sub-chip/core
int PLAYPAUSEXIT=1, DISPLAYMINIPLOT=1, INTERACTION=1;               // options for screen output

int MAXFRAMERATE=25, PLOTONLYONDEMAND=0;                            // graphics frame updates

int XFLIP=0,YFLIP=0,VECTORFLIP=0,ROTATEFLIP=0;                      //   default to no translations
int xflip=XFLIP,yflip=YFLIP,vectorflip=VECTORFLIP,rotateflip=ROTATEFLIP;  // of the data

int SDPPORT=17894;                                                  // which UDP port are we expecting our SDP traffic on

int FIXEDPOINT=16;                                                  // number of bits in word of data that are to the right of the decimal place

long int BITSOFPOPID=0;              // number of bits of population in each core (pow of 2); 0 for implicit core==popID

double ALTERSTEPSIZE=1.0;      // the step size used when altering the data to send
double DECAYPROPORTION=0.0;     // how quickly does the raster plot diminish



//float* immediate_data;           // this stores the value of each plotted point data (time == now)  - superfluous
float** history_data;       // this stores the historic value the plotted points (double the initial width should be sufficient)
float** history_data_set2;    // 2nd set of data for ancillary raster plot window
float* immediate_data;        // this creates a buffer tally for the Ethernet packets (1 ID = one plotted point)

int** maplocaltoglobal; // always 2 wide.  Size of mapping from X&Y coords #of pops
int** mapglobaltolocal; // and the reverse from the 2nd file




// which visualiser option
#define HEATMAP     1
#define RATEPLOT     2
#define RETINA        3
#define INTEGRATORFG    4
#define RATEPLOTLEGACY  5
#define MAR12RASTER     6
#define SEVILLERETINA    7
#define LINKCHECK    8
#define SPIKERVC    9
#define CHIPTEMP    10
#define CPUUTIL        11
#define RETINA2        12
#define COCHLEA     13

// different colour maps available
#define MULTI         1
#define GREYS         2
#define REDS         3
#define GREENS          4
#define BLUES           5
#define THERMAL        6
#define RED        7
#define BLUE        8

// view mode
#define TILED         1
#define INTERPOLATED     2
#define HISTOGRAM     3
#define LINES        4
#define RASTER        5
#define EEGSTYLE    6


// GLOBAL VARIABLES, my bad.
char plotvaluesinblocks=0;    // set non-zero if you just want the coloured blocks with values, (tiles/histogram only)
char somethingtoplot=0;        // determines when we should update the screen (no point in plotting no change eh?)
char freezedisplay=0;        // whether we should pause the display updates (and send a pause packet to the sim)
int64_t freezetime;        // when pausing the simulation we hold time at the time of pausing (for screen display purposes)
int boxsize=40, gap=5;        // used for button creation and gaps between these boxes and the edge of the screen
int win1 = 0;            // the main graphics window used by OpenGL
int win2 = 0;            // a test for windows spawned from the main window
int windowToUpdate;        // used to know which window to update
int xdim;// = XDIMENSIONS;        // number of items to plot in the x dimension
int ydim;// = YDIMENSIONS;        // number of items to plot in the y dimension
int plotWidth,printlabels;

int fullscreen=0;        // toggles to get rid of menus/labels/axes/key/controls etc.
int oldwindowBorder=0;        // used as border disappears when going full-screen
int gridlines=0;        // toggles gridlines, starts off

int RHMouseMenu=0;        // used for menu generation/regeneration.
int modesubmenu=0;        // for mode submenu
int coloursubmenu=0;        // for colours submenu
int transformsubmenu=0;        // for plot transformation submenu
int filesubmenu=0;        // for save file submenu
char needtorebuildmenu=0;    // if a menu is open we can't reconfigure it. So we queue the request.
char menuopen=0;        // a callback populates this as 1 or 0 depened on whether a menu is open or not.
char editmode=1,livebox=-1;    // for user feedback - box selection and whether edit mode is toggled on/off.

// GLOBAL VARIABLES, per visualiser option

// SIMULATION == HEATMAP
float alternorth=40.0,altereast=10.0,altersouth=10.0,alterwest=40.0;    // default starting temperatures, and in-flight editing values for the 4 edges
int controlboxes = 3;                            // grid of control boxes to build (3x3)
char NORTH=5, EAST=1, SOUTH=3, WEST=7, CENTRE=4;            // which box in the grid is used for each edge + middle, Ordered:  BtmR->TopR .. BtmL->TopR
int yorigin=gap;                            // Base coordinate of where to plot the compass control box
int xorigin;                                                            // for the control box
//int xorigin=(windowWidth+keyWidth)-(controlboxes*(boxsize+gap));    // for the control box
// CP made dynamic


// SIMULATION == RATEPLOT OR RATEPLOTLEGACY
float biasediting;                // keeps the transient biascurrent as it's edited so it can be sent
int rasterpopulation=-1;               // keeps the population ID that we're running rasters on (-1 if not in use)
float* biascurrent;                           // array used to know what we're starting from when adjusting
//float biascurrent[XDIMENSIONS*YDIMENSIONS];    // used to know what we're starting from when adjusting
//CP made dynamic


// SIMULATION == RETINA2
int N_PER_PROC;
int ID_OFFSET;

// FILE OPERATIONS
float playbackmultiplier=1.0;        // when using a recorded input file, 1=realtime, 0.25=quarter speed, 15=15x speed (specified by optional CLI argument)
FILE *fileinput = NULL;            // if the user chooses to provide data as input this is the handle

char outputfileformat=0;        // 3 states. 0 = no writing, 1=.spinn UDP payload format, 2 = neurotools format
char writingtofile=0;            // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy.
FILE *fileoutput = NULL;


int mappingfilesread=0,maplocaltoglobalsize,mapglobaltolocalsize;  // logs how bug each array actually gets (might not be full!)


int lasthistorylineupdated=0;                        // this is stored so that rows that have not been updated between then and now can be cleared out

int counter=0;                                // number of times the display loop has been entered
int pktcount=0;                                // total aggregate of packets received and processed
int64_t printpktgone=0;                            // if set non zero, this is the time the last Eth packet message was sent, idle function checks for 1s before stopping displaying it
struct timeval startimeus;                        // for retrieval of the time in us at the start of the simulation
int64_t starttimez,firstreceivetimez=0;                    // storage of persistent times in us
int64_t keepalivetime;                            // used by code to send out a packet every few seconds to keep ARP entries alive
unsigned int minneuridrx=10000000;                    // we only need to raster plot the number of neurons firing in a raster plot, (smallest neurid received).
unsigned int maxneuridrx=0;                        // we only need to raster plot the number of neurons firing in a raster plot, (largest neurid received).

int safelyshutcalls=0;                                                  // sometimes the routine to close (and free memory) is called > once, this protects

#ifdef TESTING
int leftbuttondown = 0;
int rectanglestartx = -1;    // if too small (i.e. less than 5x5 pixels), reset
int rectanglestarty = -1;
int rectangleendx = -1;
int rectangleendy = -1;
int tempcountcp = 0;        // recording number of spikes per screenful!
#endif



//network parameters for the SDP and SpiNNaker protocols

#define MAXBLOCKSIZE        364             // maximum possible Ethernet payload words for a packet- (SpiNN:1500-20-8-18) (SDP:1500-20-8-26)
#define SPINN_HELLO        0x41            // SpiNNaker raw format uses this as a discovery protocol
#define P2P_SPINN_PACKET     0x3A               // P2P SpiNNaker output packets (Stimulus from SpiNNaker to outside world)
#define STIM_IN_SPINN_PACKET     0x49               // P2P SpiNNaker input packets (Stimulus from outside world)
#pragma pack(1)                     // stop alignment in structure: word alignment would be nasty here, byte alignment reqd

struct spinnpacket {
   unsigned short version;
   unsigned int cmd_rc;
   unsigned int arg1;
   unsigned int arg2;
   unsigned int arg3;
   unsigned int data[MAXBLOCKSIZE];
};     // a structure that holds SpiNNaker packet data (inside UDP segment)

struct sdp_msg        // SDP message (<=292 bytes)
{
   unsigned char ip_time_out;
   unsigned char pad;
   // sdp_hdr_t
   unsigned char flags;        // SDP flag byte
   unsigned char tag;            // SDP IPtag
   unsigned char dest_port;        // SDP destination port
   unsigned char srce_port;        // SDP source port
   unsigned short dest_addr;        // SDP destination address
   unsigned short srce_addr;        // SDP source address
   // cmd_hdr_t (optional, but tends to be there!)
   unsigned short cmd_rc;        // Command/Return Code
   unsigned short seq;            // seq (new per ST email 27th Oct 2011)
   unsigned int arg1;            // Arg 1
   unsigned int arg2;            // Arg 2
   unsigned int arg3;            // Arg 3
   // user data (optional)
   unsigned int data[MAXBLOCKSIZE];    // User data (256 bytes)
};


typedef struct
{
   int64_t filesimtimeoffset;
   short incoming_packet_size;
   unsigned char payload[];
} spinnaker_saved_file_t;



//global variables for SDP packet receiver
int sockfd_input, sockfd;
char portno_input[6];
struct addrinfo hints_input, hints_output, *servinfo_input, *p_input, *servinfo, *p;
struct sockaddr_storage their_addr_input;
int rv_input;
int numbytes_input;
struct sdp_msg * scanptr;
struct spinnpacket * scanptrspinn;
in_addr spinnakerboardip;
int spinnakerboardport=0;
char spinnakerboardipset=0;
unsigned char buffer_input[1515];  //buffer for network packets (waaaaaaaaaaay too big, but not a problem here)
//end of variables for sdp spinnaker packet receiver - some could be local really - but with pthread they may need to be more visible


// prototypes for functions below
void error(char *msg);
//void init_udp_server_spinnaker();
void init_sdp_listening();
//void* input_thread (void *ptr);
void* input_thread_SDP (void *ptr);
void init_sdp_sender();
// void sdp_sender(unsigned short dest_add, unsigned int command, unsigned int arg1, unsigned int arg2, unsigned int arg3, float north, float east, float south, float west);
void sdp_sender(unsigned short dest_add, unsigned char dest_port, unsigned int command, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned char extrawords, ...);
void create_new_window();
void destroy_new_window();
void display_win2();
void filemenu (void);
void transformmenu (void);
void modemenu (void);
void colmenu (void);
void mymenu (int value);
void rebuildmenu (void);
void safelyshut(void);
void open_or_close_output_file(void);
int paramload(void);
// end of prototypes


// setup socket for SDP frame receiving on port SDPPORT defined about (usually 17894)
void init_sdp_listening()
{
   snprintf (portno_input, 6, "%d", SDPPORT);

   bzero(&hints_input, sizeof(hints_input));
   hints_input.ai_family = AF_INET; // set to AF_INET to force IPv4
   hints_input.ai_socktype = SOCK_DGRAM; // type UDP (socket datagram)
   hints_input.ai_flags = AI_PASSIVE; // use my IP

   if ((rv_input = getaddrinfo(NULL, portno_input, &hints_input, &servinfo_input)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv_input));
      exit(1);
   }

   // loop through all the results and bind to the first we can
   for(p_input = servinfo_input; p_input != NULL; p_input = p_input->ai_next) {
      if ((sockfd_input = socket(p_input->ai_family, p_input->ai_socktype, p_input->ai_protocol)) == -1) {
         printf("SDP SpiNNaker listener: socket");
         perror("SDP SpiNNaker listener: socket");
         continue;
      }

      if (bind(sockfd_input, p_input->ai_addr, p_input->ai_addrlen) == -1) {
         close(sockfd_input);
         printf("SDP SpiNNaker listener: bind");
         perror("SDP SpiNNaker listener: bind");
         continue;
      }

      break;
   }

   if (p_input == NULL) {
      fprintf(stderr, "SDP listener: failed to bind socket\n");
      printf("SDP listener: failed to bind socket\n");
      exit(-1);
   }

   freeaddrinfo(servinfo_input);

   //printf ("SDP UDP listener setup complete!\n");      // here ends the UDP listener setup witchcraft
}

void *get_in_addr(struct sockaddr *sa)
{
   if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* input_thread_SDP (void *ptr)
{
   struct sockaddr_in si_other; // for incoming frames
   socklen_t addr_len_input =sizeof (struct sockaddr_in);
   int64_t sincefirstpacket,nowtime;
   char sdp_header_len=26;
   unsigned char xsrc,ysrc;
   unsigned int i, xcoord, ycoord;
   struct timeval stopwatchus;

   //printf("Listening for SDP frames.");

   while (1) {                             // for ever ever, ever ever.
      int numAdditionalBytes = 0;

      if ((numbytes_input = recvfrom(sockfd_input, buffer_input, sizeof buffer_input , 0,
                                     (sockaddr*)&si_other, (socklen_t*)&addr_len_input)) == -1) {

         printf("Error line 441: : %s\n",strerror(errno));
         perror((char*)"error recvfrom");
         exit(-1);                    // will only get here if there's an error getting the input frame off the Ethernet
      }

      scanptr = (sdp_msg*) buffer_input;              // pointer to our packet in the buffer from the Ethernet
      scanptrspinn = (spinnpacket*) buffer_input;          // pointer to our packet in the buffer from the Ethernet
      numAdditionalBytes = numbytes_input-sdp_header_len;    // used for SDP only

      if(scanptrspinn->cmd_rc!=htonl(SPINN_HELLO)) {    // we process only spinnaker packets that are non-hellos

         if (spinnakerboardipset==0) {                // if no ip: set ip,port && init
            // if we don't already know the SpiNNaker board IP then we learn that this is our board to listen to
            spinnakerboardip=si_other.sin_addr;
            spinnakerboardport=htons(si_other.sin_port);
            spinnakerboardipset++;
            init_sdp_sender();
            printf("Pkt Received from %s on port: %d\n", inet_ntoa(si_other.sin_addr),htons(si_other.sin_port));
         }        // record the IP address of our SpiNNaker board.

         if (spinnakerboardport==0) {                // if no port: set port && init
            // if we don't already know the SpiNNaker port, then we get this dynamically from an incoming message.
            spinnakerboardport=htons(si_other.sin_port);
            init_sdp_sender();
            printf("Pkt Received from %s on port: %d\n", inet_ntoa(si_other.sin_addr),htons(si_other.sin_port));
         }        // record the port number we are being spoken to upon, and open the SDP connection externally.

         // ip && port are now set, so process this SpiNNaker packet


         /*
                 //For debugging if you like that sort of thing... (usually commented)
                 if (scanptrspinn->cmd_rc!=htonl(SPINN_HELLO)) {
                   printf ("received SDP packet of %d bytes\n", numbytes_input);
                   printf ("Header of %d Bytes, Data of %d Bytes\n",sdp_header_len,numAdditionalBytes);
                   printf("ip_time_out: %d. Padded by: %d\n",scanptr->ip_time_out,scanptr->pad);
                   printf("flags: %d. Tagged by: %d\n",scanptr->flags,scanptr->tag);
                   printf("dest_port: %d. srce_port: %d\n",scanptr->dest_port,scanptr->srce_port);
                   printf("dest_addr: %d. srce_addr: %d\n",scanptr->dest_addr,scanptr->srce_addr);
                   printf("cmd_rc: %d.\n",scanptr->cmd_rc);
                   printf("arg1: %d.\n",scanptr->arg1);
                   printf("arg2: %d.\n",scanptr->arg2);
                   printf("arg3: %d.\n",scanptr->arg3);
                   printf("data[0]: %x.\n",scanptr->data[0]);
                   printf("data[1]: %x.\n",scanptr->data[1]);
                   printf("\n");
                 }
         */

         gettimeofday(&stopwatchus,NULL);                // grab current time
         nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us
         if (firstreceivetimez==0) firstreceivetimez=nowtime;        // if 1st packet then note it's arrival
         sincefirstpacket = (nowtime-firstreceivetimez)/1000;        // how long in ms since visualisation got 1st valid packet.

         float timeperindex = displayWindow / (float) plotWidth;    // time in seconds per history index in use (or pixel displayed)
         //printf("Here, with timeperindex:%f, and y_scaling_factor:%f. Display Window = %f fps.\n",timeperindex,y_scaling_factor,displayWindow);
         int updateline=((nowtime-starttimez)/(int64_t)(timeperindex*1000000)) % (HISTORYSIZE);    // which index is being updated (on the right hand side)

         if (updateline<0 || updateline>HISTORYSIZE) {
            printf("Error line 500: Updateline out of bounds: %d. Time per Index: %f. \n  Times - Now:%lld  Start:%lld \n",updateline, timeperindex, (long long int)nowtime, (long long int)starttimez); // CPDEBUG
         } else {
            if (freezedisplay==0) {
               int linestoclear = updateline-lasthistorylineupdated;            // work out how many lines have gone past without activity.
               // when window is reduced updateline reduces. ths causes an underflow construed as a wraparound. TODO.
               if (linestoclear<0 && (updateline+500)>lasthistorylineupdated) linestoclear=0;        // to cover any underflow when resizing plotting window smaller (wrapping difference will be <500)
               if (linestoclear<0) linestoclear = (updateline+HISTORYSIZE)-lasthistorylineupdated;     // if has wrapped then work out the true value
               int numberofdatapoints=xdim*ydim;
               for (int i=0; i<linestoclear; i++) {
                  //printf("%d - %d =  %d (with WindowWidth: %d)\n",updateline, lasthistorylineupdated, linestoclear, windowWidth);
                  for (int j=0; j<numberofdatapoints; j++) history_data[(1+i+lasthistorylineupdated)%(HISTORYSIZE)][j]= INITZERO?0.0:NOTDEFINEDFLOAT;  // nullify data in the quiet period
                  if (win2) {
                     numberofdatapoints=MAXRASTERISEDNEURONS;                // bespoke for Discovery demo
                     for (int j=0; j<numberofdatapoints; j++) history_data_set2[(1+i+lasthistorylineupdated)%(HISTORYSIZE)][j]=INITZERO?0.0:NOTDEFINEDFLOAT;  // nullify data in the quiet period
                  }
               }
               // printf("%d - %d =  %d (with WindowWidth: %d)\n",updateline, lasthistorylineupdated, linestoclear, windowWidth);
               lasthistorylineupdated=updateline;
            }
         }

         if (SIMULATION==RETINA) {
            if (freezedisplay==0 && htonl(scanptrspinn->cmd_rc)==STIM_IN_SPINN_PACKET) {    // if we are not paused & we got the proper command
               for (int i=0; i<(numbytes_input-18)/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
                  uint spikerID=scanptrspinn->data[i]&0xFF;    // Get the firing neuron ID (mask off last 8 bits for neuronID ignoring chip/coreID)
                  immediate_data[spikerID]+=1;            // Set the bit to say it's arrived
                  if (spikerID<minneuridrx) minneuridrx=spikerID;
                  if (spikerID>maxneuridrx) maxneuridrx=spikerID;
                  history_data[updateline][spikerID]=immediate_data[spikerID];  // add to count in this interval
                  if (outputfileformat==2) {                // write to output file only if required and in NeuroTools format (2)
                     if (writingtofile==0) {
                        writingtofile=1;        // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
                        fprintf(fileoutput,"%lld.0\t%d.0\n",(long long int)sincefirstpacket,spikerID);      // neurotools format (ms and NeurID)
                        writingtofile=0;        // note write finished
                     }
                  }
                  //printf("Data Processed: %d, Data Raw:%d.\n",spikerID, immediate_data[spikerID]);
               }        //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
               somethingtoplot=1;                    // indicate we should refresh the screen as we likely have new data
            }
         }


         if (SIMULATION==SEVILLERETINA) {
            uint commandcode=scanptr->cmd_rc;
            uint columnnum=scanptr->arg1;
            uint numofrows=scanptr->arg2;
            uint numofcols=scanptr->arg3;

            if (freezedisplay==0 && commandcode==0x4943) {            // if we are not paused, going to I.C. the seville retina
               for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of signed shorts)
                  short datain1=(scanptr->data[i])&0xFFFF;    // 1st of the pair
                  short datain2=(scanptr->data[i]>>16)&0xFFFF;    // 2nd of the pair
                  uint pixelid=columnnum*numofrows+(i*2);        // 1st pixel ID
                  //printf("(%d,%d) %d   and  (%d,%d) %d.\n",columnnum,i*2,datain1,columnnum,(i*2)+1,datain2);
                  //printf("[%d] --  %d) %d,  %d) %d  from 0x%x.\n",pixelid,i*2,datain1,(i*2)+1,datain2,scanptr->data[i]);
                  immediate_data[pixelid]=datain1;        // store 1st pixel ID
                  history_data[updateline][pixelid]=immediate_data[pixelid];            // replace any data here already
                  immediate_data[pixelid+1]=datain2;        // store 2nd pixel ID
                  history_data[updateline][pixelid+1]=immediate_data[pixelid+1];            // replace any data here already
               }
            }
         }

		if (SIMULATION==RETINA2) 	{ // FG for Seville Retina 19th Apr 2013

	          if (freezedisplay==0) {  // so long as the display is still active then listen to new input          
//                printf("packet received length: %d\n", numAdditionalBytes/4);
                for (uint e =0; e < numAdditionalBytes/4; e++)
                {
                    uint bottom_rkey = scanptr->data[e] & 0xFFFF;
//                    printf("0x%x, %d-%d-%d-%d\n",  bottom_rkey,
//                                    (bottom_rkey >> 24) & 0xFF,
//                                    (bottom_rkey >> 16) & 0xFF,
//                                    (bottom_rkey >> 11) & 0x1F,
//                                    bottom_rkey & 0x7FF);
//                    printf("0x%x\n", bottom_rkey);
                    uint neuron_id = ( (bottom_rkey+ID_OFFSET)%0x800+
                                        ((bottom_rkey+ID_OFFSET)/0x800*N_PER_PROC)) & (XDIMENSIONS*YDIMENSIONS-1);       // trying hack for non 2048 populations
                        
	            ushort x_coord_neuron=neuron_id % XDIMENSIONS;                // X coordinate is lower 4 bits [0:3]
	            ushort y_coord_neuron=neuron_id / YDIMENSIONS;                // Y coordinate is bits [11:4] ??

	            uint pixelid = neuron_id;                           // indexID

		        immediate_data[pixelid]=x_coord_neuron;             // store 1st pixel ID
	            history_data[updateline][pixelid]=x_coord_neuron;  // replace any data here already
	            immediate_data[pixelid+1]=y_coord_neuron;           // store 2nd pixel ID
	            history_data[updateline][pixelid+1]=y_coord_neuron;// replace any data here already
				}
	          }
		}


        if (SIMULATION==COCHLEA) 	{ //  QL for silicon cochlea 27th Aug 2013, CP incorporated 4th Sept 2013.
	       if (freezedisplay==0) {  // so long as the display is still active then listen to new input          
              short neuronID=scanptr->data[0]%0x0800;
              short coreID=(scanptr->data[0]>>11)%0x20;
              short x_chip=(scanptr->data[0]>>24);
              short y_chip=(scanptr->data[0]>>16)%0x0100;
              short NUM_Cell=4;
              short NUM_Channel=64;               
              short x_coord=(coreID-1)*NUM_Cell+neuronID%NUM_Cell;
              short y_coord=neuronID/NUM_Cell;
                
              uint pixelid = (x_coord*NUM_Channel) + y_coord;
              immediate_data[pixelid]+=1;
              history_data[updateline][pixelid]=immediate_data[pixelid];
             }
        }


         if (SIMULATION==RATEPLOTLEGACY) {
            uint chippopulationid=(scanptr->srce_port&0x1F)-1;            // Francesco maps Virtual CPU ID to population 1:1 (at present)
            xsrc=scanptr->srce_addr/256; // takes the chip ID and works out the chip X coord
            ysrc=scanptr->srce_addr%256; // and the chip Y coord
            //printf("Xsrc:%d, YSrc:%d\n",xsrc,ysrc);
            xcoord = (xsrc*EACHCHIPX)+(chippopulationid/EACHCHIPY);        // each chip has 16 population values, 0=bottom left
            ycoord = ((ysrc*EACHCHIPY)+(chippopulationid%EACHCHIPY));// 3 = top left, 12=BtmR, 15=TopR
            //printf("chippop:%d,  xcoord:%d, ycoord:%d\n",chippopulationid,xcoord,ycoord);
            //uint populationid=(ycoord*YDIMENSIONS)+xcoord;
            uint populationid=(EACHCHIPX*EACHCHIPY)*((ysrc*(XDIMENSIONS/EACHCHIPX))+xsrc) + chippopulationid;
            // (EACHCHIPX*EACHCHIPY)*((ysrc*(XDIMENSIONS/EACHCHIPX))+xsrc) + populationid
            //printf("PopulationID: %d. Original: %d\n",populationid,chippopulationid);
            uint commandcode=scanptr->cmd_rc;
            uint intervalinms=scanptr->arg2+1;                // how often we are getting population firing counts for this population
            uint neuronsperpopulation=scanptr->arg3;            // how many neurons are in this population ID

            if (freezedisplay==0 && commandcode==257) {            // if we are not paused, going to populate rata data
               biascurrent[populationid]=(float)scanptr->arg1/256.0;      // 8.8 fixed format data for the bias current used for this population
               //if (populationid!=500) printf("BiasCurr: %f.  Interval: %dms,  NeuronsPerPopulation: %d.\n",biascurrent[populationid], intervalinms, neuronsperpopulation);
               for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
                  uint spikesperinterval=scanptr->data[i];    // Spikes per interval for this population
                  immediate_data[populationid]=(spikesperinterval*1000.0)/(float)(intervalinms*neuronsperpopulation);    // for this population stores average spike rate - in spikes per neuron/second
                  history_data[updateline][populationid]=immediate_data[populationid];            // replace any data here already
                  // printf("Data Raw:%d, PopID: %d,  Value: %d. Average %d spikes/neuron/s in this population of %d neurons each %d ms.\n",scanptr->data[i], populationid, spikesperinterval, immediate_data[populationid],neuronsperpopulation,intervalinms );
               }    //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
               somethingtoplot=1;                    // indicate we should refresh the screen as we likely have new data
            }  // receive rate data
            if (freezedisplay==0 && commandcode==256) {            // if we are not paused, going to populate raster spike data
               for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
                  uint neuronID=scanptr->data[i]&0xFF;        // Which neuron has fired in this population (last 8 bits are significant)
                  if (neuronID<MAXRASTERISEDNEURONS) {
                     if (neuronID<minneuridrx) minneuridrx=neuronID;
                     if (neuronID>maxneuridrx) maxneuridrx=neuronID;
                     history_data_set2[updateline][neuronID]++;    // increment the spike count for this neuron at this time
                     if (history_data_set2[updateline][neuronID]==0) history_data_set2[updateline][neuronID]++;    // increment the spike count for this neuron at this time TODO - this is a comparison of a float and an integer
                     if (outputfileformat==2) {                // write to output file only if required and in NeuroTools format (2)
                        if (writingtofile==0) {
                           writingtofile=1;        // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
                           fprintf(fileoutput,"%lld.0\t%d.0\n",(long long int)sincefirstpacket,neuronID);      // neurotools format (ms and NeurID)
                           writingtofile=0;        // note write finished
                        }
                     }
                  }
                  //if (!(neuronID%100)) printf("Updateline:%d, NeuID: %d, Tally: %f.\n",updateline, neuronID, history_data_set2[updateline][neuronID]);
               }    //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
               somethingtoplot=1;                    // indicate we should refresh the screen as we likely have new data
            }  // receive rate data
         }

         if (SIMULATION==MAR12RASTER) {
            uint commandcode=scanptr->cmd_rc;
            if (freezedisplay==0 && commandcode==80) {            // if we are not paused, going to populate rate data
               for (int i=0; i<numAdditionalBytes/4; i+=2) {      // for all extra data (assuming regular array of paired words, word1=key, word2=data)
                  xsrc=(scanptr->data[i])>>24;
                  ysrc=((scanptr->data[i])>>16)&0xFF;
                  uint chippopulationid=((scanptr->data[i])>>11)&0xF;        // Francesco maps Virtual CPU ID to population 1:1 (at present)
                  printf("CoreID: %d. CorePopID: ",chippopulationid);
                  if (BITSOFPOPID>0) {
                     chippopulationid=chippopulationid<<BITSOFPOPID;            // add space for any per core population IDs (proto pops per core)
                     chippopulationid+=((scanptr->data[i])>>4)&0x3;            // add in proto popid
                  }
                  uint populationid=(EACHCHIPX*EACHCHIPY)*((xsrc*YCHIPS)+ysrc) + chippopulationid;
                  immediate_data[populationid]=(float)scanptr->data[i+1];        // for this population stores average spike rate - in spikes per neuron/second
                  history_data[updateline][populationid]=immediate_data[populationid];            // replace any data here already
               }
            }
         }

         if (SIMULATION==SPIKERVC) {
            uint commandcode=scanptr->cmd_rc;
            for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of paired words, word1=key, word2=data)
               xsrc=(scanptr->data[i])>>24;            // chip x coordinate
               ysrc=((scanptr->data[i])>>16)&0xFF;        // chip y coordinate
               uint chipcore=((scanptr->data[i])>>11)&0xF;    // core of chip (note: 4 bits)
               uint neurid=scanptr->data[i]&0x8FF;        // neuron ID within this core
               printf("CoreID:%d, neurid:%d",chipcore,neurid); // do some printing for debug
               // note the neurid in this example is the only relevant index - there's no relevance of chip ID or core
               // if this is relevant then make the array indexes dependant on this data
               immediate_data[neurid]=1;            // make the data valid to say (at least) one spike received in the immediate data
               history_data[updateline][neurid]=immediate_data[neurid];        // make the data valid to say (at least) one spike received in this historical index data
            }
         }

         if (SIMULATION==RATEPLOT) {
            uint commandcode=scanptr->cmd_rc;
            if (freezedisplay==0 && (commandcode==64 || commandcode==65 || commandcode==66)) {            // if we are not paused, going to populate rate data
               for (int i=0; i<numAdditionalBytes/4; i+=2) {      // for all extra data (assuming regular array of paired words, word1=key, word2=data)
                  // read header info, x,y,core,pop.
                  xsrc=(scanptr->data[i])>>24;
                  ysrc=((scanptr->data[i])>>16)&0xFF;
                  uint chippopulationid=((scanptr->data[i])>>11)&0xF;        // Francesco maps Virtual CPU ID to population 1:1 (at present)
                  //printf("CoreID: %d. CorePopID: ",chippopulationid);
                  if (BITSOFPOPID>0) {
                     chippopulationid=chippopulationid<<BITSOFPOPID;            // add space for any per core population IDs (proto pops per core)
                     chippopulationid+=((scanptr->data[i])>>4)&0x3;            // add in proto popid
                  }
                  uint populationid=(EACHCHIPX*EACHCHIPY)*((xsrc*YCHIPS)+ysrc) + chippopulationid;
                  //printf("%d:PopID. xsrc:%d, ysrc:%d  linearchipid=%d\n",populationid, xsrc, ysrc, ((xsrc*YCHIPS)+ysrc));

                  if (populationid>(YDIMENSIONS*XDIMENSIONS)) commandcode = 11;    // ignore anything that will go offscreen

                  //printf("%d:PopID. SubID:%d.  (orig:%x)\n",populationid, ((scanptr->data[i])>>4)&0x3, chippopulationid,(scanptr->data[i]));
                  if (commandcode==64) {
                     immediate_data[populationid]=(float)scanptr->data[i+1];        // for this population stores average spike rate - in spikes per neuron/second
                     history_data[updateline][populationid]=immediate_data[populationid];            // replace any data here already
                     //immediate_data[populationid]=scanptr->data[i+1];        // for this population stores average spike rate - in spikes per neuron/second
                     //history_data[updateline][populationid]=immediate_data[populationid];            // replace any data here already
                     //printf("Rate  ");
                  } else if (commandcode==65) {
                     biascurrent[populationid]=(float)scanptr->data[i+1]/256.0;      // 8.8 fixed format data for the bias current used for this population
                     //printf("Bias  ");
                  } else if (commandcode==66) {    // means we are plotting voltage
                     float tempstore=(short)scanptr->data[i+1];
                     tempstore/=256.0;
                     //tempstore+=100.0;
                     immediate_data[0]=tempstore;        // only 1 value to plot - the potential!
                     //printf("Received value:%d becomes:%f, is Int:%u or Float:%f.\n",(short)scanptr->data[i+1],(float)((short)scanptr->data[i+1])/256.0,immediate_data[0],tempstore);
                     history_data[updateline][0]=immediate_data[0];            // replace any data here already
                  }
               }    //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
               somethingtoplot=1;                    // indicate we should refresh the screen as we likely have new data
            }  // receive rate data

            if (freezedisplay==0 && commandcode==256) {            // if we are not paused, going to populate raster spike data
               //printf("Spike  ");
               for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
                  uint neuronID=scanptr->data[i]&0xFF;        // Which neuron has fired in this population (last 8 bits are significant)
                  if (neuronID<MAXRASTERISEDNEURONS) {
                     if (neuronID<minneuridrx) minneuridrx=neuronID;
                     if (neuronID>maxneuridrx) maxneuridrx=neuronID;
                     history_data_set2[updateline][neuronID]++;    // increment the spike count for this neuron at this time
                     if (history_data_set2[updateline][neuronID]==0) history_data_set2[updateline][neuronID]++;    // increment the spike count for this neuron at this time TODO - this is a comparison of a float and an integer
                     if (outputfileformat==2) {                // write to output file only if required and in NeuroTools format (2)
                        if (writingtofile==0) {
                           writingtofile=1;        // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
                           fprintf(fileoutput,"%lld.0\t%d.0\n",(long long int)sincefirstpacket,neuronID);      // neurotools format (ms and NeurID)
                           writingtofile=0;        // note write finished
                        }
                     }
                  }
                  //if (!(neuronID%100)) printf("Updateline:%d, NeuID: %d, Tally: %f.\n",updateline, neuronID, history_data_set2[updateline][neuronID]);
               }    //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
               somethingtoplot=1;                    // indicate we should refresh the screen as we likely have new data
            }  // receive rate data
         }

         if (SIMULATION==HEATMAP) {
            xsrc=scanptr->srce_addr/256; // takes the chip ID and works out the chip X coord
            ysrc=scanptr->srce_addr%256; // and the chip Y coord
            for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
               uint arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc) + i;
               if (freezedisplay==0) {
                  if (arrayindex<0 || arrayindex>XDIMENSIONS*YDIMENSIONS) {
                     printf("Error line 772: Array index out of bounds: %u. (x=%u, y=%u)\n",arrayindex,xsrc,ysrc);        // CPDEBUG
                  } else {
                     immediate_data[arrayindex]=(float)scanptr->data[i]/(float)pow(2.0,FIXEDPOINT);
                     if (immediate_data[arrayindex] > highwatermark) printf ("new hwm [%d, %d, %d] = %f\n", xsrc, ysrc, i, immediate_data[arrayindex]);
                     if (updateline<0 || updateline>HISTORYSIZE) {
                        printf("Error line 776: Updateline is out of bounds: %d.\n",updateline);        // CPDEBUG
                     } else {
                        history_data[updateline][arrayindex]=immediate_data[arrayindex];        // replace any data already here
                     }
                  }
                  somethingtoplot=1;                // indicate we will need to refresh the screen

               }
               //recombine to single vector - if display paused don't update what's there
               // send to log file for plotting (overwriting what's already here)
            }
         }

         if (SIMULATION==LINKCHECK) {
            xsrc=(scanptr->srce_addr/256); // takes the chip ID and works out the base X coord
            ysrc=(scanptr->srce_addr%256); // and the chip base Y coord
            int indexer=(ysrc*(EACHCHIPX*EACHCHIPY)); // give the y offset
            indexer+=(xsrc*(EACHCHIPX*EACHCHIPY)*(YDIMENSIONS/EACHCHIPY));

            for (int i=0; i<(EACHCHIPX*EACHCHIPY); i++) {
               immediate_data[indexer+i]=100;
               if (i==5) immediate_data[indexer+i]=20;        // set centre blue
               if (i==2 || i==8 || i==3 || i==7 || i>10) immediate_data[indexer+i]=0;    // set top left and btm right black
            }

            if (xsrc>7) printf("X out of bounds. Src: 0x%x, %d\n",scanptr->srce_addr,xsrc);
            if (ysrc>7) printf("Y out of bounds. Src: 0x%x, %d\n",scanptr->srce_addr,ysrc);
            if (freezedisplay==0) {
               for(int i=0; i<6; i++) {
                  //int arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc);    // base index
                  if (scanptr->arg1&(0x1<<i)) {    // if array entry is set (have received on this port)
                     int arrayindex=indexer;
                     if (i==0) arrayindex+=1;         // RX from west
                     //if (i==1) arrayindex+=1;         // RX from sw (zero position)
                     if (i==2) arrayindex+=4;         // RX from south
                     if (i==3) arrayindex+=9;         // RX from east
                     if (i==4) arrayindex+=10;         // RX from ne
                     if (i==5) arrayindex+=6;         // RX from north
                     immediate_data[arrayindex]=60;          // update immediate data
                     history_data[updateline][arrayindex]=immediate_data[arrayindex];// update historical data
                     somethingtoplot=1;            // indicate we will need to refresh the screen
                  }
               }
            }
         }

         if (SIMULATION==CPUUTIL) {
            xsrc=scanptr->srce_addr/256; // takes the chip ID and works out the chip X coord
            ysrc=scanptr->srce_addr%256; // and the chip Y coord
            for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
               uint arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc) + i;
               if (freezedisplay==0) {
                  immediate_data[arrayindex]=(float)scanptr->data[i];                 // utilisation data
                  history_data[updateline][arrayindex]=immediate_data[arrayindex];                // replace any data already here
                  somethingtoplot=1;                                // indicate we will need to refresh the screen
               }
               //recombine to single vector - if display paused don't update what's there
               // send to log file for plotting (overwriting what's already here)
            }
         }

         if (SIMULATION==CHIPTEMP) {
            xsrc=scanptr->srce_addr/256; // takes the chip ID and works out the chip X coord
            ysrc=scanptr->srce_addr%256; // and the chip Y coord
            uint arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc);    // no per core element so no +i
            if (freezedisplay==0) {
               //immediate_data[arrayindex]=(float)scanptr->cmd_rc/(float)pow(2.0,FIXEDPOINT);     // temperature data using algorithm in report.c (not great)
               // immediate_data[arrayindex]=((float)scanptr->arg1-6300) /15.0;     // temperature 1 sensor data
               // immediate_data[arrayindex]=((float)scanptr->arg2-9300) / 18.0;     // temperature 2 sensor data
               // immediate_data[arrayindex]=(55000-(float)scanptr->arg3)/450.0;     // temperature 3 sensor data
               immediate_data[arrayindex]=((((float)scanptr->arg1-6300) /15.0) + (((float)scanptr->arg2-9300) / 18.0) + ((55000-(float)scanptr->arg3)/450.0) - 80.0) / 1.5;
               // scale to something approximating 0->100  for the extremities spotted (so far - may need to tinker!)
               history_data[updateline][arrayindex]=immediate_data[arrayindex];            // replace any data already here
               somethingtoplot=1;                            // indicate we will need to refresh the screen

            }
            //recombine to single vector - if display paused don't update what's there
            // send to log file for plotting (overwriting what's already here)
         }

         if (SIMULATION==INTEGRATORFG) {
            xsrc=scanptr->srce_addr/256; // takes the chip ID and works out the chip X coord
            ysrc=scanptr->srce_addr%256; // and the chip Y coord
            uint arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc);    // no per core element so no +i
            for (int i=0; i<numAdditionalBytes/4; i++) {      // for all extra data (assuming regular array of 4 byte words)
               if (freezedisplay==0) {
                  if (i==0) {                // only interested in first data item
                     //float input=1.0+(float)((int)scanptr->data[i])/(float)pow(2.0,FIXEDPOINT);
                     float input=1.0+(float)((int)scanptr->data[i])/256.0;
                     input*=(0.001/0.03);
                     int previousindex=updateline-1;
                     if (previousindex<0) previousindex=HISTORYSIZE;
                     float pastdata=0.0;
                     if (history_data[previousindex][arrayindex]>(NOTDEFINEDFLOAT+1))
                        pastdata=history_data[previousindex][arrayindex];
                     input+=pastdata*exp(-0.001/0.03);
                     immediate_data[arrayindex]=input;                                   // wobbler data
                     history_data[updateline][arrayindex]=immediate_data[arrayindex];    // replace any data already here
                     somethingtoplot=1;                            // indicate we will need to refresh the screen
                  }
               }
            }
            //recombine to single vector - if display paused don't update what's there
            // send to log file for plotting (overwriting what's already here)
         }


         if (outputfileformat==1) {                // write to output file only if required and in normal SPINNAKER packet format (1) - basically the UDP payload
            short test_length=numbytes_input;
            int64_t test_timeoffset=(nowtime-firstreceivetimez);
            if (writingtofile==0) {    // can only write to the file if its not paused and can write
               writingtofile=1;        // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
               fwrite(&test_length, sizeof(test_length), 1, fileoutput);
               fwrite(&test_timeoffset, sizeof(test_timeoffset), 1, fileoutput);
               fwrite(&buffer_input, test_length, 1, fileoutput);
               writingtofile=0;        // note write finished
            }
         }

         if (outputfileformat!=0) fflush (fileoutput);        // will have written something in this section to the output file - flush to the file now
         fflush (stdout);                        // flush IO buffers now - and why not? (immortal B. Norman esq)

      }       // discarding any hello packet by dropping out without processing
   }
}

void readmappings(char* filenamea, char* filenameb) {
//  const char filenamea[] = "maplocaltoglobal.csv";
//  const char filenameb[] = "mapglobaltolocal.csv";

   FILE *filea = fopen(filenamea, "r");
   if ( filea ) {
      size_t i, j, k;
      char buffer[BUFSIZ], *ptr;
      for ( i = 0; fgets(buffer, sizeof buffer, filea); ++i )
         for ( j = 0, ptr = buffer; j < (sizeof(*maplocaltoglobal)/sizeof(*(*maplocaltoglobal))); ++j, ++ptr ) {
            if (i<XDIMENSIONS*YDIMENSIONS) {
               maplocaltoglobal[i][j] = (int)strtol(ptr, &ptr, 10);
               maplocaltoglobalsize=i;
            }
         }
   } else perror(filenamea);
   fclose(filea);

   FILE *fileb = fopen(filenameb, "r");
   if ( fileb ) {
      size_t i, j, k;
      char buffer[BUFSIZ], *ptr;
      for ( i = 0; fgets(buffer, sizeof buffer, fileb); ++i )
         for ( j = 0, ptr = buffer; j < (sizeof(*mapglobaltolocal)/sizeof(*(*mapglobaltolocal))); ++j, ++ptr ) {
            if (i<XDIMENSIONS*YDIMENSIONS) {
               mapglobaltolocal[i][j] = (int)strtol(ptr, &ptr, 10);
               mapglobaltolocalsize=i;
            }
         }
   } else perror(filenameb);
   fclose(fileb);

   size_t i, j, k;
   for ( j = 0; j <= mapglobaltolocalsize; ++j ) {
      printf("mapglobaltolocal[%lu]: ", (long unsigned)j);
      for ( k = 0; k < (sizeof(*mapglobaltolocal)/sizeof(*(*mapglobaltolocal))); ++k ) printf("%4d ", mapglobaltolocal[j][k]);
      putchar('\n');
   }
   for ( j = 0; j <= maplocaltoglobalsize; ++j ) {
      printf("maplocaltoglobal[%lu]: ", (long unsigned)j);
      for ( k = 0; k < (sizeof(*maplocaltoglobal)/sizeof(*(*maplocaltoglobal))); ++k ) printf("%4d ", maplocaltoglobal[j][k]);
      putchar('\n');
   }
   mappingfilesread=1;
}

void* load_stimulus_data_from_file (void *ptr)
{
   int64_t sincefirstpacket,nowtime,filestarttime=-1;
   int64_t howlongrunning, howlongtowait;                // for timings
   struct timeval stopwatchus, deltateeus;                // declare timing variables
   struct timespec ts;                        // used for calculating how long to wait for next frame
   int numbytes;
   char sdp_header_len=26;
   //unsigned char test_buffer[1500]; // max size

   //int a1 = new int[SIZE];
   short fromfilelenproto;            // allocate new heap memory for a buffer for reading up to 100k packets in
   int64_t fromfileoffsetproto;        // allocate new heap memory for a buffer for reading up to 100k packets in
   sdp_msg fromfileproto;            // allocate new heap memory for a buffer for reading up to 100k packets in

   uint numberofpackets=0;
   uint filesimtime;                    // time in ms offset from 1st packet at beginning of file
   int64_t startimer=-1,endtimer=-1;

   printf("\nChecking File Length...\n",numberofpackets-1);

   while (fread(&fromfilelenproto, sizeof (fromfilelenproto), 1, fileinput)) {
      //printf("Length: %d\n",fromfilelen[numberofpackets]);
      fread(&fromfileoffsetproto, sizeof(fromfileoffsetproto), 1, fileinput);
      if (startimer==-1) startimer=fromfileoffsetproto;
      if (endtimer<fromfileoffsetproto) endtimer=fromfileoffsetproto;
      //printf("TimeOffset: %d\n",fromfileoffset[numberofpackets]);
      fread(&fromfileproto, fromfilelenproto, 1, fileinput);
      //printf("Balls to it: %c\n",test_buffer[0]);
      numberofpackets++;
   }

   fseek(fileinput,0 ,SEEK_SET);                    // reset position
   printf("Detected: %d packets in input file over %3.1fs. Allocating memory and loading...\n",numberofpackets-1,(float)(endtimer-startimer)/1000000.0);

   int buffsize=100000;                        // max number of packets to load each time
   if (numberofpackets<buffsize) buffsize=numberofpackets;        // size for the number of packets we have

   short *fromfilelen = new short[buffsize];            // allocate new heap memory for a buffer for reading packets into
   int64_t *fromfileoffset = new int64_t[buffsize];        // allocate new heap memory for a buffer for reading packets into
   sdp_msg *fromfile = new sdp_msg[buffsize];            // allocate new heap memory for a buffer for reading packets into
   printf("Memory Chunk Allocated:..*%d. Now transmitting...\n",buffsize);

   int stilltosend=numberofpackets-1;                // keep a tally of how many to go!
   int keepyuppyproblemo=0;                    // work out if we are keeping up, if we fall 1+ sec behind on playback print an inaccuracy warning.
   while (stilltosend>0) {
      int chunktosend=min(100000,stilltosend);
      for(int i=0; i<chunktosend; i++) {
         fread(&fromfilelen[i], sizeof (fromfilelen[i]), 1, fileinput);
         fread(&fromfileoffset[i], sizeof(fromfileoffset[i]), 1, fileinput);
         fread(&fromfile[i], fromfilelen[i], 1, fileinput);
         //printf("%d of %d) Balls to it, size: %d\n",i, chunktosend, fromfilelen[i]);
      }

      //printf("Loaded next chunk of: %d packets from the file. Starting transmission...\n",chunktosend);

      for (int i=0; i<chunktosend; i++) {
         short extradata=fromfilelen[i];
         int64_t targettime=(int64_t)((float)fromfileoffset[i]/(float)playbackmultiplier);

         //sdp_msg *scanptr3 = &fromfile[i];
         //printf("PacketID: %d, Size %u, and TimeOffset (us):%lu.\n",i,extradata,targettime);
         //  printf("ip_time_out: %d. Padded by: %d\n",scanptr3->ip_time_out,scanptr3->pad);
         //printf("flags: %u. Tagged by: %u\n",scanptr3->flags,scanptr3->tag);
         //printf("dest_port: %u. srce_port: %u\n",scanptr3->dest_port,scanptr3->srce_port);
         //printf("dest_addr: %u. srce_addr: %u\n",scanptr3->dest_addr,scanptr3->srce_addr);
         //printf("cmd_rc: %x.\n",scanptr3->cmd_rc);
         //printf("arg1: %u.\n",scanptr3->arg1);
         //printf("arg2: %u.\n",scanptr3->arg2);
         //printf("arg3: %u.\n",scanptr3->arg3);

         gettimeofday(&stopwatchus,NULL);                // grab current time
         nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us

         if (filestarttime==-1) filestarttime=nowtime-targettime;    // if 1st packet then note it's arrival (typically timeoffset==0 for 1st packet)

         howlongtowait = (filestarttime+targettime)-nowtime;        // how long in us until we need to send the next packet
         //printf("Now: %lu,   Target: %lu.  Therefore sleeping for %d us and %d ns\n",nowtime,targettime+filestarttime,howlongtowait,howlongtowait*1000);

         if (howlongtowait>0) {
            ts.tv_sec = howlongtowait/1000000;                // # seconds
            ts.tv_nsec = (howlongtowait%1000000)*1000;            // us * 1000 = nano secs
            nanosleep (&ts, NULL);            // if we are ahead of schedule sleep for a bit
         }

         if (howlongtowait<-1000000 && keepyuppyproblemo++==0)
            printf("\n\n\n***** Warning having trouble keeping up - times may be inaccurate *****\n"); // if we fall more than 1sec behind where we should be

         if (spinnakerboardipset!=0) {                // if we don't know where to send don't send!
            if (numbytes = sendto(sockfd, &fromfile[i], extradata, 0, p->ai_addr, p->ai_addrlen) == -1)    {
               perror("oh dear - we didn't send our data!\n");
               exit(1);
            }
         }          // write to the Ethernet (127.0.0.1 and relevant port number)
      }
      stilltosend-=chunktosend;    // reduce the number of packets still to send
   }

   fclose (fileinput);  // we've now send all the data,
   fileinput=NULL;

   delete[] fromfilelen;
   delete[] fromfileoffset;
   delete[] fromfile;    // free up buffer space used

   printf("\nAll packets in the file were sent. Finished.\n\n");
   freezedisplay=1;
   gettimeofday(&stopwatchus,NULL);                    // grab current time
   freezetime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us

}


void init_sdp_sender()
{
   char portno_input[7];
   snprintf (portno_input, 6, "%d", spinnakerboardport);

   //printf("Hello: --%s--\n",portno_input);

   int rv, numbytes, block_data_len, block_id, i,j,length, remaining_bytes;

   bzero(&hints_output, sizeof(hints_output));
   hints_output.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
   hints_output.ai_socktype = SOCK_DGRAM; // type UDP (socket datagram)

   if ((rv = getaddrinfo(inet_ntoa(spinnakerboardip), portno_input, &hints_output, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      //return 1;
   } else {
      // loop through all the results and make a socket
      for(p = servinfo; p != NULL; p = p->ai_next) {
         if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
         }
         break;
      }

      if (p == NULL) {
         fprintf(stderr, "OH-CRAP: failed to bind socket\n");
         //return 2; // 2 means we couldn't bind to our socket (probably already running)
      }
   }
   //printf("Hello: --%s--\n",portno_input);

   // freeaddrinfo(servinfo);  // at the end only

   //printf ("SDP UDP sender setup complete!\n");      // here ends the UDP sender setup witchcraft
}


void sdp_sender(unsigned short dest_add, unsigned char dest_port, unsigned int command, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned char extrawords, ...)
{

   va_list ExtraData;            // Initialise list of extra data
   va_start(ExtraData, extrawords);    // Populate it - it's just after the extra words argument

   unsigned int numbytes, sdplength;
   struct sdp_msg output_packet;        // create the SDP message we are going to send;
   struct sdp_msg *pointworkingpacket;     // create a pointer to it too
   pointworkingpacket = &output_packet;

   output_packet.ip_time_out=0;        // n/a
   output_packet.pad=0;            // n/a
   output_packet.flags=7;            // defaults
   output_packet.tag=255;            // not used CP Changed 1st November 2011.
   output_packet.dest_port=dest_port;    // dest port supplied externally  Was: 0x21; // core = 1,  port = 1
   output_packet.srce_port=0xFF;        // Ethernet
   output_packet.dest_addr=htons(dest_add);// supplied externally
   output_packet.srce_addr=0;        // from outside world not a SpiNNaker chip
   output_packet.cmd_rc=(unsigned short) command;    // from outside world (host ordered)
   output_packet.seq=0;            // seq code nullified - per ST email 27th Oct 2011
   output_packet.arg1=arg1;        // argument1
   output_packet.arg2=arg2;        // argument2
   output_packet.arg3=arg3;        // argument3

   for(short i=0; i<extrawords; i++) output_packet.data[i]=va_arg(ExtraData,unsigned int);
   va_end(ExtraData); // de-initialize the list

   sdplength=26+(4*extrawords);        // only send extra data if it's supplied

   if (spinnakerboardipset!=0) {                // if we don't know where to send don't send!
      if ((numbytes = sendto(sockfd, pointworkingpacket, sdplength, 0, p->ai_addr, p->ai_addrlen)) == -1)    {
         perror("oh dear - we didn't send our data!\n");
         exit(1);
      }
   }
   int64_t nowtime;
   struct timeval stopwatchus;
   gettimeofday(&stopwatchus,NULL);                // grab current time
   nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us
   printpktgone=nowtime;                        // initialise the time the message started being displayed
   //printf("Pkt sent:\n");
}


void error(char *msg)
{
   perror(msg);
   exit(1);
}


void cleardown (void)
{
   //for (int i=0;i<(xdim*ydim);i++) immediate_data[i]=NOTDEFINEDFLOAT;   // superfluous
   for (int i=0; i<(xdim*ydim); i++) immediate_data[i]=INITZERO?0.0:NOTDEFINEDFLOAT;
   highwatermark = HIWATER;                    // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
   lowwatermark = LOWATER;                        // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
   xflip=XFLIP;
   yflip=YFLIP;
   vectorflip=VECTORFLIP;
   rotateflip=ROTATEFLIP;

   //printf("Cleared the Slate\n");
}


//-------------------------------------------------------------------------
//  Draws a string at the specified coordinates.
//-------------------------------------------------------------------------
void printgl (float x, float y, void *font_style, char* format, ...)
{
   va_list arg_list;
   char str[256];
   int i;

   // font options:  GLUT_BITMAP_8_BY_13 GLUT_BITMAP_9_BY_15 GLUT_BITMAP_TIMES_ROMAN_10 GLUT_BITMAP_HELVETICA_10 GLUT_BITMAP_HELVETICA_12 GLUT_BITMAP_HELVETICA_18 GLUT_BITMAP_TIMES_ROMAN_24

   va_start(arg_list, format);
   vsprintf(str, format, arg_list);
   va_end(arg_list);

   glRasterPos2f (x, y);

   for (i = 0; str[i] != '\0'; i++)
      glutBitmapCharacter(font_style, str[i]);
}

void printglstroke (float x, float y, float size, float rotate, char* format, ...)
{
   va_list arg_list;
   char str[256];
   int i;
   GLvoid *font_style = GLUT_STROKE_ROMAN;

   va_start(arg_list, format);
   vsprintf(str, format, arg_list);
   va_end(arg_list);

   glPushMatrix();
   glEnable(GL_BLEND);   // antialias the font
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_LINE_SMOOTH);
   glLineWidth(1.5);   // end setup for antialiasing
   glTranslatef(x, y, 0);
   glScalef(size, size, size);
   glRotatef(rotate,0.0,0.0,1.0);
   for (i = 0; str[i] != '\0'; i++)  glutStrokeCharacter(GLUT_STROKE_ROMAN, str[i]);
   glDisable(GL_LINE_SMOOTH);
   glDisable(GL_BLEND);
   glPopMatrix();
}


void convert_index_to_coord(int index, int *x, int *y)
{
   int elementid = (index%(EACHCHIPX*EACHCHIPY));
   int elementx = elementid/EACHCHIPY;
   int elementy = elementid%EACHCHIPY;
   int tileid = (index/(EACHCHIPX*EACHCHIPY));
   int tilex = tileid/(YDIMENSIONS/EACHCHIPY);
   int tiley = tileid%(YDIMENSIONS/EACHCHIPY);

   int xcord = (tilex*EACHCHIPX)+elementx;
   int ycord = (tiley*EACHCHIPY)+elementy;

   *x = xcord;
   *y = ycord;
}
// call with: convert_index_to_coord(index, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)




int convert_coord_to_index(int x, int y)
{
   int elementx = x % EACHCHIPX;            // x position in tile
   int elementy = y % EACHCHIPY;            // y position in tile
   int tilex = x/EACHCHIPX;            // x tile
   int tiley = y/EACHCHIPY;            // y tile

   int elementid=(elementx*EACHCHIPY)+elementy;
   int index=(EACHCHIPX*EACHCHIPY)*((tilex*(YCHIPS))+tiley) + elementid;  // CP 16thJan to support #chips rather than calculated

   return index;
}
// call with: index = convert_coord_to_index(xcoordinate, ycoordinate);



int coordinate_manipulate(int ii)
{
   int i=ii;                            // begin with the assumption of no flipping of coordinates
   int xcoordinate, ycoordinate;

   if (yflip || xflip || vectorflip || rotateflip) {
      int elementid = (i%(EACHCHIPX*EACHCHIPY));
      int elementx = elementid/EACHCHIPY;
      int elementy = elementid%EACHCHIPY;
      int tileid = (i/(EACHCHIPX*EACHCHIPY));
      int tilex = tileid/(YDIMENSIONS/EACHCHIPY);
      int tiley = tileid%(YDIMENSIONS/EACHCHIPY);
      //printf("i=%d. Xcord=%d, ycord=%d, tilex=%d, tiley=%d.\n",i,xcord,ycord,tilex,tiley);
      if (yflip) {
         elementy=(EACHCHIPY-1)-elementy;        // flip ycords
         tiley=((YDIMENSIONS/EACHCHIPY)-1)-tiley;
      }
      if (xflip) {
         elementx=(EACHCHIPX-1)-elementx;        // flip xcoords
         tilex=((XDIMENSIONS/EACHCHIPX)-1)-tilex;
      }
      //printf("     Xcord=%d, ycord=%d, tilex=%d, tiley=%d.\n",i,xcord,ycord,tilex,tiley);

      elementid=(elementx*EACHCHIPY)+elementy;
      i=(EACHCHIPX*EACHCHIPY)*((tilex*(XDIMENSIONS/EACHCHIPX))+tiley) + elementid;

      if (vectorflip) i=(YDIMENSIONS*XDIMENSIONS)-1-i;    // go back to front (cumulative)
      if (rotateflip) {
         //printf("Hello Rotating\n");
         convert_index_to_coord(i, &xcoordinate, &ycoordinate);    // rotate
         i=convert_coord_to_index(ycoordinate, XDIMENSIONS-1-xcoordinate);
      }
   }
   return i;                            // return cumulative reorientation
}


float colour_calculator(float inputty, float hiwater, float lowater)
{
   float scalingfactor = 0;
   float fillcolour = 1.0;
   float diff = hiwater-lowater;

   if (inputty>hiwater) inputty = hiwater;         // for truncated plots or if data supplied is out of range.
   if (inputty<lowater) inputty = lowater;         // for truncated plots or if data supplied is out of range.

   if (diff <= 0.0001) {
      fillcolour = 1.0;                // if in error, or close to a divide by zero (no intensity plotted)
   } else {
      scalingfactor = 1/diff;                // work out how to scale the input data depending on low and highwater values
      fillcolour = (scalingfactor*(inputty-lowater));    // calculate the colour to plot
   }
   if (fillcolour<0.0) fillcolour=0.0;
   if (fillcolour>1.0) fillcolour=1.0;            // must always range between 0 and 1 floating point

   //if (inputty>(NOTDEFINEDFLOAT+1)) printf("Dataz[i]: %f, Hi: %f, Lo: %f, diff: %f,\n  aboveLW:%f  Scaling factor: %f, fillcolour %f\n",inputty,hiwater,lowater,diff,(inputty-lowater),scalingfactor,fillcolour);

   //if (highwatermark>0.0001) fillcolour = 1.0-(inputty/hiwater);  //stop a divide by zero!
   //if (fillcolour !=0) printf("Fillcolour: %f, inputty: %d\n", fillcolour, inputty);
   if (colourused==1) {
   }

   switch(colourused)
   {
   case 1:
   {
#define COLOURSTEPS 6    // 6 different RGB colours, Black, Blue, Cyan, Green, Yellow, Red
      int gamut[COLOURSTEPS][3] = { {0.0,0.0,0.0},{0.0,0.0,1.0},{0.0,1.0,1.0},{0.0,1.0,0.0},{1.0,1.0,0.0},{1.0,0.0,0.0} };

      int colourindex = (float)fillcolour*(float)(COLOURSTEPS-1);
      float colouroffset = (float)(colourindex+1)-(fillcolour*(float)(COLOURSTEPS-1));     // how far away from higher index (range between 0 and 1).
      float R=((1.0-colouroffset)*(float)gamut[colourindex+1][0]) + (colouroffset*(float)gamut[colourindex][0]);
      float G=((1.0-colouroffset)*(float)gamut[colourindex+1][1]) + (colouroffset*(float)gamut[colourindex][1]);
      float B=((1.0-colouroffset)*(float)gamut[colourindex+1][2]) + (colouroffset*(float)gamut[colourindex][2]);
//        printf("Dataz[i]: %f, Hi: %f, Lo: %f, Fillcolour: %f, Index %d\n    Offset %f,  Low %f, Up %f, Tot %f.\n",
//               inputty,hiwater,lowater,fillcolour,colourindex,
//                  colouroffset, (1.0-colouroffset)*(float)gamut[colourindex+1][1],(colouroffset*(float)gamut[colourindex][1]),R);
//        printf("Offset %f, LowGr %f, UpGr %f, Tot %f.\n",
//                  colouroffset, (1.0-colouroffset)*(float)gamut[colourindex+1][1],(colouroffset*(float)gamut[colourindex][1]),G);
//        printf("R %f,   G %f,   B %f.   Offset: %f\n",R,G,B,colouroffset);
      //spilt into n sections. specify colours for each section. how far away from top of section is it
      //multiply R,G,B difference by this proportion

      glColor4f(R,G,B,1.0);              // colours option
      break;
   }
   case 2:
      glColor4f(fillcolour,fillcolour,fillcolour,1.0);    // greyscales option
      break;
   case 3:
      glColor4f(fillcolour,0.0,0.0,1.0);              // redscales only
      break;
   case 4:
      glColor4f(0.0,fillcolour,0.0,1.0);              // greenscales option
      break;
   case 5:
      glColor4f(0.0,0.0,fillcolour,1.0);              // bluescales option
      break;
   case 6:
   {
#define COLOURSTEPSB 5    //  black, purpleymagenta, red, yellow, white (RGB)
      int gamut[COLOURSTEPSB][3] = { {0.0,0.0,0.0},{1.0,0.0,1.0},{1.0,0.0,0.0},{1.0,1.0,0.0},{1.0,1.0,1.0} };

      int colourindex = (float)fillcolour*(float)(COLOURSTEPSB-1);
      float colouroffset = (float)(colourindex+1)-(fillcolour*(float)(COLOURSTEPSB-1));     // how far away from higher index (range between 0 and 1).
      float R=((1.0-colouroffset)*(float)gamut[colourindex+1][0]) + (colouroffset*(float)gamut[colourindex][0]);
      float G=((1.0-colouroffset)*(float)gamut[colourindex+1][1]) + (colouroffset*(float)gamut[colourindex][1]);
      float B=((1.0-colouroffset)*(float)gamut[colourindex+1][2]) + (colouroffset*(float)gamut[colourindex][2]);

      glColor4f(R,G,B,1.0);              // colours option
      break;
   }
   case 7:
      glColor4f(fillcolour<0.01?0.0:1.0,0.0,0.0,1.0);          // everything is red option except v close to 0 (to get rid of flickery colour in line mode) etc.
      break;
   case 8:
      glColor4f(0.0,0.0,fillcolour<0.01?0.0:1.0,1.0);          // everything is white option except v close to 0 (to get rid of flickery colour in line mode etc.
      break;

   }



   return fillcolour;
}



void displayb(void)                            // not currently used
{
   glLoadIdentity();
   glutSetWindow(windowToUpdate);                    // this used to give a blank screen for the 2nd screen when loaded (so it doens't flash scr1)
   glLoadIdentity();
   //glutPostRedisplay();
   glClearColor (0,0,0,1.0);                     // background colour - grey surround
   glClear(GL_COLOR_BUFFER_BIT);
   glutSwapBuffers();             // no flickery gfx
}



// display function, called whenever the display window needs redrawing
void display(void)
{

   int64_t nowtime;
   float timeperindex = displayWindow / (float) plotWidth;    // time in seconds per history index in use

   glPointSize(1.0);

   counter++;                                // how many frames have we plotted in our history

   //struct timeval stopwatchus;                                        // CPDEBUG
   //gettimeofday(&stopwatchus,NULL);                    // grab current time             // CPDEBUG
   //nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec); // get microsec    // CPDEBUG
   //if (counter%10000==0) printf("Counter: %d. Nowtime: %llu  Starttime: %llu,   Diff: %llu\n",counter,nowtime,starttimez,nowtime-starttimez ); // CPDEBUG

   glLoadIdentity();
   glutSetWindow(windowToUpdate);                    // specifically look at our plotting window
   glLoadIdentity();

   //glutPostRedisplay();

   glClearColor (0.8,0.8,0.8,1.0);                     // background colour - grey surround
   glClear(GL_COLOR_BUFFER_BIT);

   // if we want a forced black background then clear it now
   if (BLACKBACKGROUND && displaymode==TILED) {
      glColor4f(0.0,0.0,0.0,1.0);                         // Black for filling
      glBegin(GL_QUADS);
      glVertex2f(windowBorder, windowBorder);  //btm left
      glVertex2f(windowWidth-windowBorder-keyWidth, windowBorder); //btm right
      glVertex2f(windowWidth-windowBorder-keyWidth, windowHeight-windowBorder);  // top right
      glVertex2f(windowBorder, windowHeight-windowBorder); // top left
      glEnd();                           // this plots the basic quad box filled as per colour above
   }


   glColor4f(0.0,0.0,0.0,1.0);                         // Black Text for Labels


   if (printlabels && fullscreen==0) {                            // titles and labels are only printed if border is big enough

      printgl((windowWidth/2)-200,windowHeight-50,GLUT_BITMAP_TIMES_ROMAN_24,(char*)TITLE);        // Print Title of Graph
      //printf("\nSimulation Title: %s\n", TITLE);
//   printgl((windowWidth/2)-278,windowHeight-50,GLUT_BITMAP_TIMES_ROMAN_24,stringy2, MAXFRAMERATE,(1000*counter)/(howlongrunning/1000));        // Print Title of Graph with fps stats
      char stringy3[]="Colour Map (1,2,3..), Mode: (t)iled, (h)istogram, (i)nterpolation, (l)ines, (r)aster, Menu: right click.";
      printgl((windowWidth/2)-250,windowHeight-80,GLUT_BITMAP_HELVETICA_12,stringy3);        // Print subtitle of Graph
      // Graph Title

      char stringy4[]="%d";

      // X Axis
      if (DISPLAYXLABELS) {
         if (displaymode==HISTOGRAM) {
            char stringy1[]="Tile (X,Y) Coords";
            printglstroke ((windowWidth/2)-40, 20, 0.12, 0, stringy1);
         } else if (displaymode==LINES || displaymode==RASTER || displaymode==EEGSTYLE) {
            char stringy1[]="Simulation Time. Window Width=%3.1f secs ('<' '>')";
            printglstroke ((windowWidth/2)-200, 20, 0.12, 0, stringy1, displayWindow*playbackmultiplier);
         } else {
            char stringy1[]="X Coord";
            printglstroke ((windowWidth/2)-25, 20, 0.12, 0, stringy1);
            int xlabels = xdim;
            float xspacing = ((float)(plotWidth)/xdim);
            if (LABELBYCHIP) {
               xlabels = xdim / EACHCHIPX;
               xspacing*=EACHCHIPX;
            }
            int xplotted, spacing=24, lastxplotted=-100;
            for (int i=0; i<xlabels; i++) {                         // X-Axis
               if (i>100) spacing=32;
               xplotted=(i*xspacing)+windowBorder+((xspacing-8)/2)-3;                      // what will be the next x coordinate
               if (xplotted>lastxplotted+spacing) {                        // plot if enough space to not overlap labels.
                  printgl(xplotted, 60,GLUT_BITMAP_HELVETICA_18,stringy4, i);        // Print X Axis Labels at required intervals
                  lastxplotted=xplotted;                                                    // record last x coordinate plotted to
               }
            }
         }
      }
      // X Axis


      if (DISPLAYYLABELS) {
         // Y Axis
         if (displaymode==HISTOGRAM || displaymode==LINES) {
            glColor4f(0.0,0.0,0.0,1.0);                                             // Black Text for Labels
            char stringy5[]="Data Value";
            printglstroke (25, (windowHeight/2)-50, 0.12, 90, stringy5);                            // Print Y-Axis label for Graph
            char stringy8[]="%.2f";
            if (PERCENTAGESCALE) {
               char stringy8[]="%.2f%%";
               printgl (55, windowHeight-(windowBorder)-5, GLUT_BITMAP_HELVETICA_12, stringy8, 100.0);            // Print HighWaterMark Value
               printgl (55, windowBorder-5, GLUT_BITMAP_HELVETICA_12, stringy8, 0.0);                    // Print LowWaterMark Value
            } else {         
               printgl (55, windowHeight-(windowBorder)-5, GLUT_BITMAP_HELVETICA_12, stringy8, highwatermark);            // Print HighWaterMark Value
               printgl (55, windowBorder-5, GLUT_BITMAP_HELVETICA_12, stringy8, lowwatermark);                    // Print LowWaterMark Value
            }
            glColor4f(0.6,0.6,0.6,1.0);
            glLineWidth(1.0);
            glBegin(GL_LINES);
            glVertex2f(windowWidth-windowBorder-keyWidth+10,windowBorder); // rhs
            glVertex2f(windowBorder-10,windowBorder); // inside
            glEnd();
            float interval, difference = highwatermark-lowwatermark;
            for (float i=10000; i>=0.1; i/=10.0) if (difference<i) {
                  if (difference<(i/2)) interval=i/20.0;
                  else interval=i/10.0;
               }
            int multipleprinted=1;
            float linechunkiness = (float)(windowHeight-(2*windowBorder))/(float)(highwatermark-lowwatermark);
            for (int i=0; i<windowHeight-(2*windowBorder); i++) {
               float temperaturehere = ((float)i / linechunkiness)+lowwatermark;
               float positiveoffset=temperaturehere-lowwatermark;
               if (PERCENTAGESCALE) positiveoffset = (positiveoffset/difference)*100.0; // scale it to a range of 0-100
               if (positiveoffset>=(interval*multipleprinted)) {
                  glColor4f(0.6,0.6,0.6,1.0);
                  glLineWidth(1.0);
                  glBegin(GL_LINES);
                  glVertex2f(windowWidth-windowBorder-keyWidth+10,i+windowBorder); // rhs
                  glVertex2f(windowBorder-10,i+windowBorder); // inside
                  glEnd();
                  glColor4f(0.0,0.0,0.0,1.0);
                  printgl (55, i+windowBorder-5, GLUT_BITMAP_HELVETICA_12, stringy8, lowwatermark+(multipleprinted*interval));
                  multipleprinted++;
               }
            }
         } else if (displaymode==RASTER || displaymode==EEGSTYLE) {
            // plotted sequentially up and down
            if (displaymode==RASTER) {
               char stringy5[]="Raster Activity Plot";
               printglstroke (25, (windowHeight/2)-70, 0.12, 90, stringy5);                        // Print Y-Axis label for Graph
            } else {
               char stringy5[]="Data Value";
               printglstroke (25, (windowHeight/2)-50, 0.12, 90, stringy5);                        // Print Y-Axis label for Graph
            }

            int numberofrasterplots=xdim*ydim;
            if (windowToUpdate==win2) numberofrasterplots=maxneuridrx;                    // bespoke for Discovery demo
            float yspacing = (float)(windowHeight-(2*windowBorder))/(float)(numberofrasterplots);            // how many pixels per neuron ID
   
            int lastyplotted = -100;
            for (int i=0; i<numberofrasterplots; i++) {                                 // for all plottable items
               char stringxycoords[]="(%d,%d)";
               //int xcord = i % xdim;                        // X coord is Least significant
               //int ycord = ydim - 1 -(i / xdim);                   // inverted y coords (most signif)
               //printglstroke (windowBorder+((xcord+0.5)*xsize), 60, 0.09, 45, stringxycoords,i % xdim,ydim-(i/xdim)-1);
               int xcord, ycord;
               convert_index_to_coord(i, &xcord, &ycord);  // (where xcoordinate and ycoordinate are ints)
               if (windowToUpdate==win2) {
                  xcord=0;   // bespoke for Discovery demo
                  ycord=i;
               }
               int yplotted=(i*yspacing)+windowBorder+((yspacing-18)/2)+6;     // what will be the next y coordinate
               if (yplotted>lastyplotted+12) {                    // plot only if enough space to not overlap labels.
                  lastyplotted=yplotted;                    // we were the last one plotted
                  if (windowToUpdate==win2) printgl(60,yplotted, GLUT_BITMAP_HELVETICA_12, stringy4, i+1);// Print Y Axis Labels at required intervals (linear format)
                  else printgl(60,yplotted, GLUT_BITMAP_HELVETICA_12, stringxycoords, xcord, ycord);    // Print Y Axis Labels at required intervals (X,Y format)
               }
            }
         } else {
            char stringy5[]="Y Coord";
            printglstroke (25, (windowHeight/2)-50, 0.12, 90, stringy5);                            // Print Y-Axis label for Graph
            int ylabels = ydim;
            float yspacing = ((float)(windowHeight-(2*windowBorder))/ydim);
            if (LABELBYCHIP) {
               ylabels = ydim / EACHCHIPY;
               yspacing*=EACHCHIPY;
            }
            int yplotted, lastyplotted = -100;
            for (int i=0; i<ylabels; i++) {                                          // Y-Axis
               yplotted=(i*yspacing)+windowBorder+((yspacing-18)/2)+2;               // what will be the next y coordinate
               if (yplotted>(lastyplotted+16)) {                                     // plot only if enough space to not overlap labels.
                  printgl(60,(i*yspacing)+windowBorder+((yspacing-18)/2)+2, GLUT_BITMAP_HELVETICA_18, stringy4, i); // Print Y Axis Label
                  lastyplotted = yplotted;                                         // record where last label plotted on the Y axis
               }
            }
         }
      }
      // Y Axis

   }   // titles and labels are only printed if border is big enough


   for(int i=0; i<(xdim*ydim); i++) {
      //if(immediate_data[i]>(NOTDEFINEDFLOAT+1)) immediate_data[i]=immediate_data[i];    // set data to be worked upon superfluous
      //else immediate_data[i]=NOTDEFINEDFLOAT;                                            // out of range  superfluous
      //if (immediate_data[i] == NOTDEFINEDINT) immediate_data[i]=NOTDEFINEDFLOAT;                // if not out of range
      //else immediate_data[i]=(float)immediate_data[i]/(float)pow(2.0,FIXEDPOINT);    // scale data to something sensible for colour gamut
      //printf("Data: %d, POWER: %f  = %f\n", immediate_data[i],(float)pow(2,FIXEDPOINT), immediate_data[i]);
      if(immediate_data[i]>(NOTDEFINEDFLOAT+1)) {    // is valid
         if (immediate_data[i]>MAXDATAFLOAT) immediate_data[i]=MAXDATAFLOAT;            // check: can't increment above saturation level 
         if (immediate_data[i]<MINDATAFLOAT) immediate_data[i]=MINDATAFLOAT;            // check: can't decrement below saturation level 
         if (DYNAMICSCALE) {
            if (immediate_data[i]>highwatermark && spinnakerboardipset) highwatermark=immediate_data[i];    // only alter the high water mark when using dynamic scaling & data received
            if (immediate_data[i]<lowwatermark && spinnakerboardipset) lowwatermark=immediate_data[i];    // only alter the low water mark when using dynamic scaling & data received
         }
      }
   }  // scale all the values to plottable range

   //float xsize = 20;     // changed for dynamic reshaping
   float xsize = ((float)(plotWidth)/xdim);     // changed for dynamic reshaping
   if (xsize<1.0) xsize=1.0;
   float ysize = ((float)(windowHeight-(2*windowBorder))/ydim);        // changed for dynamic reshaping
   float tileratio=xsize/ysize;

   for(int i=0; i<(xdim*ydim); i++) {                  //
      int ii=coordinate_manipulate(i);                // if any manipulation of how the data is to be plotted is required, do it

      int xcord, ycord;
      convert_index_to_coord(i, &xcord, &ycord);      // find out the (x,y) coordinates of where to plot this data

      float magnitude = colour_calculator(immediate_data[ii],highwatermark,lowwatermark);            // work out what colour we should plot - sets 'ink' plotting colour

      // if required, plot tiled mini version in bottom left
      if (DISPLAYMINIPLOT) {
         if (fullscreen==0) {
            float ysize=max((float)1.0,(float)(windowBorder-(6*gap))/(float)ydim);
            float xsize=max((float)1.0,ysize*tileratio);                    // draw little / mini tiled version in btm left - pixel size
            if (immediate_data[ii]>(NOTDEFINEDFLOAT+1)) {                        // only plot if data is valid
               glBegin(GL_QUADS);                            // draw little tiled version in btm left
               glVertex2f((2*gap)+(xcord*xsize), (2*gap)+(ycord*ysize));          //btm left
               glVertex2f((2*gap)+((xcord+1)*xsize), (2*gap)+(ycord*ysize));     //btm right
               glVertex2f((2*gap)+((xcord+1)*xsize), (2*gap)+((ycord+1)*ysize));      // top right
               glVertex2f((2*gap)+(xcord*xsize), (2*gap)+((ycord+1)*ysize));     // top left
               glEnd();                                   // this plots the basic quad box filled as per colour above
            }

            if (livebox==i) {                            // draw outlines for selected box in little / mini version
               glLineWidth(1.0);
               glColor4f(0.0,0.0,0.0,1.0);
               glBegin(GL_LINE_LOOP);
               glVertex2f((2*gap)+(xcord*xsize), (2*gap)+(ycord*ysize));          //btm left
               glVertex2f((2*gap)+((xcord+1)*xsize), (2*gap)+(ycord*ysize));         //btm right
               glVertex2f((2*gap)+((xcord+1)*xsize), (2*gap)+((ycord+1)*ysize));      // top right
               glVertex2f((2*gap)+(xcord*xsize), (2*gap)+((ycord+1)*ysize));         // top left
               glEnd();                           // this plots the external black outline of the selected tile
               glColor4f(1.0,1.0,1.0,1.0);
               glBegin(GL_LINE_LOOP);
               glVertex2f(1+(2*gap)+(xcord*xsize), 1+(2*gap)+(ycord*ysize));          //btm left
               glVertex2f((2*gap)+((xcord+1)*xsize)-1, 1+(2*gap)+(ycord*ysize));     //btm right
               glVertex2f((2*gap)+((xcord+1)*xsize)-1, (2*gap)+((ycord+1)*ysize)-1);      // top right
               glVertex2f(1+(2*gap)+(xcord*xsize), (2*gap)+((ycord+1)*ysize)-1);     // top left
               glEnd();                           // this plots the internal white outline of the selected tile
               glLineWidth(1.0);
            }
            if (SIMULATION==RATEPLOT||SIMULATION==RATEPLOTLEGACY) {
               if (INTERACTION) {
                  if (rasterpopulation==i) {
                     if (magnitude>0.6) glColor4f(0.0,0.0,0.0,1.0);
                     else glColor4f(1.0,1.0,1.0,1.0);
                     //print R
                     char stringR[]="R";
                     printgl((2*gap)+(xcord*xsize)+((xsize-8)/2), (2*gap)+(ycord*ysize)+((ysize-10)/2), GLUT_BITMAP_HELVETICA_12, stringR);        // print an R in the box to indicate this is box that's raster plotting
                  }
               }
            }
         }
      }

      xsize = ((float)(plotWidth)/xdim);     // changed for dynamic reshaping
      if (xsize<1.0) xsize=1.0;
      ysize = ((float)(windowHeight-(2*windowBorder))/ydim);        // changed for dynamic reshaping

      if (displaymode==HISTOGRAM) {
         //xcord = (i%xdim)+((xdim*(ydim-1))-ydim*(i/xdim));            // for histogram is just the data index, reorder into btmleft, btmright, ... topleft, topright coords
         ycord = 0;                                   // for histogram always at origin
         xcord = i;                                // just print in order, easy peasey!

         xsize = ((float)(plotWidth)/(xdim*ydim));                // Histogram means all across X axis
         //if (xsize<1.0) xsize=1.0;
         ysize = magnitude*((float)(windowHeight-(2*windowBorder)));        // Histogram means height of block adjusts based on value
      }

      magnitude = colour_calculator(immediate_data[ii],highwatermark,lowwatermark);            // work out what colour we should plot - sets 'ink' plotting colour

      if (displaymode==HISTOGRAM || displaymode==TILED) {                    // basic plot if not using triangular interpolation
         char stringnums[]="%3.2f";
         if (immediate_data[ii]>(NOTDEFINEDFLOAT+1)) {
            glBegin(GL_QUADS);
            glVertex2f(windowBorder+(xcord*xsize), windowBorder+(ycord*ysize));  //btm left
            glVertex2f(windowBorder+((xcord+1)*xsize), windowBorder+(ycord*ysize)); //btm right
            glVertex2f(windowBorder+((xcord+1)*xsize), windowBorder+((ycord+1)*ysize));  // top right
            glVertex2f(windowBorder+(xcord*xsize), windowBorder+((ycord+1)*ysize)); // top left
            glEnd();                           // this plots the basic quad box filled as per colour above
         }

         if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
            if (INTERACTION) {
               if (livebox==i) {
                  // printf("livebox: %d\n",livebox);
                  // draw 2 borders around box
                  //if (displaymode!=TILED) livebox==-1;    // if not in tiled mode can't alter the data.
                  glLineWidth((float)gap);
                  glColor4f(1.0,1.0,1.0,1.0);
                  glBegin(GL_LINE_LOOP);
                  glVertex2f((gap*1.5)+windowBorder+(xcord*xsize), (gap*1.5)+windowBorder+(ycord*ysize));  //btm left
                  glVertex2f(windowBorder+((xcord+1)*xsize)-(gap*1.5), (gap*1.5)+windowBorder+(ycord*ysize)); //btm right
                  glVertex2f(windowBorder+((xcord+1)*xsize)-(gap*1.5), windowBorder+((ycord+1)*ysize)-(gap*1.5));  // top right
                  glVertex2f((gap*1.5)+windowBorder+(xcord*xsize), windowBorder+((ycord+1)*ysize)-(gap*1.5)); // top left
                  glEnd();                           // this plots the internal white outline of the selected tile
                  glColor4f(0.0,0.0,0.0,1.0);
                  glBegin(GL_LINE_LOOP);
                  glVertex2f((gap*0.5)+windowBorder+(xcord*xsize), (gap*0.5)+windowBorder+(ycord*ysize));  //btm left
                  glVertex2f(windowBorder+((xcord+1)*xsize)-(gap*0.5), (gap*0.5)+windowBorder+(ycord*ysize)); //btm right
                  glVertex2f(windowBorder+((xcord+1)*xsize)-(gap*0.5), windowBorder+((ycord+1)*ysize)-(gap*0.5));  // top right
                  glVertex2f((gap*0.5)+windowBorder+(xcord*xsize), windowBorder+((ycord+1)*ysize)-(gap*0.5)); // top left
                  glEnd();                           // this plots the external black outline of the selected tile
                  glLineWidth(1.0);
               }
            }
         }

         if(plotvaluesinblocks!=0 && xsize>8) {                                    // if we want to plot numbers / values in blocks (& blocks big enough)
            if (immediate_data[ii]>(NOTDEFINEDFLOAT+1)) {
               if (magnitude>0.6) glColor4f(0.0,0.0,0.0,1.0);
               else glColor4f(1.0,1.0,1.0,1.0);            // choose if light or dark labels
               if (displaymode==HISTOGRAM) printglstroke (windowBorder+5+((xcord+0.5)*xsize), windowBorder+((ycord+0.5)*ysize), 0.08, 90, stringnums,immediate_data[ii]);    // sideways if histogram
               else printglstroke (windowBorder-20+((xcord+0.5)*xsize), windowBorder-6+((ycord+0.5)*ysize), 0.12, 0, stringnums,immediate_data[ii]);                // normal
               //printf("immediate_data[%d] = %f.\n",ii,immediate_data[ii]);
            }
         }

      }

      if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
         if (INTERACTION) {
            glColor4f(0.0,0.0,0.0,1.0);
            if (livebox==i) {
               char stringnums[]="%3.2f";
               char stringbiascurrent[]="Bias Curr:";
               printgl(windowWidth-windowBorder,windowBorder, GLUT_BITMAP_HELVETICA_18, stringbiascurrent);
               printgl(windowWidth-windowBorder,windowBorder-30, GLUT_BITMAP_HELVETICA_18, stringnums, biasediting);
               if (!(((biasediting - 0.01) < biascurrent[livebox]) && ((biasediting + 0.01) > biascurrent[livebox]))) {
                  char stringNAK[]="NAK:(last:%3.2f)";
                  printgl(windowWidth-windowBorder,windowBorder-45, GLUT_BITMAP_HELVETICA_12, stringNAK, biascurrent[livebox]);
                  // print 'NAK' if the bias here is different to the last received value
               }
            }
            if (rasterpopulation!=-1) {
               if (displaymode==HISTOGRAM) {
                  char stringraster[]="'R'aster: (%d)";  // rasterpopulation is the active raster ID.
                  if (rasterpopulation!=-1) printgl(windowWidth-windowBorder,windowBorder-60, GLUT_BITMAP_HELVETICA_12, stringraster, (rasterpopulation%xdim)+((xdim*(ydim-1))-ydim*(rasterpopulation/xdim)));
               } else {
                  char stringraster[]="'R'aster: (%d,%d)";  // rasterpopulation is the active raster ID.
                  int xcoordinate, ycoordinate;
                  convert_index_to_coord(rasterpopulation, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)
                  if (rasterpopulation!=-1) printgl(windowWidth-windowBorder,windowBorder-60, GLUT_BITMAP_HELVETICA_12, stringraster, xcoordinate, ycoordinate);
               }
            }
         }
      }


      glColor4f(0.0,0.0,0.0,1.0);

      if (displaymode==HISTOGRAM && printlabels && fullscreen==0) {        // titles and labels are only printed if border is big enough
         char stringxycoords[]="(%d,%d)";
         if ( (((float)plotWidth/(float)(xdim*ydim) )>40.0 )
               || (!(xcord%(int)(40.0/((float)plotWidth/(float)(xdim*ydim))))) ) {    // only print labels that don't overlap (40pixels)
            glLineWidth(1.0);
            glBegin(GL_LINES);
            glVertex2f(windowBorder+((xcord+0.5)*xsize),windowBorder);     // top - used xsize from earlier so this is why below the main plot.
            glVertex2f(windowBorder+((xcord+0.5)*xsize),windowBorder-10);     // inside
            glEnd();
            //printglstroke (windowBorder+((xcord+0.5)*xsize), 60, 0.09, 45, stringxycoords,i % xdim,ydim-(i/xdim)-1);
            int xcoordinate, ycoordinate;
            convert_index_to_coord(i, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)
            printglstroke (windowBorder+((xcord+0.5)*xsize), 60, 0.09, 45, stringxycoords,xcoordinate,ycoordinate);
            //printf("X: %d, Y: %d.\n");
         }
      }
      //}
   }


   // this (below) plots the triangular interpolated view. Each point smoothly blends fill between 3 vertex values 2 are real points (edges),
   // and the third a pseudo vertex in the centre of a block (average between 4 surrounding real vertices).
   if (displaymode==INTERPOLATED) {

      for(int i=0; i<ydim-1; i++) {                      // (y coord)
         int yc = ydim-2-i;
         for(int j=0; j<xdim-1; j++) {                     // (x coord)
            int lowerleftvertex=coordinate_manipulate(convert_coord_to_index(j,i));    // if any manupulation of how the data is to be plotted is required, do it
            int upperleftvertex=coordinate_manipulate(convert_coord_to_index(j,i+1));
            int upperrightvertex=coordinate_manipulate(convert_coord_to_index(j+1,i+1));
            int lowerrightvertex=coordinate_manipulate(convert_coord_to_index(j+1,i));
            //float pseudoaverageb = (immediate_data[lowerleftvertex] + immediate_data[upperleftvertex] + immediate_data[upperrightvertex] + immediate_data[lowerrightvertex] )/4.0;
            float pseudoaverage = ( (immediate_data[lowerleftvertex]>(NOTDEFINEDFLOAT+1) ? immediate_data[lowerleftvertex] : 0)
                                    + (immediate_data[upperleftvertex]>(NOTDEFINEDFLOAT+1) ? immediate_data[upperleftvertex] : 0)
                                    + (immediate_data[upperrightvertex]>(NOTDEFINEDFLOAT+1) ? immediate_data[upperrightvertex] : 0)
                                    + (immediate_data[lowerrightvertex]>(NOTDEFINEDFLOAT+1) ? immediate_data[lowerrightvertex] : 0) )/4.0;
            // if data is invalid then take it out of the average
            //printf("Just Added %f vs. Conditional %f:%f\n     ll:%f, ul:%f, ur:%f, lr:%f\n\n",pseudoaverageb, pseudoaverage, immediate_data[lowerleftvertex], immediate_data[upperleftvertex],immediate_data[upperrightvertex],immediate_data[lowerrightvertex]);
            glBegin(GL_TRIANGLE_FAN);
            colour_calculator(pseudoaverage,highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize)+(j*xsize),(windowHeight-windowBorder)-((ysize)+(yc*ysize)));            // pseudo vertex
            //printf("Pseudo X=%f, Y=%f. \n",100.0+(xsize/2.0)+(j*xsize),100.0+(ysize/2.0)+(i*ysize));
            colour_calculator(immediate_data[upperleftvertex],highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize/2.0)+(j*xsize),(windowHeight-windowBorder)-((ysize/2.0)+(yc*ysize)));        // upper left vertex
            //printf("upper left vertex X=%f, Y=%f. \n",100.0+(j*xsize),100.0+(i*ysize));
            colour_calculator(immediate_data[lowerleftvertex],highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize/2.0)+(j*xsize),(windowHeight-windowBorder)-((ysize/2.0)+((yc+1)*ysize)));    // lower left vertex
            //printf("lower left vertex X=%f, Y=%f. \n",100.0+(j*xsize),100.0+((i+1)*ysize));
            colour_calculator(immediate_data[lowerrightvertex],highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize/2.0)+((j+1)*xsize),(windowHeight-windowBorder)-((ysize/2.0)+((yc+1)*ysize)));    // lower right vertex
            //printf("lower right vertex X=%f, Y=%f. \n",100.0+((j+1)*xsize),100.0+((i+1)*ysize));
            colour_calculator(immediate_data[upperrightvertex],highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize/2.0)+((j+1)*xsize),(windowHeight-windowBorder)-((ysize/2.0)+(yc*ysize)));    // upper right vertex
            //printf("upper right vertex X=%f, Y=%f. \n",100.0+((j+1)*xsize),100.0+(i*ysize));
            colour_calculator(immediate_data[upperleftvertex],highwatermark,lowwatermark);
            glVertex2f(windowBorder+(xsize/2.0)+(j*xsize),(windowHeight-windowBorder)-((ysize/2.0)+(yc*ysize)));        // upper left vertex
            glEnd();   // this plots the triangle fan (a 4 triangle grad)
            //}
         }
      }  // we build pseudo points between each 4 points (xsize-1*ysize-1) - average out the values and use this when drawing our triangles

   }



   if ((displaymode==TILED || displaymode==HISTOGRAM) && gridlines!=0) {                    // scrolling modes x scale and labels and gridlines
      uint xsteps=xdim,ysteps=ydim;
      glColor4f(0.8,0.8,0.8,1.0);                             // Grey Colour for Gridlines
      if (displaymode==HISTOGRAM) {
         ysize=windowHeight-(2*windowBorder);
         xsteps=xdim*ydim;
         ysteps=1;
      }
      if(xsize>3.0) {                                            // if not going to completely obscure the data
         for(int xcord=0; xcord<=xsteps; xcord++) {                      // vertical grid lines
            //xsize
            glBegin(GL_LINES);
            glVertex2f(windowBorder+(xcord*xsize), windowBorder);              //bottom
            glVertex2f(windowBorder+(xcord*xsize), windowHeight-windowBorder);     //top
            glEnd();
         }
      }
      if(ysize>3.0) {                                            // if not going to completely obscure the data
         for(int ycord=0; ycord<=ysteps; ycord++) {                          // horizontal grid lines
            glBegin(GL_LINES);
            glVertex2f(windowBorder, windowBorder+(ycord*ysize));                  //left
            glVertex2f(windowWidth-windowBorder-keyWidth, windowBorder+(ycord*ysize));     //right
            glEnd();
         }
      }
   }

   if (DISPLAYXLABELS) {
      if (displaymode==RASTER || displaymode==LINES || displaymode==EEGSTYLE) {
         struct timeval stopwatchus;
         gettimeofday(&stopwatchus,NULL);                    // grab current time
         nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);        // get time now in us

         if (freezedisplay==1) nowtime=freezetime;

         // print some labels (no scrolling as vom inducing
         // if less than 1000s then dec place, else full seconds only
         float minlabelinterval = 100;        // 1 label at least every 100 pixels (so as not so crowded) - should be a function of screen size?
         float interval = -1;            // number of pixels between each label (interval distance)
         float i=0.01;                // interval in seconds being tests to see meets the # of labels required
         float howmanyintervals;            // how many labels using this test value would create
         float maxnumberoflabels=((float)plotWidth/(float)minlabelinterval);    // what is the target maximum number of labels that we will print

         do {
            howmanyintervals=displayWindow/i;                // how many of the intervals are covered on the screen (eg. 0.01 0.1, 1, 10)
            if(howmanyintervals <= maxnumberoflabels) {            // if we are now less than or equal to our target # of labels
               interval=plotWidth/howmanyintervals;
               //printf("Locked, interval tested = %f.  # of labels to print: %d. Pixel Interval Gap:%f... plotWidth:%d.\n",i,int(howmanyintervals),interval,plotWidth);
            }
            if(interval < 0) {
               howmanyintervals=displayWindow/(i*2);                // how many of the intervals are covered on the screen (eg. 0.02 0.2, 2, 20)
               if(howmanyintervals <= maxnumberoflabels) {            // if we are now less than or equal to our target # of labels
                  interval=plotWidth/howmanyintervals;
                  //printf("Locked, interval tested = %f.  # of labels to print: %d. Pixel Interval Gap:%f... plotWidth:%d.\n",i*2,int(howmanyintervals),interval,plotWidth);
               }
            }
            if(interval < 0) {
               howmanyintervals=displayWindow/(i*5);                // how many of the intervals are covered on the screen (eg. 0.05 0.5, 5, 50)
               if(howmanyintervals <= maxnumberoflabels) {            // if we are now less than or equal to our target # of labels
                  interval=plotWidth/howmanyintervals;
                  //printf("Locked, interval tested = %f.  # of labels to print: %d. Pixel Interval Gap:%f... plotWidth:%d.\n",i*5,int(howmanyintervals),interval,plotWidth);
               }
            }

            i*=10;
         } while (interval < 0 && i<=1000);                    // while answer not found, but stopping before infinite division

         if (i>1000) interval = 10000;                        // only print the 1st label

         if (plotWidth>=1) {                            // No labels to print, will cause an overflow
            for(float j=plotWidth; j>=0; j-=(int)interval) {
               if (printlabels && fullscreen==0) {            // titles and labels are only printed if border is big enough
                  // nowtime - starttime;
                  int64_t timelabel = ((nowtime-starttimez)/100000) - ((plotWidth-(int)j)*(10*timeperindex));    // work out how long ago
   
                  glLineWidth(1.0);
                  glColor4f(0.75,0.75,0.75,1.0);            // dull non-distracting grey
                  glBegin(GL_LINES);
                  glVertex2f(windowBorder+(int)j,windowHeight+10-windowBorder); // top - used xsize from earlier so this is why below the main plot.
                  glVertex2f(windowBorder+(int)j,windowBorder-10); // inside
                  glEnd();
                  //printglstroke (windowBorder+j, 60, 0.09, 0, stringtime,10.0);
                  glColor4f(0.0,0.0,0.0,1.0);            // black
                  if ((timelabel*playbackmultiplier)<3600) {                // if over 10000 deciseconds (1000secs) don't print the decimal.
                     char stringtime[]="%3.1f";
                     printgl(windowBorder+(int)j-18, 60,GLUT_BITMAP_HELVETICA_18,stringtime, ((float)timelabel/10.0)*playbackmultiplier);
                  } else {
                     char stringtime[]="%um%u";
                     int64_t mins=(timelabel*playbackmultiplier)/600;
                     int64_t secs=(((int64_t)((float)timelabel*playbackmultiplier))%600)/10;
                     //printf("timelabel*mult: %d. Mins:%d, Secs:%d.\n",(int)(timelabel*playbackmultiplier),(int)mins,(int)secs);
                     printgl(windowBorder+(int)j-18, 60,GLUT_BITMAP_HELVETICA_18,stringtime, (int)mins,(int)secs);
                  }
               }
            }
         }
         //printf("counter=%d, updateline=%d.\n",counter,updateline);
         glPointSize(1.0);
      }
   }



//#endif


   if (DISPLAYKEY) {
      if (fullscreen==0) {                            // only print if not in fullscreen mode
         glColor4f(0.0,0.0,0.0,1.0);                         // Black Text for Labels
         int keybase=windowBorder+(0.20*(windowHeight-windowBorder));    // bottom of the key
         char stringy8[]="%.2f";
         if (PERCENTAGESCALE) {
            char stringy8[]="%.2f%%";
            printgl (windowWidth-55, windowHeight-(1*windowBorder)-5, GLUT_BITMAP_HELVETICA_12, stringy8, 100.0);// Print HighWaterMark Value
            printgl (windowWidth-55, keybase-5, GLUT_BITMAP_HELVETICA_12, stringy8, 0.0);                        // which for percentages is 0-100%
         } else {         
            printgl (windowWidth-55, windowHeight-(1*windowBorder)-5, GLUT_BITMAP_HELVETICA_12, stringy8, highwatermark);// Print HighWaterMark Value
            printgl (windowWidth-55, keybase-5, GLUT_BITMAP_HELVETICA_12, stringy8, lowwatermark);            // Print LowWaterMark Value
         }
         float interval, difference = highwatermark-lowwatermark;
         for (float i=10000; i>=0.1; i/=10.0) if (difference<i) {
            if (difference<(i/2)) interval=i/20.0;
            else interval=i/10.0;
         }
         if (PERCENTAGESCALE) interval=10;  // fixed for percentage viewing
         int multipleprinted=1;
         float linechunkiness = (float)(windowHeight-(windowBorder+keybase))/(float)(highwatermark-lowwatermark);
         if ((windowHeight-windowBorder-keybase)>0) {                // too small to print
            for (uint i=0; i<(windowHeight-windowBorder-keybase); i++) {
               float temperaturehere=1.0;
               if (linechunkiness>0.0) temperaturehere = ((float)i / linechunkiness)+lowwatermark;
               float magnitude = colour_calculator(temperaturehere,highwatermark,lowwatermark);
               glBegin(GL_LINES);
               glVertex2f(windowWidth-65,i+keybase); // rhs
               glVertex2f(windowWidth-65-keyWidth,i+keybase); // lhs
               glEnd();      //draw_line;
               float positiveoffset=temperaturehere-lowwatermark;
               if (PERCENTAGESCALE) positiveoffset = (positiveoffset/difference)*100.0; // scale it to a range of 0-100
               if (positiveoffset>=(interval*multipleprinted)) {
                  glColor4f(0.0,0.0,0.0,1.0);
                  glLineWidth(4.0);
                  glBegin(GL_LINES);
                     glVertex2f(windowWidth-65,i+keybase); // rhs
                     glVertex2f(windowWidth-75,i+keybase); // inside
                     glVertex2f(windowWidth-55-keyWidth,i+keybase); // inside
                     glVertex2f(windowWidth-65-keyWidth,i+keybase); // lhs
                  glEnd();
                  glLineWidth(1.0);
                  printgl (windowWidth-55, i+keybase-5, GLUT_BITMAP_HELVETICA_12, stringy8, lowwatermark+(multipleprinted*interval));
                  // Print labels for key - font? GLUT_BITMAP_8_BY_13
                  multipleprinted++;
               }
               // if need to print a tag - do it
            }

            glColor4f(0.0,0.0,0.0,1.0);
            glLineWidth(2.0);
            glBegin(GL_LINE_LOOP);
            glVertex2f(windowWidth-65-keyWidth,keybase); // bottomleft
            glVertex2f(windowWidth-65,keybase); // bottomright
            glVertex2f(windowWidth-65,windowHeight-(1*windowBorder)); // topright
            glVertex2f(windowWidth-65-keyWidth,windowHeight-(1*windowBorder)); // topleft
            glEnd();      //draw_line loop around the key;
            glLineWidth(1.0);
         } // key is only printed if big enough to print
      }
   }

   if (PLAYPAUSEXIT) {
// for display of visualisation screen controls
      if (fullscreen==0 && windowToUpdate==win1) {        // only print if not in fullscreen mode & the main window
         for (int boxer=0; boxer<3; boxer++) {
            int boxsize=40, gap=10;
            int xorigin = (windowWidth-3*(boxsize+gap)), yorigin=windowHeight-gap-boxsize;
            // local to this scope

            if ( (freezedisplay==0 && boxer==0) || (freezedisplay!=0 && boxer==1) || boxer==2) {
               glColor4f(0.0,0.0,0.0,1.0);                             // black is the new black
               glBegin(GL_QUADS);
               glVertex2f(xorigin+(boxer*(boxsize+gap)),yorigin); //btm left
               glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize,yorigin); //btm right
               glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize,yorigin+boxsize); // top right
               glVertex2f(xorigin+(boxer*(boxsize+gap)),yorigin+boxsize); // top left
               glEnd();
               // now draw shapes on boxes
               if (boxer==2) {
                  glColor4f(1.0,0.0,0.0,1.0);
                  glLineWidth(15.0);
                  glBegin(GL_LINES);
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+boxsize-gap); // topleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize-gap,yorigin+gap); // bottomright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize-gap,yorigin+boxsize-gap); // topright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+gap); // bottomleft
                  glEnd();
                  glLineWidth(1.0);
               }
               if (boxer==0) {
                  glColor4f(1.0,0.0,0.0,1.0);
                  glLineWidth(15.0);
                  glBegin(GL_QUADS);
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+boxsize-gap); // topleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+gap); // bottomleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+((boxsize+gap)/2)-gap,yorigin+gap); // bottomright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+((boxsize+gap)/2)-gap,yorigin+boxsize-gap); // topright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+((boxsize-gap)/2)+gap,yorigin+boxsize-gap); // topleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+((boxsize-gap)/2)+gap,yorigin+gap); // bottomleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize-gap,yorigin+gap); // bottomright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize-gap,yorigin+boxsize-gap); // topright
                  glEnd();
                  glLineWidth(1.0);
               }
               if (boxer==1) {
                  glColor4f(1.0,0.0,0.0,1.0);
                  glLineWidth(15.0);
                  glBegin(GL_TRIANGLES);
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+boxsize-gap); // topleft
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+boxsize-gap,yorigin+(boxsize/2)); // centreright
                  glVertex2f(xorigin+(boxer*(boxsize+gap))+gap,yorigin+gap); // bottomleft
                  glEnd();
                  glLineWidth(1.0);
               }


            }

         }


         if (printpktgone!=0) {
            glColor4f(0.0,0.0,0.0,1.0);
            if (spinnakerboardipset==0) {
               char stringy12[]="Target Unknown";
               printgl ((windowWidth-3*(boxsize+gap))-5, windowHeight-gap-boxsize-25, GLUT_BITMAP_8_BY_13, stringy12);
            } else {
               char stringy12[]="Packet Sent";
               printgl ((windowWidth-3*(boxsize+gap))+5, windowHeight-gap-boxsize-25, GLUT_BITMAP_8_BY_13, stringy12);
            }
         }
      }
   }


   if (SIMULATION==HEATMAP) {
      if (INTERACTION) {
         if (fullscreen==0) {                            // only print if not in fullscreen mode
// for Heat Map work
            for (int boxer=0; boxer<(controlboxes*controlboxes); boxer++) {
               int boxx=boxer/controlboxes, boxy=boxer%controlboxes;
               if(boxx==1 || boxy==1) {
                  glColor4f(0.0,0.0,0.0,1.0);
                  if (boxer==livebox) glColor4f(0.0,1.0,1.0,1.0);
                  char stringy9[]="%3.1f";
                  char stringy10[]="%c";
                  if ( boxer==CENTRE || boxer==WEST || boxer==SOUTH || boxer==NORTH ||boxer==EAST ) { //only plot NESW+centre
                     if (editmode!=0 || boxer==CENTRE) {
                        if (boxer==CENTRE && editmode!=0) glColor4f(0.0,0.6,0.0,1.0);    // go button is green!
                        glBegin(GL_QUADS);
                        glVertex2f(windowWidth-((boxx+1)*(boxsize+gap)),yorigin+(boxy*(boxsize+gap))); //btm left
                        glVertex2f(windowWidth-((boxx+1)*(boxsize+gap))+boxsize,yorigin+(boxy*(boxsize+gap))); //btm right
                        glVertex2f(windowWidth-((boxx+1)*(boxsize+gap))+boxsize,yorigin+boxsize+(boxy*(boxsize+gap))); // top right
                        glVertex2f(windowWidth-((boxx+1)*(boxsize+gap)),yorigin+boxsize+(boxy*(boxsize+gap))); // top left
                        glEnd();  // alter button
                     }
                     if (boxer!=CENTRE) {
                        glColor4f(0.0,0.0,0.0,1.0);
                        if (editmode!=0 && boxer!=livebox) glColor4f(1.0,1.0,1.0,1.0);
                        float currentvalue;
                        if (boxer==NORTH) currentvalue=alternorth;
                        if (boxer==EAST) currentvalue=altereast;
                        if (boxer==SOUTH) currentvalue=altersouth;
                        if (boxer==WEST) currentvalue=alterwest;
                        printgl (windowWidth-((boxx+1)*(boxsize+gap)), yorigin+(boxy*(boxsize+gap))+(boxsize/2)-5, GLUT_BITMAP_8_BY_13, stringy9,currentvalue);
                     } else {
                        glColor4f(1.0,1.0,1.0,1.0);
                        if (editmode==0) {
                           char stringy10[]="Alter";
                           printgl (windowWidth-((boxx+1)*(boxsize+gap)), yorigin+(boxy*(boxsize+gap))+(boxsize/2)-5, GLUT_BITMAP_8_BY_13, stringy10);
                        } else {
                           char stringy11[]=" Go!";
                           printgl (windowWidth-((boxx+1)*(boxsize+gap)), yorigin+(boxy*(boxsize+gap))+(boxsize/2)-5, GLUT_BITMAP_8_BY_13, stringy11);
                        }
                     }
                  }
               }
            }
// for Heat Map work
         }
      }
   }


//    if (displaymode==LINES  && freezedisplay==0) {
   if (displaymode==RASTER || displaymode==LINES || displaymode==EEGSTYLE) {
      struct timeval stopwatchus;
      gettimeofday(&stopwatchus,NULL);                    // grab current time
      nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us

      if (freezedisplay==1) nowtime=freezetime;

      float x_scaling_factor=1;
      float y_scaling_factor=( ((float)windowHeight-(2.0*(float)windowBorder)) / (highwatermark-lowwatermark) );    // value rise per pixel of display

      //printf("Here, with timeperindex:%f, and y_scaling_factor:%f. Display Window = %f fps.\n",timeperindex,y_scaling_factor,displayWindow);

      int updateline=((nowtime-starttimez)/(int64_t)(timeperindex*1000000)) % (HISTORYSIZE);    // which index is being plotted now (on the right hand side)

      if (updateline<0 || updateline>HISTORYSIZE) {
         printf("Error line 2093: Updateline out of bounds: %d. Times - Now:%lld  Start:%lld \n",updateline, (long long int)nowtime, (long long int)starttimez);        // CPDEBUG
      } else {
         int linestoclear = updateline-lasthistorylineupdated;            // work out how many lines have gone past without activity.
         if (linestoclear<0 && (updateline+500)>lasthistorylineupdated) linestoclear=0;        // to cover any underflow when resizing plotting window smaller (wrapping difference will be <500)
         if (linestoclear<0) linestoclear = (updateline+HISTORYSIZE)-lasthistorylineupdated;     // if has wrapped then work out the true value
         int numberofdatapoints=xdim*ydim;
         for (int i=0; i<linestoclear; i++) {
            //printf("%d - %d =  %d (with WindowWidth: %d)\n",updateline, lasthistorylineupdated, linestoclear, windowWidth);
            for (int j=0; j<numberofdatapoints; j++) history_data[(1+i+lasthistorylineupdated)%(HISTORYSIZE)][j]=INITZERO?0.0:NOTDEFINEDFLOAT;  // nullify data in the quiet period
            if (win2) {
               numberofdatapoints=MAXRASTERISEDNEURONS;                // bespoke for Discovery demo
               for (int j=0; j<numberofdatapoints; j++) history_data_set2[(1+i+lasthistorylineupdated)%(HISTORYSIZE)][j]=INITZERO?0.0:NOTDEFINEDFLOAT;  // nullify data in the quiet period
            }
         }
         // Upon Plot screen. All between lastrowupdated and currenttimerow will be nothing - clear between last and to now.  If lastrowupdated = currenttimerow, nothing to nullify.
      }

      int itop1 = updateline-plotWidth;        // final entry to print (needs a max)
      int itop2 = (HISTORYSIZE);                            // begin with assumption no need for any wraparound
      //printf("1) updateline: %i, itop1:%d, itop2:%d\n",updateline, itop1,itop2);
      if (itop1<0) {                                // if final entry has wrapped around array
         itop2 = (HISTORYSIZE)+itop1;                        // 2nd bite adds on extra wraparound data
         if (itop2<0) itop2=0;                                // can we go to x scaling here?  This is a bit coarse.
         itop1 = 0;                                // 1st bite floors at bottom of array
      }
      //printf("2)updateline: %i, itop1:%d, itop2:%d\n",updateline, itop1,itop2);

      glColor4f(0.0,0.0,1.0,1.0);                        // Will plot in blue

      float targetdotsize=(float)(windowHeight-(2*windowBorder))/((float)maxneuridrx);

      //printf("DotSize: %f, maxneuird:%f. \n",targetdotsize,((float)maxneuridrx));

      if (targetdotsize>4) targetdotsize=4.0;
      if (targetdotsize<1) targetdotsize=1.0;
      glPointSize(targetdotsize);

      float workingwithdata;                            // data value being manipulated/studied
      int numberofrasterplots=xdim*ydim;
      if (windowToUpdate==win2) numberofrasterplots=maxneuridrx;        // bespoke for Discovery demo

      if (displaymode==RASTER) {
         uint *spikesperxcoord = new uint[plotWidth];
         for(int j=0; j<plotWidth; j++) spikesperxcoord[j]=0;
         uint maxspikerate = 200;
         glBegin(GL_POINTS); // TODO if targetdotsize>=4 - draw lines?
         for(int j=0; j<numberofrasterplots; j++) {
            int jj=coordinate_manipulate(j);            // if any manipulation of how the data is to be plotted is required, do it
            for(int i=updateline; i>=itop1; i--) {          // For each column of elements to the right / newer than the current line
               workingwithdata=history_data[i][jj];
               if (windowToUpdate==win2) workingwithdata=history_data_set2[i][jj];        // bespoke for Discovery demo
               if (workingwithdata>(NOTDEFINEDFLOAT+1)) {
                  y_scaling_factor=(float)(windowHeight-(2*windowBorder))/(float)(numberofrasterplots);    // how many pixels per neuron ID
                  int y=(int) ((float)(j+0.5)*(float)y_scaling_factor)+windowBorder;
                  int x=(windowWidth-windowBorder-keyWidth)-((updateline-i)*x_scaling_factor);
                  glVertex2f(x,y); // TODO change to lines for low counts? 1 of 2 (targetdotsize).
                  // start at y-(targetdotsize/2) end at y+(targetdotsize/2)
                  spikesperxcoord[x-windowBorder]++;
                  //if (spikesperxcoord[x-windowBorder]>maxspikerate) maxspikerate=spikesperxcoord[x-windowBorder];
                  //printf("i:%u: Data:%f.\n",i,workingwithdata);
               }
            }
            for(int i=(HISTORYSIZE-1); i>itop2; i--) {          // For each column of elements to the right / newer than the current line
               workingwithdata=history_data[i][jj];
               if (windowToUpdate==win2) workingwithdata=history_data_set2[i][jj];        // bespoke for Discovery demo
               if (workingwithdata>(NOTDEFINEDFLOAT+1)) {
                  y_scaling_factor=(float)(windowHeight-(2*windowBorder))/(float)(numberofrasterplots);    // how many pixels per neuron ID
                  int y=(int) ((float)(j+0.5)*(float)y_scaling_factor)+windowBorder;
                  int x=(windowWidth-windowBorder-keyWidth)-((updateline+((HISTORYSIZE)-i))*x_scaling_factor);
                  glVertex2f(x,y); // TODO change to lines for low counts? 2 of 2. (targetdotsize)
                  // start at y-(targetdotsize/2) end at y+(targetdotsize/2)
                  spikesperxcoord[x-windowBorder]++;
                  //if (spikesperxcoord[x-windowBorder]>maxspikerate) maxspikerate=spikesperxcoord[x-windowBorder];
                  //printf("i:%u: Data:%f.\n",i,workingwithdata);
               }
            }

         }
         glEnd();

#ifdef TESTING
         glColor4f(1.0,0.0,0.0,0.2);                        // Will plot histogram of spike rate in red, but supposed to be a bit see-through -not see through!

         for(int j=0; j<plotWidth; j++) {
            //printf("Spike Rate: %i = %u.\n",j,spikesperxcoord[j]);
            glBegin(GL_LINES);
            //glVertex2f(j+windowBorder,windowBorder);
            int y=windowBorder+((windowHeight-(2*windowBorder)) * ((float)spikesperxcoord[j]/(float)maxspikerate));
            //printf("Y coord = %u for value: %u. MaxRate:%d.\n",j,spikesperxcoord[j], maxspikerate);
            //glVertex2f(j+windowBorder,y);
            tempcountcp+=spikesperxcoord[j];
            glEnd();
         }

#endif
         delete spikesperxcoord;    // free stack memory back up again explicitly



      } else if (displaymode==LINES) {
         glLineWidth(2.0);
         for(int j=0; j<numberofrasterplots; j++) {
            int jj=coordinate_manipulate(j);            // if any manipulation of how the data is to be plotted is required, do it
            float magnitude = colour_calculator(immediate_data[jj],highwatermark,lowwatermark);
            glBegin(GL_LINE_STRIP);
            for(int i=updateline; i>=itop1; i--) {          // For each column of elements to the right / newer than the current line
               workingwithdata=INITZERO?0.0:NOTDEFINEDFLOAT;            // default to invalid
               //if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj]/(float)pow(2.0,FIXEDPOINT);
               if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj];
               if (workingwithdata>(NOTDEFINEDFLOAT+1)) {
                  int y=(int)(((float)windowBorder+((workingwithdata-lowwatermark)*y_scaling_factor)));
                  //printf("y:%u: Orig:%f, DataWorkedWith:%f, LowWater=%f\n",i,history_data[i][jj],workingwithdata,lowwatermark);
                  if (y>(windowHeight-windowBorder)) y=(windowHeight-windowBorder);
                  if (y<windowBorder) y=windowBorder;
                  glVertex2f((windowWidth-windowBorder-keyWidth)-((updateline-i)*x_scaling_factor),y);
                  //printf("i:%u: Data:%f.\n",i,history_data[i][j]);
               }
            }
            //glEnd();
            //glBegin(GL_LINE_STRIP);
            for(int i=(HISTORYSIZE-1); i>itop2; i--) {      // For each column of elements to the right / newer than the current line
               workingwithdata=INITZERO?0.0:NOTDEFINEDFLOAT;            // default to invalid
               //if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj]/(float)pow(2.0,FIXEDPOINT);
               if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj];
               if (workingwithdata>(NOTDEFINEDFLOAT+1)) {
                  int y=(int)(((float)windowBorder+((workingwithdata-lowwatermark)*y_scaling_factor)));
                  if (y>(windowHeight-windowBorder)) y=(windowHeight-windowBorder);
                  if (y<windowBorder) y=windowBorder;
                  glVertex2f((windowWidth-windowBorder-keyWidth)-((updateline+((HISTORYSIZE)-i))*x_scaling_factor),y);
                  //printf("i:%u: Data:%f.\n",i,history_data[i][j]);
                  //printf("i: %d, x: %d.\n",i,(windowWidth-windowBorder-keyWidth)-((HISTORYSIZE)-i));
               }
            }
            glEnd();
         }
         glLineWidth(1.0);
      } else if (displaymode==EEGSTYLE) {
         float eegrowheight=(float)( (windowHeight-(2*windowBorder)) - (0*numberofrasterplots) )  /  (float)numberofrasterplots;    // space given over for each eeg-style plot EEGSTYLE difference to LINES:
         y_scaling_factor = eegrowheight/(float)(highwatermark-lowwatermark); // value rise per pixel of eeg-style row strip space EEGSTYLE difference to LINES:

         glLineWidth(2.0);
         for(int j=0; j<numberofrasterplots; j++) {
            int jj=coordinate_manipulate(j);            // if any manipulation of how the data is to be plotted is required, do it
            glColor4f(0.7,0.7,0.7,1.0);
            glLineWidth(1.0);
            glBegin(GL_LINE_STRIP);
            glVertex2f(windowBorder-10,windowBorder+((int)(eegrowheight*(float)j)));
            glVertex2f(windowBorder+plotWidth+10,windowBorder+((int)(eegrowheight*(float)j)));
            glEnd();
            glLineWidth(2.0);
            float magnitude = colour_calculator(immediate_data[jj],highwatermark,lowwatermark);
            glBegin(GL_LINE_STRIP);
            for(int i=updateline; i>=itop1; i--) {              // For each column of elements to the right / newer than the current line
               workingwithdata=INITZERO?0.0:NOTDEFINEDFLOAT;                // default to invalid
               //if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj]/(float)pow(2.0,FIXEDPOINT);
               if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj];
               if (workingwithdata>=lowwatermark) {
                  int y=(int)(((float)windowBorder+((workingwithdata-lowwatermark)*y_scaling_factor)));
                  y+=(int)(eegrowheight*(float)j);        // EEGSTYLE difference to LINES: add on row offset up screen
                  if (y>(windowHeight-windowBorder)) y=(windowHeight-windowBorder);
                  if (y<windowBorder) y=windowBorder;
                  glVertex2f((windowWidth-windowBorder-keyWidth)-((updateline-i)*x_scaling_factor),y);
                  //printf("i:%u: Data:%f.\n",i,history_data[i][j]);
               }
            }
            //glEnd();
            //glBegin(GL_LINE_STRIP);
            for(int i=(HISTORYSIZE-1); i>itop2; i--) {          // For each column of elements to the right / newer than the current line
               workingwithdata=INITZERO?0.0:NOTDEFINEDFLOAT;                // default to invalid
               //if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj]/(float)pow(2.0,FIXEDPOINT);
               if (history_data[i][jj]>(NOTDEFINEDFLOAT+1)) workingwithdata=history_data[i][jj];
               if (workingwithdata>=lowwatermark) {
                  int y=(int)(((float)windowBorder+((workingwithdata-lowwatermark)*y_scaling_factor)));
                  y+=(int)(eegrowheight*(float)j);        // EEGSTYLE difference to LINES: add on row offset up screen
                  if (y>(windowHeight-windowBorder)) y=(windowHeight-windowBorder);
                  if (y<windowBorder) y=windowBorder;
                  glVertex2f((windowWidth-windowBorder-keyWidth)-((updateline+((HISTORYSIZE)-i))*x_scaling_factor),y);
                  //printf("i:%u: Data:%f.\n",i,history_data[i][j]);
                  //printf("i: %d, x: %d.\n",i,(windowWidth-windowBorder-keyWidth)-((HISTORYSIZE)-i));
               }
            }
            glEnd();
         }
         glColor4f(0.7,0.7,0.7,1.0);
         glLineWidth(1.0);
         glBegin(GL_LINE_STRIP);
         glVertex2f(windowBorder-10,windowBorder+((int)(eegrowheight*(float)numberofrasterplots)));
         glVertex2f(windowBorder+plotWidth+10,windowBorder+((int)(eegrowheight*(float)numberofrasterplots)));
         glEnd();
      }


   }

//}


   if (DECAYPROPORTION>0.0) { // CP - if used!
      if(freezedisplay==0) for (int i=0; i<(xdim*ydim); i++) if (immediate_data[i]>(NOTDEFINEDFLOAT+1)) immediate_data[i]*=DECAYPROPORTION;   // puts a decay on the data per frame plotted
   }



#ifdef TESTING
   if (windowToUpdate==win2) {
      //printf("Spikes on Screen this frame = %d.\n", tempcountcp/10);
      tempcountcp=0;
   }

   if ( (abs(rectanglestartx-rectangleendx)>5) && (abs(rectanglestarty-rectangleendy)>5) ) {    // box is to be displayed if larger than 5x5 (user wants to draw a box)
      if (leftbuttondown==0) {    // box is finished
         glColor4f(0.0,1.0,0.0,1.0);
         glBegin(GL_LINE_LOOP);
         glVertex2f(rectanglestartx, rectanglestarty);
         glVertex2f(rectangleendx, rectanglestarty);
         glVertex2f(rectangleendx, rectangleendy);
         glVertex2f(rectanglestartx, rectangleendy);
         glEnd();
      } else {            // drawing in progress
         glColor4f(1.0,0.0,0.0,1.0);
         glBegin(GL_LINE_LOOP);
         glVertex2f(rectanglestartx, rectanglestarty);
         glVertex2f(rectangleendx, rectanglestarty);
         glVertex2f(rectangleendx, rectangleendy);
         glVertex2f(rectanglestartx, rectangleendy);
         glEnd();
      }
   }
#endif


   glutSwapBuffers();             // no flickery gfx
   somethingtoplot=0;            // indicate we have finished plotting
} // display



// called whenever the display window is resized
void reshape(int width, int height)
{
   if (glutGetWindow()==1) {
      windowWidth = width;
      plotWidth=windowWidth-(2*windowBorder)-keyWidth;
      if (fullscreen==1) {
         windowWidth+=keyWidth;
         plotWidth=windowWidth-keyWidth;
      }
      if (windowWidth<(2*windowBorder)+keyWidth) {
         windowWidth=(2*windowBorder)+keyWidth;    // stop the plotting area becoming -ve and crashing
         plotWidth=0;
      }
      windowHeight = height;
   }

   // turn off label printing if too small, and on if larger than this threshold.
   if (plotWidth<=1 || height-(2*windowBorder)<=1) printlabels=0; // turn off label printing (not much data to plot!)
   else printlabels=1;


   //printf("Wid: %d, Hei: %d.\n",width,height);
   glViewport(0, 0, (GLsizei) width, (GLsizei) height); // viewport dimensions
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   // an orthographic projection. Should probably look into OpenGL perspective projections for 3D if that's your thing
   glOrtho(0.0, width, 0.0, height, -50.0, 50.0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   somethingtoplot=1;                // indicate we will need to refresh the screen

} // reshape


// Called when arrow keys (and some others) are pressed
void specialDown(int key, int x, int y)
{
   if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
      if (INTERACTION) {
         int xc, yc;
         convert_index_to_coord(livebox, &xc,&yc);
         switch(key)
         {
         case GLUT_KEY_UP:
            if(displaymode==HISTOGRAM) {
               //if (livebox==-1) livebox = (ydim-1)*xdim; else livebox=-1;
               if (livebox==-1) livebox = convert_coord_to_index(0,0);
               else livebox=-1;
            } else {
               //if (livebox==-1) livebox = (ydim-1)*xdim; else livebox-=xdim;
               if (livebox==-1) livebox = convert_coord_to_index(0,0);
               else {
                  convert_index_to_coord(livebox, &xc,&yc);
                  if (yc+1>=ydim) livebox=-1;
                  else livebox = convert_coord_to_index(xc, yc+1);
               }
            }
            break;
            needtorebuildmenu=1;
         case GLUT_KEY_DOWN:
            if(displaymode==HISTOGRAM) {
               //if (livebox==-1) livebox = (ydim-1)*xdim; else livebox=-1;
               if (livebox==-1) livebox = convert_coord_to_index(xdim-1,ydim-1);
               else livebox=-1;
            } else {
               if (livebox==-1) livebox = convert_coord_to_index(0,ydim-1);
               else {
                  if (yc-1<0) livebox=-1;
                  else livebox = convert_coord_to_index(xc, yc-1);
               }
            }
            break;
            needtorebuildmenu=1;;
         case GLUT_KEY_RIGHT:
            if(displaymode==HISTOGRAM) {
               if (livebox==-1) livebox = convert_coord_to_index(0,0);
               else if(livebox+1<(xdim*ydim)) livebox++;
               else livebox=-1;
            } else {
               if (livebox==-1) livebox = convert_coord_to_index(0,0);
               else if (xc<(xdim-1)) livebox = convert_coord_to_index(++xc,yc);
               else livebox=-1;
            }
            break;
            needtorebuildmenu=1;;
         case GLUT_KEY_LEFT:
            if(displaymode==HISTOGRAM) {
               if (livebox==-1) livebox = convert_coord_to_index(xdim-1,ydim-1);
               else if(livebox>0) livebox--;
               else livebox = -1;
            } else {
               if (livebox==-1) livebox = convert_coord_to_index(xdim-1,0);
               else if (xc>0) livebox = convert_coord_to_index(--xc,yc);
               else livebox=-1;
            }
            break;
            needtorebuildmenu=1;;
         }
         if (livebox<0 || livebox>=(xdim*ydim)) livebox =-1;
         if (livebox!=-1) biasediting=biascurrent[coordinate_manipulate(livebox)];    // **swopxy**
         somethingtoplot=1;                // indicate we will need to refresh the screen

      }
   }
// call with: convert_index_to_coord(index, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)
// call with: index = convert_coord_to_index(xcoordinate, ycoordinate);


}

// Called when arrow keys (and some others) are released
void specialUp(int key, int x, int y)
{
   if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
      if (INTERACTION) {
         switch(key)
         {
         case GLUT_KEY_UP:
            break;
         case GLUT_KEY_DOWN:
            break;
         case GLUT_KEY_RIGHT:
            break;
         case GLUT_KEY_LEFT:
            break;
         }

      }
   }
}

// Called when keys are pressed
void keyDown (unsigned char key, int x, int y)
{
   switch(tolower(key))
   {

   case 'f':
      if (fullscreen==1) {
         fullscreen=0;                // turn off full screen mode
         windowBorder=oldwindowBorder;        // restore old bordersize
         windowWidth-=keyWidth;            // recover the key area
         plotWidth=windowWidth-(2*windowBorder)-keyWidth;
         break;
      } else {
         fullscreen=1;
         oldwindowBorder=windowBorder;        // used as border disappears when going full-screen
         windowBorder=0;                // no borders around the plots
         windowWidth+=keyWidth;            // take over the area used for the key too
         plotWidth=windowWidth-keyWidth;
         break;
      }

   case 'b':
      if (gridlines==1) {
         gridlines=0;            // if sQuare gridlines are on then toggle off
         break;
      } else {
         gridlines=1;            // if sQuare gridlines are off then toggle on
         break;
      }

      // number keys used to select colour maps
   case '1':
      colourused=1;   // sets the colour when a numeric key is pressed
      break;
   case '2':
      colourused=2;   // sets the colour when a numeric key is pressed
      break;
   case '3':
      colourused=3;   // sets the colour when a numeric key is pressed
      break;
   case '4':
      colourused=4;   // sets the colour when a numeric key is pressed
      break;
   case '5':
      colourused=5;   // sets the colour when a numeric key is pressed
      break;
   case '6':
      colourused=6;   // sets the colour when a numeric key is pressed
      break;
   case '7':
      colourused=7;   // sets the colour when a numeric key is pressed
      break;
   case '8':
      colourused=8;   // sets the colour when a numeric key is pressed
      break;


   case 'c':
      cleardown();   // clears the output when 'c' key is pressed
      break;
   case 'i':
      displaymode=INTERPOLATED;    // switch display mode to interpolated mode when the 'i' key is pressed
      needtorebuildmenu=1;
      break;
   case 'h':
      displaymode=HISTOGRAM;           // switch display mode to histogram mode when the 'h' key is pressed
      needtorebuildmenu=1;
      break;
   case 't':
      displaymode=TILED;           // switch display mode to tiled mode when the 't' key is pressed
      needtorebuildmenu=1;
      break;
   case 'l':
      displaymode=LINES;           // switch display mode to meandering worm-type lines
      needtorebuildmenu=1;
      break;
   case 'r':
      displaymode=RASTER;           // switch display mode to raster plot mode
      needtorebuildmenu=1;
      break;
   case 'm':
      displaymode=EEGSTYLE;           // switch display mode to raster plot mode
      needtorebuildmenu=1;
      break;
   case 'q':
      safelyshut();
      break;
   case '"':
   {  int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below. PAUSE
      //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
      sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 2, 0,0,0,4, 0, 0, 0, 0);
      // send pause packet out
      freezedisplay=1;
      struct timeval stopwatchus;
      gettimeofday(&stopwatchus,NULL);                    // grab current time
      freezetime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us
      needtorebuildmenu=1;;
      break;
   }
   case 'p':
   {  int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below. RESUME
      //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
      sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 3, 0,0,0,4, 0, 0, 0, 0);
      // send resume/restart packet out
      freezedisplay=0;
      needtorebuildmenu=1;;
      break;
   }
   case '#':
      if (plotvaluesinblocks==0) {
         plotvaluesinblocks = 1;
      } else {
         plotvaluesinblocks = 0;
      }       // toggles the plotting of values when the hash '#' key is pressed
      break;

   case 'd':                            // 90 degree rotate
      if (rotateflip==0) rotateflip=1;
      else rotateflip=0;
      //printf("Rotateflip: %d",rotateflip);
      break;
   case 'v':                            // vector swap
      if (vectorflip==0) vectorflip=1;
      else vectorflip=0;
      //printf("Vflip: %d",vectorflip);
      break;
   case 'x':                            // x flip
      if (xflip==0) xflip=1;
      else xflip=0;
      //printf("xflip: %d",xflip);
      break;
   case 'y':                            // y flip
      if (yflip==0) yflip=1;
      else yflip=0;
      //printf("yflip: %d",yflip);
      break;


      if (displaymode==RASTER || displaymode==LINES || ((SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) && rasterpopulation!=-1)) {
         // only if a scrolling mode in operation on the main plot!  (or (surrounded by ifdefs) its a 2ndary rasterised plot)
      case '>':
         displayWindow+=0.1;
         if (displayWindow>100) displayWindow=100;
         break;
      case '<':
         displayWindow-=0.1;
         if (displayWindow<0.1) displayWindow=0.1;
         break;
      }

      if (INTERACTION) {
      case '+':
         if (SIMULATION==CHIPTEMP || SIMULATION==CPUUTIL) {
            for (int i=0; i<(XDIMENSIONS/EACHCHIPX)*(YDIMENSIONS/EACHCHIPY); i++) {
               sdp_sender((256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ),17+(3<<5),2,0,0,0,0);    // send message to increase (2) CPU utilisation
               printf("%d=(%d,%d), ",i,(i/(XDIMENSIONS/EACHCHIPX)),(i%(YDIMENSIONS/EACHCHIPY)));
            }
         } else if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
            if (livebox!=-1) {
               short subcorepopid=0;
               biasediting+=ALTERSTEPSIZE;
               if (biasediting>100.0) biasediting=10.0;
               //printf("SCROLLUP LiveBox:%d   BiasCurrent:%f\n",livebox, biasediting);
               int popid=(coordinate_manipulate(livebox)%(EACHCHIPY*EACHCHIPX))+1;
               short coreid=popid;
               int chipx=((coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) / (XDIMENSIONS/EACHCHIPX))*256;
               int chipy= (coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) % (XDIMENSIONS/EACHCHIPX);
               if (BITSOFPOPID>0) {
                  subcorepopid=(popid-1)&(int)(pow(2,BITSOFPOPID)-1);        // work out the proto population.
                  coreid=1+((popid-1)>>BITSOFPOPID);                // work out the coreID that this population is on.
               }
               //printf("BIASUP LiveBox:%d   BiasCurrent:%f, PopID: %d. CoreID: %d, SubProt:%d\n",livebox, biasediting,popid,coreid,subcorepopid);
               sdp_sender(chipx+chipy, 0x80+coreid,259,subcorepopid,0,(int)(biasediting*256.0),0);    // send message to alter bias current (arg3) (+0x80 = port 4)
               somethingtoplot=1;                        // refresh screen
            }
         } else if (SIMULATION==HEATMAP) {
            if (livebox==NORTH) alternorth+=ALTERSTEPSIZE;
            if (livebox==EAST) altereast+=ALTERSTEPSIZE;
            if (livebox==SOUTH) altersouth+=ALTERSTEPSIZE;
            if (livebox==WEST) alterwest+=ALTERSTEPSIZE;
            //printf("SCROLLUP LiveBox:%d   N:%f, E:%f, S:%f, W:%f\n",livebox,alternorth, altereast, altersouth, alterwest);
         }
         break;
      case '-':
         if (SIMULATION==CHIPTEMP || SIMULATION==CPUUTIL) {
            for (int i=0; i<(XDIMENSIONS/EACHCHIPX)*(YDIMENSIONS/EACHCHIPY); i++)
               sdp_sender((256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ),17+(3<<5),1,0,0,0,0);    // send message to decrease (1) CPU utilisation
         } else if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
            if (livebox!=-1) {
               short subcorepopid=0;
               biasediting-=ALTERSTEPSIZE;
               if (biasediting<-10.0) biasediting=-10.0;
               //printf("SCROLLUP LiveBox:%d   BiasCurrent:%f\n",livebox, biasediting);
               int popid=(coordinate_manipulate(livebox)%(EACHCHIPY*EACHCHIPX))+1;
               short coreid=popid;
               int chipx=((coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) / (XDIMENSIONS/EACHCHIPX))*256;
               int chipy= (coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) % (XDIMENSIONS/EACHCHIPX);
               if (BITSOFPOPID>0) {
                  subcorepopid=(popid-1)&(int)(pow(2,BITSOFPOPID)-1);        // work out the proto population.
                  coreid=1+((popid-1)>>BITSOFPOPID);                // work out the coreID that this population is on.
               }
               //printf("BIASDOWN LiveBox:%d   BiasCurrent:%f, PopID: %d. CoreID: %d, SubProt:%d\n",livebox, biasediting,popid,coreid,subcorepopid);
               sdp_sender(chipx+chipy, 0x80+coreid,259,subcorepopid,0,(int)(biasediting*256.0),0);    // send message to alter bias current (arg3) (+0x80 = port 4)
               somethingtoplot=1;                        // refresh screen
            }
         } else if (SIMULATION==HEATMAP) {
            // if scroll down, and livebox!=0, decrement variable
            if (livebox==NORTH) alternorth-=ALTERSTEPSIZE;
            if (livebox==EAST) altereast-=ALTERSTEPSIZE;
            if (livebox==SOUTH) altersouth-=ALTERSTEPSIZE;
            if (livebox==WEST) alterwest-=ALTERSTEPSIZE;
            //printf("SCROLLDWN LiveBox:%d   N:%f, E:%f, S:%f, W:%f\n",livebox,alternorth, altereast, altersouth, alterwest);
         }
         break;
      }


      if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) {
         if (INTERACTION) {
         case 'z':    // Call for Raster Data
            if (livebox!=-1 && (rasterpopulation==-1 || livebox!=rasterpopulation)) {
               printf("Just called up for a raster from population: %d\n",livebox+1);
               for(int i=0; i<(xdim*ydim); i++) {
                  sdp_sender(0, 0x80+i+1,258,0,4,0,0);  // turn off raster on all populations
                  int rubbish=0;
                  for(int j=0; j<10000000; j++) rubbish++;
                  //sleep(1);
                  printf("%d: ",i);
               }
               if (win2==0) create_new_window();           // if window doesn't already exist then create it on demand
               sdp_sender(0, 0x80+livebox+1,258,1,4,0,0);    // send message to turn on bit 4 (raster on)
               rasterpopulation=livebox;
               //livebox=-1;
               somethingtoplot=1;
               needtorebuildmenu=1;
            } else {
               printf("Just sent messages to cancel all rasters, while on box: %d\n",livebox);
               rasterpopulation = -1;
               for(int i=0; i<(xdim*ydim); i++) {
                  sdp_sender(0, 0x80+i+1,258,0,4,0,0);      // turn off raster on all populations
                  int rubbish=0;
                  for(int j=0; j<10000000; j++) rubbish++;  //
                  //sleep(1);
                  printf("%d, ",i);
               }
               printf("\n");
               if (win2!=0) destroy_new_window();      // if spawned window does exist then close it
               //livebox=-1;
               somethingtoplot=1;
               needtorebuildmenu=1;
            }
            needtorebuildmenu=1;
            break;

         }
      }

      if (SIMULATION==HEATMAP) {
         if (INTERACTION) {
            // for Heat Map work
         case 'n':
            if (editmode!=0) {
               if (livebox==NORTH) {
                  livebox =-1;
               }
               else {
                  livebox=NORTH;
               }
            }
            break;
         case 'e':
            if (editmode!=0) {
               if (livebox==EAST) {
                  livebox =-1;
               }
               else {
                  livebox=EAST;
               }
            }
            break;
         case 's':
            if (editmode!=0) {
               if (livebox==SOUTH) {
                  livebox =-1;
               }
               else {
                  livebox=SOUTH;
               }
            }
            break;
         case 'w':
            if (editmode!=0) {
               if (livebox==WEST) {
                  livebox =-1;
               }
               else {
                  livebox=WEST;
               }
            }
            break;
         case 'a':
            if (editmode==0) {
               editmode=1;
               livebox=-1;
            }
            break;
         case 'g':
            if (editmode!=0) {
#ifdef LOCKINGSEND
               editmode=0; // set to 0 for locking sends
#endif
               livebox=-1;
               int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below.
               //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
               sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21
                           , 1, 0,0,0,4, (int)(alternorth*65536), (int)(altereast*65536), (int)(altersouth*65536), (int)(alterwest*65536));    // send temperature packet out
            }
            break;
         case '9':                // special case to randomise the heatmap
         {  int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below.
            //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
            alternorth=(rand()%(int)(highwatermark-lowwatermark))+lowwatermark;
            altereast=(rand() % (int)(highwatermark-lowwatermark))+lowwatermark;
            altersouth=(rand() % (int)(highwatermark-lowwatermark))+lowwatermark;
            alterwest=(rand() % (int)(highwatermark-lowwatermark))+lowwatermark;
            sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21
                        , 1, 0,0,0,4, (int)(alternorth*65536), (int)(altereast*65536), (int)(altersouth*65536), (int)(alterwest*65536) );    // send temperature packet out (reset to zero).
         }
         break;
         case '0':                // special case to zero the heatmap
            livebox=-1;
            int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below.
            //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
            if (alternorth<1.0 && altereast<1.0 && altersouth<1.0 && alterwest<1.0) {                // if very low -reinitialise
               alternorth=40.0;
               altereast=10.0;
               altersouth=10.0;
               alterwest=40.0;
               sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21
                           , 1, 0,0,0,4, (int)(alternorth*65536), (int)(altereast*65536), (int)(altersouth*65536), (int)(alterwest*65536) );    // send temperature packet out (reset to zero).
            } else {                                                // else reset to zero
               alternorth=0.0;
               altereast=0.0;
               altersouth=0.0;
               alterwest=0.0;
               sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21
                           , 1, 0,0,0,4, (int)(0), (int)(0), (int)(0), (int)(0));    // send temperature packet out (reset to zero).
            }
            break;

            // for Heat Map work
         }
      }
   }
   somethingtoplot=1;                // indicate we will need to refresh the screen
}


// Called when keys are released
void keyUp (unsigned char key, int x, int y)
{
   switch(tolower(key))
   {
   case 'f':
      break;
   case 'b':
      break;

   case '1':
      break;
   case '2':
      break;
   case '3':
      break;
   case '4':
      break;
   case '5':
      break;
   case '6':
      break;
   case '7':
      break;
   case '8':
      break;

   case 'c':
      break;
   case 'i':
      break;
   case 'h':
      break;
   case 't':
      break;
   case 'l':
      break;
   case 'r':
      break;
   case 'm':
      break;
   case 'q':
      break;
   case '"':
      break;
   case 'p':
      break;
   case '#':
      break;
   case 'x':
      break;
   case 'y':
      break;
   case 'v':
      break;
   case 'd':
      break;

      if (INTERACTION) {
         if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY||SIMULATION==HEATMAP) { // cp was ifdef
         case '+':
            break;
         case '-':
            break;
         }

         if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) { // cp was ifdef
         case 'z':
            break;
         }


         if (SIMULATION==HEATMAP)  // cp was ifdef
            // for Heat Map work
         case 'n':
         break;
      case 'e':
         break;
      case 's':
         break;
      case 'w':
         break;
      case 'a':
         break;
      case 'g':
         break;
      case '9':
         break;
      case '0':
         break;
         // for Heat Map work
      }
   }
}


// called when something happs with the moosie
void mousehandler(int button, int state, int x, int y)
{
   if(state==GLUT_DOWN && button==GLUT_LEFT_BUTTON) {
      for (int boxer=0; boxer<3; boxer++) {
         int boxsize=40, gap=10;
         int xorigin = (windowWidth-3*(boxsize+gap)), yorigin=windowHeight-gap-boxsize;
         // local to this scope

         if ( (freezedisplay==0 && boxer==0) || (freezedisplay!=0 && boxer==1) || boxer==2) {
            int match=0;
            // look to see which box we are in,

            //printf("Clicked Box %d, at %d,%d\n",boxer,x,windowHeight-y);
            //printf("Xmin= %d, Xmax = %d, Ymin=%d, Ymax=%d\n",xorigin+(boxer*(boxsize+gap)),(xorigin+(boxer*(boxsize+gap))+boxsize),yorigin+boxsize,yorigin);

            if( x>=(xorigin+(boxer*(boxsize+gap))) && x<(xorigin+(boxer*(boxsize+gap))+boxsize)
                  && ((windowHeight-y)<yorigin+boxsize) && ((windowHeight-y)>=yorigin) ) {
               match=1; // in this box, mark it so
               //printf("Matched %d\n",boxer);
               if (boxer==2) {
                  safelyshut();
               } else if (boxer==0) {
                  int i=0;    // only sending to 0,0  - if all chips, uncomment line below.
                  //for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
                  sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 2, 0,0,0,4, 0, 0, 0, 0);
                  // send pause packet out
                  freezedisplay=1;
                  struct timeval stopwatchus;
                  gettimeofday(&stopwatchus,NULL);                    // grab current time
                  freezetime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us
                  somethingtoplot=1;                // indicate we will need to refresh the screen
                  needtorebuildmenu=1;
               } else if (boxer==1) {
                  int i=0;    // only sending to 0,0  - if all chips, uncomment line below.
                  //for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
                  sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 3, 0,0,0,4, 0, 0, 0, 0);
                  // send resume/restart packet out
                  freezedisplay=0;
                  somethingtoplot=1;                // indicate we will need to refresh the screen
                  needtorebuildmenu=1;
               }
            }
         }
      }
   }


   if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) { 
      if (INTERACTION) {

         float xsize = ((float)(plotWidth)/xdim);                 // size of the tiles in the x dimension
         float ysize = ((float)(windowHeight-(2*windowBorder))/ydim);        // size of the tiles in the y dimension
         // section for mini display selection detection (on all display options)
         if (state==GLUT_DOWN && button==GLUT_LEFT_BUTTON && fullscreen==0) {    // print mini version unless fullscreen
            if (xsize<1.0) xsize=1.0;                    // small pixels can't be less than a pixel wide or they're invisible
            float tileratio=xsize/ysize;                    // the aspect ratio of the mini plot changes with the aspect ratio of the main screen

            for(int i=0; i<(xdim*ydim); i++) {
               //int xcord = i % xdim;                        // X coord is Least significant
               //int ycord = ydim - 1 -(i / xdim);                   // inverted y coords (most signif)
               int xcord,ycord;
               convert_index_to_coord(i, &xcord, &ycord);              // (where xcoordinate and ycoordinate are ints)

               float ysize=max((float)1.0,(float)(windowBorder-(6*gap))/(float)ydim);    // minplot is the height of the border - 6*gapsizes. Plit between the # of y coords
               float xsize=max((float)1.0,ysize*tileratio);                    // draw little tiled version in btm left - pixel size (can't be smaller than zero).
               int xmin=(int)((float)(2*gap)+((float)xcord*xsize));
               int xmax=(int)((float)(2*gap)+(((float)xcord+1)*xsize));
               int ymax=windowHeight-(int)((float)(2*gap)+((float)ycord*ysize));    // y coords for screen and mouse are inverted
               int ymin=windowHeight-(int)((float)(2*gap)+(((float)ycord+1)*ysize));
               //printf("X:%d - L:%u and R:%u.  Y:%d - B:%u and T:%u\n",x,xmin,xmax,y,ymin,ymax );
               if ( x>=xmin && x<xmax && y>=ymin && y<ymax) {
                  if (livebox==i) livebox=-1;
                  else livebox=i;            // toggle on off same box
                  biasediting=biascurrent[coordinate_manipulate(i)];        // **swopxy** the value we are going to be altering
                  somethingtoplot=1;                        // need to refresh the screen
                  needtorebuildmenu=1;                            // context has changed so the menu probably will change and needs rebuilding
                  //printf("Hello, X: %d, Y: %d, livebox is now: %d, and biascurrent is: %f\n",xcord,ycord,livebox, biasediting);
               }
            }
         }

         xsize = ((float)(plotWidth)/xdim);                 // reset size of the tiles in the x dimension
         ysize = ((float)(windowHeight-(2*windowBorder))/ydim);        // reset size of the tiles in the y dimension

         if (state==GLUT_DOWN && button==GLUT_LEFT_BUTTON && (displaymode==TILED||displaymode==HISTOGRAM)) {// if we are in a tiled mode we can make a selection
            if (displaymode==HISTOGRAM) {
               xsize = ((float)(plotWidth)/(xdim*ydim));    // Histogram means all tiles are across X axis
               ysize = ((float)(windowHeight-(2*windowBorder)));            // Histogram means height of block adjusts is height of the plottable area
            }
            for(int i=0; i<(xdim*ydim); i++) {
               int match=0;
               //int xcord = i % xdim;                        // X coord is Least significant
               //int ycord = ydim - 1 -(i / xdim);                   // inverted y coords (most signif)
               int xcord,ycord;
               convert_index_to_coord(i, &xcord, &ycord);              // (where xcoordinate and ycoordinate are ints)

               if (displaymode==HISTOGRAM) {
                  //xcord = (i%xdim)+((xdim*(ydim-1))-ydim*(i/xdim));    // for histogram is just the data index, reorder into btmleft, btmright, ... topleft, topright coords
                  xcord = i;                        // for histogram is just the data index, reorder into btmleft, btmright, ... topleft, topright coords
                  ycord = 0;                           // for histogram always at origin
               }
               if( (x>=(windowBorder+(xcord*xsize))) && ((windowHeight-y)>=windowBorder+(ycord*ysize)) &&
                     (x<windowBorder+((xcord+1)*xsize)) && ((windowHeight-y)<windowBorder+((ycord+1)*ysize)) )
                  match=1; // in this box, mark it so

               if (match==1) {
                  if (livebox==i) livebox=-1;
                  else livebox=i;
                  biasediting=biascurrent[coordinate_manipulate(i)];                // **swopxy**
                  somethingtoplot=1;                    // refresh screen
                  needtorebuildmenu=1;
                  //printf("Hello, X: %d, Y: %d, livebox is now: %d, and biascurrent is: %f\n",xcord,ycord,livebox, biasediting);
               }
            }
         }

         else if(state==GLUT_DOWN && button==3 && livebox!=-1) {
            // if scroll up, and box is selected, increment variable
            if (livebox!=-1) {
               short subcorepopid=0;
               biasediting+=ALTERSTEPSIZE;
               if (biasediting>100.0) biasediting=10.0;
               //printf("SCROLLUP LiveBox:%d   BiasCurrent:%f\n",livebox, biasediting);
               int popid=(coordinate_manipulate(livebox)%(EACHCHIPY*EACHCHIPX))+1;
               short coreid=popid;
               int chipx=((coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) / (XDIMENSIONS/EACHCHIPX))*256;
               int chipy= (coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) % (XDIMENSIONS/EACHCHIPX);
               if (BITSOFPOPID>0) {
                  subcorepopid=(popid-1)&(int)(pow(2,BITSOFPOPID)-1);        // work out the proto population.
                  coreid=1+((popid-1)>>BITSOFPOPID);                // work out the coreID that this population is on.
               }
               //printf("BIASUP LiveBox:%d   BiasCurrent:%f, PopID: %d. CoreID: %d, SubProt:%d\n",livebox, biasediting,popid,coreid,subcorepopid);
               sdp_sender(chipx+chipy, 0x80+coreid,259,subcorepopid,0,(int)(biasediting*256.0),0);    // send message to alter bias current (arg3) (+0x80 = port 4)
               somethingtoplot=1;                        // refresh screen
            }
         } else if(state==GLUT_DOWN && button==4 && livebox!=-1) {
            // if scroll down, and box is selected, decrement variable
            if (livebox!=-1) {
               short subcorepopid=0;
               biasediting-=ALTERSTEPSIZE;
               if (biasediting<-10.0) biasediting=-10.0;
               //printf("SCROLLUP LiveBox:%d   BiasCurrent:%f\n",livebox, biasediting);
               int popid=(coordinate_manipulate(livebox)%(EACHCHIPY*EACHCHIPX))+1;
               short coreid=popid;
               int chipx=((coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) / (XDIMENSIONS/EACHCHIPX))*256;
               int chipy= (coordinate_manipulate(livebox)/(EACHCHIPY*EACHCHIPX)) % (XDIMENSIONS/EACHCHIPX);
               if (BITSOFPOPID>0) {
                  subcorepopid=(popid-1)&(int)(pow(2,BITSOFPOPID)-1);        // work out the proto population.
                  coreid=1+((popid-1)>>BITSOFPOPID);                // work out the coreID that this population is on.
               }
               //printf("BIASDOWN LiveBox:%d   BiasCurrent:%f, PopID: %d. CoreID: %d, SubProt:%d\n",livebox, biasediting,popid,coreid,subcorepopid);
               sdp_sender(chipx+chipy, 0x80+coreid,259,subcorepopid,0,(int)(biasediting*256.0),0);    // send message to alter bias current (arg3) (+0x80 = port 4)
               somethingtoplot=1;
            }
         }
      }
   }


   if (SIMULATION==HEATMAP) {
      if (INTERACTION) {
//    printf("x=%d, y=%d, button=%d, updown=%d\n",x,y,button, state);
//    if(state==GLUT_DOWN && button==GLUT_LEFT_BUTTON) printf("Left Down");

         if(state==GLUT_DOWN && button==GLUT_LEFT_BUTTON) {

            for (int boxer=0; boxer<(controlboxes*controlboxes); boxer++) {
               int boxx=boxer%controlboxes;
               int boxy=boxer/controlboxes;
               int match=0;
               if( (x>=(windowWidth-((boxx+1)*(boxsize+gap)))) && ((windowHeight-y)>=(yorigin+(boxy*(boxsize+gap)))) &&
                     (x<(windowWidth-((boxx+1)*(boxsize+gap))+boxsize)) && ((windowHeight-y)<(yorigin+boxsize+(boxy*(boxsize+gap)))) )
                  match=1; // in this box, mark it so

               if (match==1) {
                  int selectedbox = (boxx*controlboxes)+boxy;
                  if (editmode==0) {
                     if (selectedbox==CENTRE) {
                        // if editmode==0 then if box ==4 editmode=1, livebox =0, calculate side values to edit;
                        editmode =1;
                        livebox =-1;
                        somethingtoplot=1;                // indicate we will need to refresh the screen
                     }
                  } else {
                     if (selectedbox==CENTRE) {
                        // if editmode==1 then if box ==4 editmode=0, send command;
#ifdef LOCKINGSEND
                        editmode = 0;
#endif
                        livebox =-1;
                        int i=0;    // only sending to 0,0  - if all chips, uncomment line below.
                        //for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
                        sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21
                                    , 1, 0,0,0,4, (int)(alternorth*65536), (int)(altereast*65536), (int)(altersouth*65536), (int)(alterwest*65536));    // send temp pack
                        somethingtoplot=1;            // indicate we will need to refresh the screen
                     } else if (selectedbox==WEST || selectedbox==SOUTH || selectedbox==NORTH ||selectedbox==EAST) { //NESW
                        if (selectedbox==livebox) {
                           // if editmode==1 and box==livebox livebox=0
                           livebox=-1;
                           somethingtoplot=1;            // indicate we will need to refresh the screen

                        } else {
                           // if editmode==1 and box!=livebox livebox=box
                           livebox=selectedbox;
                           somethingtoplot=1;            // indicate we will need to refresh the screen
                        }
                     }
                  }
                  //printf("In Box %d, EditMode:%d, LiveBox:%d\n",selectedbox,editmode,livebox);
               }
            }

            // now check for play, pause and exit buttons.
            // if freezedisplay==0 then display pause button, else display play button.


         } else if(state==GLUT_DOWN && button==3 && livebox!=-1) {
            // if scroll up, and livebox!=0, increment variable
            if (livebox==NORTH) alternorth+=ALTERSTEPSIZE;
            if (livebox==EAST) altereast+=ALTERSTEPSIZE;
            if (livebox==SOUTH) altersouth+=ALTERSTEPSIZE;
            if (livebox==WEST) alterwest+=ALTERSTEPSIZE;
            //printf("SCROLLUP LiveBox:%d   N:%f, E:%f, S:%f, W:%f\n",livebox,alternorth, altereast, altersouth, alterwest);
            somethingtoplot=1;                // indicate we will need to refresh the screen

         } else if(state==GLUT_DOWN && button==4 && livebox!=-1) {
            // if scroll down, and livebox!=0, decrement variable
            if (livebox==NORTH) alternorth-=ALTERSTEPSIZE;
            if (livebox==EAST) altereast-=ALTERSTEPSIZE;
            if (livebox==SOUTH) altersouth-=ALTERSTEPSIZE;
            if (livebox==WEST) alterwest-=ALTERSTEPSIZE;
            //printf("SCROLLDWN LiveBox:%d   N:%f, E:%f, S:%f, W:%f\n",livebox,alternorth, altereast, altersouth, alterwest);
            somethingtoplot=1;                // indicate we will need to refresh the screen

         } else {
            //anything else you fancy
         }
      }
   }


   if (state==GLUT_DOWN && button==GLUT_LEFT_BUTTON && somethingtoplot==0)    {
      livebox=-1;            // if you didn't manage to do something useful, then likely greyspace around the figure was clicked (should now deselect any selection)
      somethingtoplot=1;
      rebuildmenu();
   }


#ifdef TESTING

   if (state==GLUT_DOWN && button==GLUT_LEFT_BUTTON)    {
      if (leftbuttondown==0) {
         rectanglestartx=x;
         rectanglestarty=windowHeight-y;
         rectangleendx=x;
         rectangleendy=y;
      }
      leftbuttondown=1;
   }

   if (state==GLUT_UP && button==GLUT_LEFT_BUTTON)    {
      if (leftbuttondown==1) {
         rectangleendx=x;    // current coords with after button released
         rectangleendy=windowHeight-y;    // current coords with after button released
         if ( (abs(rectanglestartx-rectangleendx)<100) || (abs(rectanglestarty-rectangleendy)<100) ) {
            rectanglestartx = -1;    // if too small (i.e. less than 100x100 pixels), reset
            rectanglestarty = -1;
            rectangleendx = -1;
            rectangleendy = -1;
         }
      }
      leftbuttondown=0;
   }
   somethingtoplot=1;

#endif
}

#ifdef TESTING
void MouseMotion(int x, int y)
{
   if (leftbuttondown!=0) {
      rectangleendx=x;        // current coords with LH mouse button still held down
      rectangleendy=windowHeight-y;    // current coords with mouse button still held down
   }
}
#endif


// Called repeatedly, once per OpenGL loop
void idleFunction()
{
   if (needtorebuildmenu==1 && menuopen == 0) {
      filemenu();
      rebuildmenu();    // if menu is not open we can make changes
      needtorebuildmenu=0;
   }

   int usecperframe = (1000000/MAXFRAMERATE);              // us target per frame
   struct timeval stopwatchus, deltateeus;                // declare timing variables
   struct timespec ts;                        // used for calculating how long to wait for next frame
   int64_t nowtime,howlongrunning, howlongtowait;            // for timings


   if (plotWidth!=windowWidth-(2*windowBorder)-keyWidth) printf("NOT SAME: windowWidth-(2*windowBorder)-keyWidth=%d, plotWidth=%d.\n",windowWidth-(2*windowBorder)-keyWidth,plotWidth);

   gettimeofday(&stopwatchus,NULL);                // grab current time
   howlongtowait = ((int64_t)starttimez+((int64_t)counter*(int64_t)usecperframe)) - (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // how long in us until we need to draw the next frame
   //printf("Now: %lu,   Target: %lu.  Therefore sleeping for %lu us and %dns\n",(starttimez+(counter*usecperframe)),((stopwatchus.tv_sec*1000000) + stopwatchus.tv_usec),howlongtowait,howlongtowait*1000);

   if (howlongtowait>0) {
      ts.tv_sec = howlongtowait/1000000;                // # seconds (very unlikely to be in the seconds!)
      ts.tv_nsec = (howlongtowait%1000000)*1000;            // us * 1000 = nano secs
      nanosleep (&ts, NULL);            // if we are ahead of schedule sleep for a bit
   }

   // log lastrowupdated.
   // Upon Receive packet. All between lastrowupdated and currenttimerow will be nothing - clear between lastrowupdated+1 and to now.
   // If lastrowupdated = currenttimerow, nothing to nullify, just add on.
   // Upon Plot screen. All between lastrowupdated and currenttimerow will be nothing - clear between last and to now.
   // If lastrowupdated = currenttimerow, nothing to nullify.

   gettimeofday(&stopwatchus,NULL);                // grab current time
   nowtime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);    // get time now in us
   howlongrunning = nowtime-starttimez;                // how long in us since visualation started running.
   // idle until the next frame/s interval is due

   /*    if (nowtime > (keepalivetime + 30000000)) {            // if more than 30s ago
           keepalivetime = nowtime;                // record time as last time sent
           char buff[40];                        // intialise a buffer for a cmd
           sprintf(buff, "%s %s %s", "ping -c 1 -w 1", inet_ntoa(spinnakerboardip), "&");
           //printf("Keepalive sent: %s\n", buff);            // print on screen
           //system(buff);                        // spawn a keep-alive ping -OR-
           sdp_sender( 0, 0x21, 99, 0,0,0,4, 0, 0, 0, 0);                // send a dummy/keepalive message out to (0,0) will be discarded
       }
   */     //keepalive message no longer used (5th Oct)

   if (printpktgone!=0 && nowtime > (printpktgone + 1000000)) {
      printpktgone = 0;                    // if packet send message has been displayed for more than 1s, stop its display
      somethingtoplot=1;                    // force refresh screen
   }

   if (PLOTONLYONDEMAND==0) somethingtoplot=1;            // force the refresh for this frame timing (even if nothing has changed!)

   if (somethingtoplot!=0) {
      //printf("Plotme\n");                    // indicate we will need to refresh the screen
      windowToUpdate=win1;                    // update the main master window
      display();                          // update the display - will be timered inside this function to get desired FPS
      if (win2) {
         int mainwindowmode=displaymode;
         displaymode=RASTER;
         windowToUpdate=win2;                // do the 2nd window too
         display();                      // update the display - will be timered inside this function to get desired FPS
         displaymode=mainwindowmode;
         //display_win2();                 // plot 2nd window for testing if open TODO make scalable with many windows etc.
      }
   } else {}                            // do we actually want to update a bare min number of times - e.g. once per sec?
}


void myinit (void)
{
   glClearColor (0.0, 0.0, 0.0, 1.0);
   glColor3f (1.0, 1.0, 1.0);
   glShadeModel (GL_SMOOTH);   // permits nice shading between plot points for interpolation if required
   filemenu();
   transformmenu();
   modemenu();
   colmenu();
   rebuildmenu();
}



void myfilemenu (int value)
{
   int menuitem=1;
   if (outputfileformat==0) {                    // no savefile open
      if (value==menuitem++) {
         outputfileformat=1;
         open_or_close_output_file();    // start saving data in spinn
      }
      if (value==menuitem++) {
         outputfileformat=2;
         open_or_close_output_file();    //  or neurotools format
      }
   } else {                                    // savefile open
      if (writingtofile==2)    {
         if (value==menuitem++) {
            writingtofile=0;                                //   and paused
            printf("Recording resumed...  ");
         }
      } else {
         if (value==menuitem++) {
            writingtofile=2;                                //   or running
            printf("Recording paused...  ");
         }
      }
      if (value==menuitem++) open_or_close_output_file();                // closefile out
   }
   needtorebuildmenu=1;
   //rebuildmenu(); // rebuild menu with state modified by this work
}

void filemenu (void)
{
   int menuitem=1;
   glutDestroyMenu(filesubmenu);
   filesubmenu = glutCreateMenu(myfilemenu);
   if (outputfileformat==0) {                    // no savefile open
      glutAddMenuEntry("Save Input Data in .spinn format (replayable)",menuitem++);        // start saving data in spinn
      glutAddMenuEntry("Save Input Spike Data as write-only .neuro Neurotools format",menuitem++);//  or neurotools format
   } else {                                        // savefile open
      if (writingtofile==2)    glutAddMenuEntry("Resume Saving Data to file",menuitem++);    //   and paused
      else glutAddMenuEntry("Pause Saving Data to file",menuitem++);            //   or running
      glutAddMenuEntry("End saving Data to file",menuitem++);                // closefile out
   }
}


void mytransformmenu (int value)
{
   int menuitem=1;
   if (value==menuitem++) if (xflip==0) xflip=1;
      else xflip=0;
   if (value==menuitem++) if (yflip==0) yflip=1;
      else yflip=0;
   if (value==menuitem++) if (vectorflip==0) vectorflip=1;
      else vectorflip=0;
   if (value==menuitem++) if (rotateflip==0) rotateflip=1;
      else rotateflip=0;
   if (value==menuitem++) cleardown();
   needtorebuildmenu=1;
   //rebuildmenu(); // rebuild menu with state modified by this work
}

void transformmenu (void)
{
   int menuitem=1;
   glutDestroyMenu(transformsubmenu);
   transformsubmenu = glutCreateMenu(mytransformmenu);
   glutAddMenuEntry("(X) Mirror (left to right swop)",menuitem++);
   glutAddMenuEntry("(Y) Reflect (top to bottom swop)",menuitem++);
   glutAddMenuEntry("(V) Vector Swop (Full X+Y Reversal)",menuitem++);
   glutAddMenuEntry("90 (D)egree Rotate Toggle",menuitem++);
   glutAddMenuEntry("(C) Revert changes back to default",menuitem++);
}

void mycolmenu (int value)
{
   int menuitem=1;
   if (value==menuitem++) colourused=MULTI;
   if (value==menuitem++) colourused=GREYS;
   if (value==menuitem++) colourused=REDS;
   if (value==menuitem++) colourused=GREENS;
   if (value==menuitem++) colourused=BLUES;
   if (value==menuitem++) colourused=THERMAL;
   if (value==menuitem++) colourused=RED;
   if (value==menuitem++) colourused=BLUE;
   needtorebuildmenu=1;
   //rebuildmenu(); // rebuild menu with state modified by this work
}

void colmenu (void)
{
   int menuitem=1;
   glutDestroyMenu(coloursubmenu);
   coloursubmenu = glutCreateMenu(mycolmenu);
   glutAddMenuEntry("(1) MultiColoured",menuitem++);
   glutAddMenuEntry("(2) Greys",menuitem++);
   glutAddMenuEntry("(3) Reds",menuitem++);
   glutAddMenuEntry("(4) Greens",menuitem++);
   glutAddMenuEntry("(5) Blues",menuitem++);
   glutAddMenuEntry("(6) Thermal Imaging",menuitem++);
   glutAddMenuEntry("(7) ON/OFF Red Only",menuitem++);
   glutAddMenuEntry("(8) ON/OFF Blue Only",menuitem++);

}

void mymodemenu (int value)
{
   int menuitem=1;
   if (value==menuitem++) displaymode=TILED;
   if (value==menuitem++) displaymode=HISTOGRAM;
   if (value==menuitem++) displaymode=INTERPOLATED;
   if (value==menuitem++) displaymode=LINES;
   if (value==menuitem++) displaymode=RASTER;
   if (value==menuitem++) displaymode=EEGSTYLE;
   needtorebuildmenu=1;
   //rebuildmenu(); // rebuild menu with state modified by this work
}

void modemenu (void)
{
   int menuitem=1;
   glutDestroyMenu(modesubmenu);
   modesubmenu = glutCreateMenu(mymodemenu);
   glutAddMenuEntry("(T)iled",menuitem++);
   glutAddMenuEntry("(H)istogram",menuitem++);
   glutAddMenuEntry("(I)nterpolated",menuitem++);
   glutAddMenuEntry("(L)ines",menuitem++);
   glutAddMenuEntry("(R)aster",menuitem++);
   glutAddMenuEntry("(M)ultichannel EEG Style",menuitem++);
}

void rebuildmenu (void)
{
//    printf("hello rasterpop:%d, livebox:%d.\n",rasterpopulation,livebox);
   int menuitem=1;
   glutDestroyMenu(RHMouseMenu);
   RHMouseMenu = glutCreateMenu(mymenu);

   if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) { 
      //if (displaymode==HISTOGRAM || displaymode==TILED) {
      if (livebox>=0) {
         if (rasterpopulation==livebox) {
            glutAddMenuEntry("(Z) Turn OFF this population's Raster Plot",menuitem++);    // if box not selected offer to turn raster plots off all
            // if off then turn on
         }
         else {
            glutAddMenuEntry("(Z) Turn Raster Plot ON for this population",menuitem++);    // if box not selected offer to turn raster plots off all
         }
      }
      //}
      glutAddMenuEntry("Turn OFF ALL population Raster Plots",menuitem++);    // if mode unavailable to deselect box then offer to turn it all off!
      glutAddMenuEntry("-----",menuitem++);  // segmenter
   }
   glutAddSubMenu("Plot Mode",modesubmenu);
   glutAddSubMenu("Transform Plot",transformsubmenu);
   glutAddSubMenu("Colours",coloursubmenu);
   glutAddSubMenu("Save Data Operations",filesubmenu);

   glutAddMenuEntry("-----",menuitem++); // segmenter
   if (displaymode==HISTOGRAM || displaymode==TILED) {
      if (gridlines==1) glutAddMenuEntry("Grid (B)orders off",menuitem++);
      else glutAddMenuEntry("Grid (B)orders on",menuitem++);
      if (plotvaluesinblocks==1) glutAddMenuEntry("Numbers (#) off",menuitem++);
      else glutAddMenuEntry("Numbers (#) on",menuitem++);
   }
   if (fullscreen==1) glutAddMenuEntry("(F)ull Screen off",menuitem++);
   else glutAddMenuEntry("(F)ull Screen on",menuitem++);


   glutAddMenuEntry("-----",menuitem++); // segmenter


   if (freezedisplay==0) glutAddMenuEntry("(\") Pause Plot",menuitem++);
   else glutAddMenuEntry("(P)lay / Restart Plot",menuitem++);
   glutAddMenuEntry("(Q)uit",menuitem++);
   glutAttachMenu(GLUT_RIGHT_BUTTON);
}


void mymenu (int value)
{
   int menuitem=1;

   if (SIMULATION==RATEPLOT|| SIMULATION==RATEPLOTLEGACY) { 
      //if (displaymode==HISTOGRAM || displaymode==TILED) {
      if (livebox>=0) {    // we have a box selected
         if (rasterpopulation==livebox) {
            if (value==menuitem++) {
               for(int i=0; i<(xdim*ydim); i++) {
                  sdp_sender(0, 0x80+i+1,258,0,4,0,0);  // turn off raster on all populations
                  int rubbish=0;
                  for(int j=0; j<10000000; j++) rubbish++;
                  printf("%d: ",i);
               }
               printf("\n");
               if (win2!=0) destroy_new_window();      // if spawned window does exist then close it
               rasterpopulation = -1;
               //livebox=-1;
               somethingtoplot=1;
               //printf("rasterpopulation: %d, livebox:%d",rasterpopulation,livebox);
            }
         } else {
            if (value==menuitem++) {
               for(int i=0; i<(xdim*ydim); i++) {
                  sdp_sender(0, 0x80+i+1,258,0,4,0,0);      // turn off raster for all populations
                  int rubbish=0;
                  for(int j=0; j<10000000; j++) rubbish++;  //
                  printf("%d, ",i);
               }
               printf("\n");
               if (win2==0) create_new_window();           // if window doesn't already exist then create it on demand
               sdp_sender(0, 0x80+livebox+1,258,1,4,0,0);    // send message to turn on bit 4 (raster on)
               rasterpopulation=livebox;
               //livebox=-1;
               somethingtoplot=1;
               //printf("rasterpopulation: %d, livebox:%d",rasterpopulation,livebox);
            }
         }
      }
      //}
      if (value==menuitem++) {
         rasterpopulation = -1;
         for(int i=0; i<(xdim*ydim); i++) {
            sdp_sender(0, 0x80+i+1,258,0,4,0,0);      // turn off raster on all populations
            int rubbish=0;
            for(int j=0; j<10000000; j++) rubbish++;  //
            printf("%d, ",i);
         }
         printf("\n");
         if (win2!=0) destroy_new_window();      // if spawned window does exist then close it
         livebox=-1;
         somethingtoplot=1;
         //printf("rasterpopulation: %d, livebox:%d",rasterpopulation,livebox);
      }
      if (value==menuitem++) {}; //do nothing - segmenter;
   }

   if (value==menuitem++) {}; //do nothing - segmenter;

   if (displaymode==HISTOGRAM || displaymode==TILED) {
      if (value==menuitem++) if (gridlines==1) gridlines=0;
         else gridlines=1;
      if (value==menuitem++) if (plotvaluesinblocks==1) plotvaluesinblocks=0;
         else plotvaluesinblocks=1;
   }
   if (value==menuitem++) {
      if (fullscreen==1) {
         fullscreen=0;                // turn off full screen mode
         windowBorder=oldwindowBorder;        // restore old bordersize
         windowWidth-=keyWidth;            // recover the key area
         plotWidth=windowWidth-(2*windowBorder)-keyWidth;
      } else {
         fullscreen=1;
         oldwindowBorder=windowBorder;        // used as border disappears when going full-screen
         windowBorder=0;                // no borders around the plots
         windowWidth+=keyWidth;            // take over the area used for the key too
         plotWidth=windowWidth-keyWidth;
      }
   }


   if (value==menuitem++) {}; //do nothing - segmenter;

   if (value==menuitem++) {
      if (freezedisplay==0) {
         {  int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below. PAUSE
            //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
            sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 2, 0,0,0,4, 0, 0, 0, 0);
            // send pause packet out
            freezedisplay=1;
            struct timeval stopwatchus;
            gettimeofday(&stopwatchus,NULL);                    // grab current time
            freezetime = (((int64_t)stopwatchus.tv_sec*(int64_t)1000000) + (int64_t)stopwatchus.tv_usec);
         }    // get time now in us
      } else {
         {  int i=0;    // only sending to 0,0  - if needs to go to all chips, uncomment line below. RESUME
            //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
            sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 3, 0,0,0,4, 0, 0, 0, 0);
            // send resume/restart packet out
            freezedisplay=0;
         }
      }
   }
   if (value==menuitem++) safelyshut();

   needtorebuildmenu=1;


}


void logifmenuopen(int status, int x, int y)
{
   if (status == GLUT_MENU_IN_USE)
      menuopen = 1;            // no changes while it's open
   else {
      if (needtorebuildmenu==1) {
         filemenu();
         rebuildmenu();
      }
      menuopen = 0;            // if menu is not open we can make changes
      needtorebuildmenu=0;
   }
}


void safelyshut(void)
{
   // in some circumstances this gets run twice, therefore we check for this (partic the frees!)
   if (safelyshutcalls==0) {
      int ii;         // used for freeing loops
      int i=0;    // only sending to 0,0  - if all chips, uncomment line below.
      //for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
      if (spinnakerboardport!=0)
         sdp_sender( (256*(i/(XDIMENSIONS/EACHCHIPX)) + (i%(YDIMENSIONS/EACHCHIPY)) ), 0x21, 0, 0,0,0,4, 0, 0, 0, 0);
      // send exit packet out if we are interacting

      open_or_close_output_file();        // Close down any open output file
      if (fileinput!=NULL) fclose(fileinput);    // deal with input file

      // free up mallocs made for dynamic arrays
      if (SIMULATION == RATEPLOT || RATEPLOTLEGACY) free(biascurrent);

      //free(immediate_data);  // superfluous

      for (ii=0; ii<HISTORYSIZE; ii++) free(history_data[ii]);
      free(history_data);
      for (ii=0; ii<HISTORYSIZE; ii++) free(history_data_set2[ii]);
      free(history_data_set2);

      free(immediate_data);

      for (ii=0; ii<XDIMENSIONS*YDIMENSIONS; ii++) free(maplocaltoglobal[ii]);
      free(maplocaltoglobal);
      for (ii=0; ii<XDIMENSIONS*YDIMENSIONS; ii++) free(mapglobaltolocal[ii]);
      free(mapglobaltolocal);

      //printf("Safely Shutting down and that. %d\n",fileinput);
      safelyshutcalls++;          // note that this routine has been run before
   }
   exit(0);                // kill program dead
}

void display_win2(void)
{
   glutSetWindow(win2);

   glClearColor (0.2,0.2,0.7,1.0);                     // background colour - random surround
   glClear(GL_COLOR_BUFFER_BIT);

   glColor4f(0.0,0.0,0.0,1.0);                         // Black Text for Labels

   char stringy[]="This is a Test Mssg\n";
   printgl(250,500, GLUT_BITMAP_HELVETICA_12, stringy);
   printglstroke (25+(rand() % 50), 20, 0.12, 0, stringy);
   printgl(250,300,GLUT_BITMAP_TIMES_ROMAN_24,stringy);
   glColor4f(1.0,0.0,0.0,1.0);
   glBegin(GL_QUADS);
   glVertex2f(100,100);
   glVertex2f(200,100);
   glVertex2f(200,800);
   glVertex2f(100,800);
   glEnd();    // draw centre window
   //printf("hello in display_win2 %d - thinks we are in win %d\n",win2, glutGetWindow());
   glFlush();
   glutSwapBuffers();
}


void destroy_new_window(void)
{
   printf("Destroying new Window: %d, - after destruction is:", win2);
   glutDestroyWindow(win2);
   win2=0;
   printf("%d.\n",win2);
}



void create_new_window(void)
{
//    printf("Creating new Window. Was: %d, - and now after creations is:", win2);
   glutInitDisplayMode (GLUT_DOUBLE|GLUT_RGB);    /* Set the display mode */
   glutInitWindowSize (windowWidth,windowHeight);   /* Set the window size */
   glutInitWindowPosition (800, 100);    /* Set the window position */
   win2 = glutCreateWindow ("Spawned window");  /* Create the window */
   myinit();
   glutDisplayFunc(display);   /* Register the "display" function */
   glutReshapeFunc(reshape);   /* Register the "reshape" function */
   glutIdleFunc(idleFunction); /* Register the idle function */
   glutSpecialFunc (specialDown);  /* Register the special key press function  */
   glutSpecialUpFunc (specialUp); /* Register the special key release function */
   glutKeyboardFunc(keyDown); /* Register the key press function */
   glutKeyboardUpFunc(keyUp); /* Register the key release function */
   glutMouseFunc(mousehandler); /* Register the mouse handling function */
//    printf("%d.\n",win2);
}

void open_or_close_output_file(void)
{
   time_t rawtime;
   struct tm * timeinfo;
   char filenamebuffer [80];
   time ( &rawtime );
   timeinfo = localtime ( &rawtime );
   //fprintf(fileoutput,"# SpiNNaker Dump File Format\n");  // ! writing header for neurotools format file

   if (fileoutput==NULL) {                // If file isn't already open, so this is a request to open it
      if (outputfileformat==2) {               //SAVE AS NEUROTOOLS FORMAT
         strftime (filenamebuffer,80,"packets-20%y%b%d_%H%M.neuro",timeinfo);
         printf("Saving spike packets in this file:\n       %s\n",filenamebuffer);
         fileoutput = fopen(filenamebuffer, "w");
         fprintf(fileoutput,"# first_id =          \n");  // ! writing header for neurotools format file
         fprintf(fileoutput,"# n =          \n");  // ! writing header for neurotools format file
         fprintf(fileoutput,"# dt = 1.0\n");  // ! writing header for neurotools format file
         fprintf(fileoutput,"# dimensions = [          ]\n");  // ! writing header for neurotools format file
         fprintf(fileoutput,"# last_id =          \n");  // ! writing header for neurotools format file
         //fprintf(fileoutput,"# Note: If entries above are blank - the file was not closed properly with 'q' or Graphic 'X'\n");  // ! writing header for neurotools format file
      } else if (outputfileformat==1) {           //SAVE AS SPINN (UDP Payload) FORMAT
         strftime (filenamebuffer,80,"packets-20%y%b%d_%H%M.spinn",timeinfo);
         printf("Saving all input data in this file:\n       %s\n",filenamebuffer);
         fileoutput = fopen(filenamebuffer, "wb");
      }
   } else {                    // File is open already, so we need to close
      if (outputfileformat==2) {               // File was in neurotools format
         do {} while (writingtofile==1);     // busy wait for file to finish being updated if in-flight
         writingtofile=2;            // stop anybody else writing the file, pause further updating
         fseek(fileoutput,13 ,SEEK_SET);     // pos 13 First ID
         fprintf(fileoutput,"%d",minneuridrx);        // write lowest detected neurid
         fseek(fileoutput,29 ,SEEK_SET);     // pos 29 n number of neurons-1
         fprintf(fileoutput,"%d",(maxneuridrx-minneuridrx));        // write number of neurons in range
         fseek(fileoutput,67 ,SEEK_SET);     // pos 67 dimensions number of neurons-1
         fprintf(fileoutput,"%d",(maxneuridrx-minneuridrx));        // write number of neurons in range
         fseek(fileoutput,90 ,SEEK_SET);     // pos 90 Last ID
         fprintf(fileoutput,"%d",maxneuridrx);        // write highest detected neurid
      }
      fflush(fileoutput);
      fclose(fileoutput);
      printf("File Save Completed\n");
      fileoutput=NULL;
      outputfileformat=0;
      writingtofile=0;
   }
}

int paramload(char* config_file_name) {
   // check if visparam exists
   // if not then use in-built defaults
   // if it does then deal with it

   config_t cfg;               /*Returns all parameters in this structure */
   config_setting_t *setting;
   const char *paramblock;
   const char *titletemp;
   int tmp;
   int ii; // used for 2dimensional loops and setting up pointers to lists
   //const char *config_file_name = "visparam.ini";

   config_init(&cfg);     /*Initialization */

   if (!config_read_file(&cfg, config_file_name)) {
      printf("No readable %s in the local directory - configuration defaulted to 48-chip HEATMAP.\n",config_file_name);
      setting != NULL;
      //config_destroy(&cfg);
      //return -1;
   } else {
      /* Get the simulation parameters to use. */
      if (config_lookup_string(&cfg, "simparams", &paramblock)) printf("Sim params specified: %s\n", paramblock);
      else printf("No 'simparams' settings in configuration file.\n");

      setting = config_lookup(&cfg, paramblock);  /*Read the simulation parameters group*/
   }

   if (setting != NULL)
   {
      if (!(config_setting_lookup_string(setting, "TITLE", &titletemp))) titletemp="NO SIMULATION TITLE SUPPLIED";

      long long VALUE=0;

      if (config_setting_lookup_int64(setting, "SIMULATION", &VALUE)) SIMULATION=(int)VALUE;


      if (config_setting_lookup_int64(setting, "WINBORDER", &VALUE)) WINBORDER=(int)VALUE;
      if (config_setting_lookup_int64(setting, "WINHEIGHT", &VALUE)) WINHEIGHT=(int)VALUE;
      if (config_setting_lookup_int64(setting, "WINWIDTH", &VALUE)) WINWIDTH=(int)VALUE;
      config_setting_lookup_float(setting, "TIMEWINDOW", &TIMEWINDOW);
      if (config_setting_lookup_int64(setting, "KEYWIDTH", &VALUE)) KEYWIDTH=(int)VALUE;
      if (config_setting_lookup_int64(setting, "DISPLAYKEY", &VALUE)) DISPLAYKEY=(int)VALUE;
      if (config_setting_lookup_int64(setting, "DISPLAYXLABELS", &VALUE)) DISPLAYXLABELS=(int)VALUE;
      if (config_setting_lookup_int64(setting, "DISPLAYYLABELS", &VALUE)) DISPLAYYLABELS=(int)VALUE;
      if (config_setting_lookup_int64(setting, "BLACKBACKGROUND", &VALUE)) BLACKBACKGROUND=(int)VALUE;
      if (config_setting_lookup_int64(setting, "INITZERO", &VALUE)) INITZERO=(int)VALUE;


      if (config_setting_lookup_int64(setting, "XDIMENSIONS", &VALUE)) XDIMENSIONS=(int)VALUE;
      if (config_setting_lookup_int64(setting, "YDIMENSIONS", &VALUE)) YDIMENSIONS=(int)VALUE;
      if (config_setting_lookup_int64(setting, "EACHCHIPX", &VALUE)) EACHCHIPX=(int)VALUE;
      if (config_setting_lookup_int64(setting, "EACHCHIPY", &VALUE)) EACHCHIPY=(int)VALUE;

      if (config_setting_lookup_int64(setting, "N_PER_PROC", &VALUE)) N_PER_PROC=(int)VALUE;
      if (config_setting_lookup_int64(setting, "ID_OFFSET", &VALUE)) ID_OFFSET=(int)VALUE;

      //printf("%d, %d\n", N_PER_PROC, ID_OFFSET);

      if (!(config_setting_lookup_int64(setting, "XCHIPS", &VALUE))) XCHIPS=(XDIMENSIONS/EACHCHIPX);
      if (!(config_setting_lookup_int64(setting, "YCHIPS", &VALUE))) YCHIPS=(YDIMENSIONS/EACHCHIPY);
      // if not explicitly defined, we assume display will be chipwise

      if (config_setting_lookup_int64(setting, "HISTORYSIZE", &VALUE)) HISTORYSIZE=(int)VALUE;
      if (config_setting_lookup_int64(setting, "MAXRASTERISEDNEURONS", &VALUE)) MAXRASTERISEDNEURONS=(int)VALUE;

      if (config_setting_lookup_int64(setting, "XFLIP", &VALUE)) XFLIP=(int)VALUE;
      if (config_setting_lookup_int64(setting, "YFLIP", &VALUE)) YFLIP=(int)VALUE;
      if (config_setting_lookup_int64(setting, "VECTORFLIP", &VALUE)) VECTORFLIP=(int)VALUE;
      if (config_setting_lookup_int64(setting, "ROTATEFLIP", &VALUE)) ROTATEFLIP=(int)VALUE;   // transform processing

      config_setting_lookup_float(setting, "HIWATER", &HIWATER);
      config_setting_lookup_float(setting, "LOWATER", &LOWATER);
      if (config_setting_lookup_int64(setting, "DYNAMICSCALE", &VALUE)) DYNAMICSCALE=(int)VALUE;
      if (config_setting_lookup_int64(setting, "PERCENTAGESCALE", &VALUE)) PERCENTAGESCALE=(int)VALUE;

      if (config_setting_lookup_int64(setting, "STARTCOLOUR", &VALUE)) STARTCOLOUR=(int)VALUE;
      if (config_setting_lookup_int64(setting, "STARTMODE", &VALUE)) STARTMODE=(int)VALUE;

      if (config_setting_lookup_int64(setting, "LABELBYCHIP", &VALUE)) LABELBYCHIP=(int)VALUE;
      if (config_setting_lookup_int64(setting, "PLAYPAUSEXIT", &VALUE)) PLAYPAUSEXIT=(int)VALUE;
      if (config_setting_lookup_int64(setting, "DISPLAYMINIPLOT", &VALUE)) DISPLAYMINIPLOT=(int)VALUE;
      if (config_setting_lookup_int64(setting, "INTERACTION", &VALUE)) INTERACTION=(int)VALUE; // options for screen output & interactivity

      if (config_setting_lookup_int64(setting, "MAXFRAMERATE", &VALUE)) MAXFRAMERATE=(int)VALUE;
      if (config_setting_lookup_int64(setting, "PLOTONLYONDEMAND", &VALUE)) PLOTONLYONDEMAND=(int)VALUE;
      //printf("FPS: %d, On Demand:%d.\n",MAXFRAMERATE,PLOTONLYONDEMAND);

      if (config_setting_lookup_int64(setting, "SDPPORT", &VALUE)) SDPPORT=(int)VALUE;
      //printf("*****\n\nSDPPORT: %d %ld\n\n****",SDPPORT,VALUE);

      if (config_setting_lookup_int64(setting, "FIXEDPOINT", &VALUE)) FIXEDPOINT=(int)VALUE;

      if (config_setting_lookup_int64(setting, "BITSOFPOPID", &VALUE)) BITSOFPOPID=(int)VALUE;

      config_setting_lookup_float(setting, "ALTERSTEPSIZE", &ALTERSTEPSIZE);
      config_setting_lookup_float(setting, "DECAYPROPORTION", &DECAYPROPORTION);

   } else {
      printf("Sim Name not found, so using defaults\n");
      titletemp="SIM PARAMETER LIST NOT FOUND";
      XCHIPS=(XDIMENSIONS/EACHCHIPX);
      YCHIPS=(YDIMENSIONS/EACHCHIPY);

   }

   // this section sets the variables based on the input (or defaults)

   strcpy (TITLE,titletemp);

   windowBorder=WINBORDER;
   windowHeight=WINHEIGHT;
   keyWidth=KEYWIDTH;
   if (DISPLAYKEY==0) keyWidth=0;
   windowWidth=WINWIDTH+keyWidth; // startup for window sizing
   displayWindow = TIMEWINDOW;

   plotWidth=windowWidth-(2*windowBorder)-keyWidth;     // how wide is the actual plot area
   printlabels=(windowBorder>=100);                     // only print labels if the border is big enough

   // set global variables based on input file data
   xdim = XDIMENSIONS;        // number of items to plot in the x dimension
   ydim = YDIMENSIONS;        // number of items to plot in the y dimension

   colourused = STARTCOLOUR;    // start with requested colour scheme
   displaymode = STARTMODE;    // initialise mode variable for the start

   if (SIMULATION == HEATMAP) xorigin=(windowWidth+keyWidth)-(controlboxes*(boxsize+gap));  // for the control box

   // malloc appropriate memory

   //float immediate_data[XDIMENSIONS*YDIMENSIONS];  // this stores the value of each plotted point data (time == now)
   //immediate_data = (float*)malloc(XDIMENSIONS*YDIMENSIONS*sizeof(float)); // allocate an array of floats   superfluous

   //float history_data[HISTORYSIZE][XDIMENSIONS*YDIMENSIONS];   // this stores the historic value the plotted points (double the initial width should be sufficient)
   history_data = (float**) malloc (HISTORYSIZE*sizeof(float*)); // allocate an array of pointers (rows), then cols
   for (ii=0; ii<HISTORYSIZE; ii++) history_data[ii]=(float*) malloc(XDIMENSIONS*YDIMENSIONS*sizeof(float*));

   //float history_data_set2[HISTORYSIZE][MAXRASTERISEDNEURONS];      // 2nd set of data for ancillary raster plot window
   history_data_set2 = (float**) malloc (HISTORYSIZE*sizeof(float*)); // allocate an array of pointers (rows), then cols
   for (ii=0; ii<HISTORYSIZE; ii++) history_data_set2[ii]=(float*) malloc(MAXRASTERISEDNEURONS*sizeof(float*));

   //int immediate_data[XDIMENSIONS*YDIMENSIONS];        // this creates a buffer tally for the Ethernet packets (1 ID = one plotted point)
   immediate_data = (float*)malloc(XDIMENSIONS*YDIMENSIONS*sizeof(float)); // allocate an array of floats

   //int maplocaltoglobal[XDIMENSIONS*YDIMENSIONS][2];  // always 2 wide.  Size of mapping from X&Y coords #of pops
   maplocaltoglobal = (int**) malloc (XDIMENSIONS*YDIMENSIONS*sizeof(int*)); // allocate an array of pointers (rows), then cols
   for (ii=0; ii<XDIMENSIONS*YDIMENSIONS; ii++) maplocaltoglobal[ii]=(int*) malloc(2*sizeof(int*));
   //int mapglobaltolocal[XDIMENSIONS*YDIMENSIONS][2];  // and the reverse from the 2nd file
   mapglobaltolocal = (int**) malloc (XDIMENSIONS*YDIMENSIONS*sizeof(int*)); // allocate an array of pointers (rows), then cols
   for (ii=0; ii<XDIMENSIONS*YDIMENSIONS; ii++) mapglobaltolocal[ii]=(int*) malloc(2*sizeof(int*));

   // Per visualiser option
   if (SIMULATION == RATEPLOT || RATEPLOTLEGACY)
      biascurrent = (float*)malloc(XDIMENSIONS*YDIMENSIONS*sizeof(float)); // allocate an array of floats for this
   //float biascurrent[XDIMENSIONS*YDIMENSIONS];
   //CP made dynamic from above


   config_destroy(&cfg);

}

int main(int argc, char **argv)
{
   //printf("\n\n\n\nSystem Bit Size Detected = %d bit.\n\n\n\n",(int) MACHINEBITS);

   // read and check the command line arguments
   int errfound=0;
   int gotconfigfn=0, gotreplayfn=0, gotl2gfn=0, gotg2lfn=0, gotanipaddr;
   char *configfn, *replayfn, *l2gfn, *g2lfn, *sourceipaddr;
   float replayspeed=1.0;

   int commandlooper;
   for (commandlooper = 1; commandlooper < argc; commandlooper++) {  // go through all the arguments
      if ((strcmp(argv[commandlooper], "-c") == 0) || (strcmp(argv[commandlooper], "-config") == 0)) {
         if (commandlooper+1 < argc) {
            // TODO: check if filename begins with - and give error if this is the case?
            gotconfigfn = 1;
            configfn = argv[commandlooper+1];
            printf("Attempting to load configuration file: %s.\n",configfn);
            commandlooper++;
         } else {
            errfound++;
            printf("*No local config filename provided. Error.\n");
         }
      } else if ((strcmp(argv[commandlooper], "-r") == 0) || (strcmp(argv[commandlooper], "-replay") == 0)) {
         if (commandlooper+1 < argc) {
            gotreplayfn = 1;
            replayfn = argv[commandlooper+1];
            printf("Attempting to load file for replay: %s.\n",replayfn);
            commandlooper++;
            if ((commandlooper+1 < argc) && (atof(argv[commandlooper+1])>=0.1) && (atof(argv[commandlooper+1])<=100.0) ) {
               // if next argument is a number then this is the multiplier
               replayspeed=atof(argv[commandlooper+1]);
               printf("** Replay multiplier: %f.\n",replayspeed);
               commandlooper++;
            }
            else {
               printf("** Note: no multiplier option supplied.\n ");
            }
         } else {
            errfound++;
            printf("** No replay filename provided. Error.\n");
         }
      } else if (strcmp(argv[commandlooper], "-l2g") == 0) {
         if (commandlooper+1 < argc) {
            gotl2gfn = 1;
            l2gfn = argv[commandlooper+1];
            printf("Attempting to load Local to Global file: %s.\n",l2gfn);
            commandlooper++;
         } else {
            errfound++;
            printf("*** No L to G filename provided. Error.\n");
         }
      } else if (strcmp(argv[commandlooper], "-g2l") == 0) {
         if (commandlooper+1 < argc) {
            gotg2lfn = 1;
            g2lfn = argv[commandlooper+1];
            printf("Attempting to load Global to Local file: %s.\n",g2lfn);
            commandlooper++;
         } else {
            errfound++;
            printf("**** No G to L filename provided. Error.\n");
         }
      } else if (strcmp(argv[commandlooper], "-ip") == 0) {
         // spinnakerboardip is set
         if (commandlooper+1 < argc) {                 // check to see if a 2nd argument provided
            sourceipaddr = argv[commandlooper+1];     // here's our hostname or ip address to check
            hostent *validipfound = gethostbyname(sourceipaddr);
            if(!validipfound) {
               errfound++;                           // if doesn't exist then fail
               printf("***** Can't figure out the IP address/hostname supplied, sorry. Error.\n");
            } else {
               spinnakerboardip = *(in_addr *)validipfound->h_addr;
               spinnakerboardipset++;
               //spinnakerboardport=SDPPORT;
               //init_sdp_sender();  // unsure of sending port # - so this is a guess...
               printf("Waiting for packets only from: %s.\n",inet_ntoa(spinnakerboardip));
               commandlooper++;
            }
         } else {
            errfound++;
            printf("***** You said you'd supply an IP address/hostname but didn't, I'm sad. Error.\n");
         }
      } else {
         errfound++;
      }
   }

   if (gotl2gfn==1 && gotg2lfn==0) {
      printf("L to G filename specified, but G to L is not. Error.\n");
      errfound=1;
   }
   if (gotl2gfn==0 && gotg2lfn==1) {
      printf("G to L filename specified, but L to G is not. Error.\n");
      errfound=1;
   }

   if(errfound>0) {
      printf("\n Unsure of your command line options old chap.\n\n");
      fprintf(stderr, "usage: %s [-c configfile] [-r savedspinnfile [replaymultiplier(0.1->100)]] [-l2g localtoglobalmapfile] [-g2l globaltolocalmapfile] [-ip boardhostname|ipaddr]\n", argv[0]);
      exit(1);
   }

   if (gotconfigfn==0) configfn=(char*)"visparam.ini";   // default filename if not supplied by the user

   printf("\n\n"); // give some spacing for the output


   paramload(configfn);    // recover the parameters from the file used to configure this visualisation


   if (gotl2gfn==1 && gotg2lfn==1) {  // if both translations are provided
      readmappings(l2gfn, g2lfn);       // read mappings file into array
      while(!mappingfilesread) {};   // ensure file is read before continuing
   }


   for (int ii=0; ii<(XDIMENSIONS*YDIMENSIONS); ii++) {
      int xcoordinate,ycoordinate,index;
      convert_index_to_coord(ii, &xcoordinate, &ycoordinate);
      index=convert_coord_to_index(xcoordinate, ycoordinate);
      //printf("%d -> %d,%d. (updown: %d  side2side: %d)  reverts -> %d\n",ii,xcoordinate,ycoordinate,xcoordinate*(XDIMENSIONS/EACHCHIPX),index,index,index);
   }
   //call with: convert_index_to_coord(index, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)
   //convert_coord_to_index(int x, int y)

   cleardown();    // reset the plot buffer to something sensible (i.e. 0 to start with)
   //if (!printlabels) keyWidth=0;    // only if borders are wide enough then print the labelling/controls/titles around the screen
   //printf("Labels: %d, keyWidth: %d\n",printlabels,keyWidth);
   gettimeofday(&startimeus,NULL);
   starttimez = (((int64_t)startimeus.tv_sec*(int64_t)1000000) + (int64_t)startimeus.tv_usec);
   keepalivetime = starttimez;

   //printf ("Sizes of int: %d, long: %d, and int64_t: %d\n",sizeof(int), sizeof(long), sizeof(int64_t));

   for(int j=0; j<(HISTORYSIZE); j++) for(int i=0; i<(xdim*ydim); i++) history_data[j][i]=INITZERO?0.0:NOTDEFINEDFLOAT;
   //for(int j=0;j<(HISTORYSIZE);j++) for(int i=0;i<(xdim*ydim);i++) history_data[j][i]=(((float)i*7.0)+(float)(rand() % 10))*((float)j/((float)HISTORYSIZE));

   if (gotreplayfn == 0) {
      fprintf(stderr, "No Input File provided.   Using Ethernet Frames Only\n");
   }
   else {
      playbackmultiplier = replayspeed;
      if (playbackmultiplier==0.0) playbackmultiplier=1;      // if mis-understood set to default 1
      if (playbackmultiplier<0.01) playbackmultiplier=0.01;      // if too slow, floor at 0.01 speed.
      if (playbackmultiplier>100) playbackmultiplier=100;      // if too fast a multiplier ceiling at 100.
      printf("\nGot a request for a file called: %s.\n",replayfn);
      if (playbackmultiplier!=1) printf("    Requested Playback speed will be at %3.2f rate.\n",playbackmultiplier);
      pthread_t p1;
      if ((fileinput = fopen(replayfn, "rb")) == NULL) {
         fprintf(stderr, "I can't read the file you've specified you muppet:\n");
         exit(2);
      }  // check if file is readable

      // setup with target 127.0.0.1 on right port if not already set
      if (spinnakerboardipset==0) inet_aton("127.0.0.1",&spinnakerboardip);
      spinnakerboardport=SDPPORT; // SDPPORT is used for outgoing cnnx
      spinnakerboardipset++;
      init_sdp_sender();
      printf("Set up to receive internally from %s on port: %d\n", inet_ntoa(spinnakerboardip),SDPPORT);
      //fprintf(fileoutput,"# SpiNNaker Dump File Format\n");  // ! writing header for neurotools format file
      pthread_create (&p1, NULL, load_stimulus_data_from_file, NULL);    // away the file receiver goes
   }

   pthread_t p2;            // this sets up the thread that can come back to here from type
   init_sdp_listening();        //initialization of the port for receiving SDP frames
   pthread_create (&p2, NULL, input_thread_SDP, NULL);    // away the SDP network receiver goes

   glutInit(&argc, argv);  /* Initialise OpenGL */

   glutInitDisplayMode (GLUT_DOUBLE|GLUT_RGB);    /* Set the display mode */
   glutInitWindowSize (windowWidth+keyWidth,windowHeight);   /* Set the window size */
   glutInitWindowPosition (0, 100);    /* Set the window position */
   win1 = glutCreateWindow ("VisRT - plotting your network data in real time");  /* Create the window */
   windowToUpdate = win1;
   myinit();
   glutDisplayFunc(display);   /* Register the "display" function */
   glutReshapeFunc(reshape);   /* Register the "reshape" function */
   glutIdleFunc(idleFunction); /* Register the idle function */
   glutSpecialFunc (specialDown);  /* Register the special key press function  */
   glutSpecialUpFunc (specialUp); /* Register the special key release function */
   glutKeyboardFunc(keyDown); /* Register the key press function */
   glutKeyboardUpFunc(keyUp); /* Register the key release function */
   glutMouseFunc(mousehandler); /* Register the mouse handling function */
#ifdef TESTING
   glutMotionFunc (MouseMotion); /* Register the mouse handling function when a button depressed */
#endif

   //glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
   glutCloseFunc(safelyshut); // register what to do when the use kills the window via the frame object
   glutMenuStatusFunc(logifmenuopen);    // this keeps an eye on whether a window is open (as can't alter when open!)
   //create_new_window();

   glutMainLoop(); /* Enter the main OpenGL loop */
   printf("goodbye");

   return 0;
}

// -----------------------------------------------------------------------------------------------------
//  Legacy Versioning information
// -----------------------------------------------------------------------------------------------------
//
// CP 28/09/12 added -lpthread as not implicit in some builds, 14/04/2013 added libconfig for config file reading
// 5th Oct 2012:  Fixed Bug with timer/wraparound (casting issue), and support for 48 chip model where chips are missing to be greyscales
// v17.2 - 9th Nov. Fix Temp/CPU x/y display orientations, chunked reading files so doesn't run out of memory, times >6mins now in "9m47" format, spike rate option for raster.
// v17.1 - 3rd Nov. Added 90 (D)egree rotate to the transformation menu, fixed X/Y transposition error on bias sends for ratedemo.
// v17.0 - 3rd Nov. Coordinates system redone to better support future simulation and correct some 'hacks' done for heatdemo
// v16.8 - 2nd Nov. fixed issue with multi-chip sims for rate plot (was hard coded for 0,0 previously)
// v16.7 - 1st Nov. long replaced with int64_t for 32/64 bit compatibility of saved files.
// v16.6 - 1st Nov. Added Thermometer and CPU Utilisation simulations, fixed some aspects of rateplot
// v16.5 - 30th Oct. File Controls now on right mouse button rather than in defines.  Note: File Pause just drops data, it does not pause time!
// v16.4 - 29th Oct. Fixed colour bug & simplified colour map section
// v16.3 - 27th Oct. Fixed bug with playback if a pause of >1sec without packets
// v16.2 - 27th Oct. Added option to play back recording at different rates (multiplier at cmd line)
//         eg. "visualiser file.spinn 4"  will play back at 4 speed, or 0.2 (for 1/5th speed)
// v16.1 - 26th Oct. Fixes to load/save routines. Going into SVN now.
// v16.0 - 25th Oct. Added support for Load/Save of files of packet dumps instead of live Ethernet... (add filename as 1st argument to load a .spinn file).
//        For save, uncomment DEFINE and look for dated "packetsYYMMMDD_HHMM.spinn"   Alternative save format NeuroTools (.neuro) - see 2nd define
// v15.3 - 25th Oct. Added support for multi-chip spike rate plots
// v15.2 - 24th Oct. S(q)uare becomes 'b' for borders,  E(x)it becomes (Q)uit.  'Y' 'X' and 'V'ector plot flips added to keys indicated, dynamically raster plot neurids received (and scale to suit, no longer fixed at 100)
// v15.1 - 21st Oct. Added zero (0) and randomise (9) values for heatmap demo
// v15.0 - 20th Oct. EEG option added. (inbetween version numbers are unlucky), little map usable for selection & in all modes now.
// v12.4 - 19th Oct. Added '0' zero option on heatmap to reset all to zero. Fix double entry for Right click to turn off all Raster entries.
// v12.3 - 18th Oct. Bug fix histogram full size, ID of packet receive ignores hellos, startup window 0 error, e(x)it on menu, hide (" > X) controls from 2nd window
// v12.2 - 17th Oct. Added Right Mouse Button support (including special features for rate plots spawning raster plots)
// v12.1 - 17th Oct. Added support for 'r'aster plot option.  Also on RATEPLOT code, pressing 'z' on selected tile will open population raster plot
// v12.0 - 13th Oct. Added support for line plotting (l) - worms for each data value
// v11.1 - 11th Oct. Added sQuare gridlines 'q' support, and green and blue colour options (4,5), add YFLIP define option
// v11.0 - 11th Oct. Added support for SpiNNaker packets and RETINA visualisation application
// v10.1 - 10th Oct. Full screen mode 'f' toggle added (takes away all borders/menus/titles etc).
// v10.0 - 6th Oct.  Changed pause button to ", colour changing to numerics not cursors,  added 'r'aster request functionality to Rate Plot model used with cursors.
// v9.0 -  5th Oct.  Incorporate Spike Rate Plot Option, variable parameters for sdpsend, spike rate plot, keepalives removed per LAP
// v8.1 -  4th Oct.  Added keepalive packets (every 30s) - per LAP/ST - see idleFunction().
// v8.0 -  4th Oct.  Numbers '#' option available for histogram too.  Colour maps now #defines.
// v7.0 -  3rd Oct.  Added Histogram option for plotting - use 't' for tiled. 'h' for histogram and 'i' for interpolated mode
//                     Changed default ratio, changed co-ordinates to 0:7 on both axes, values text changes colour with intensity
// v6.0 -  3rd Oct.  Add in option for plotting of values - toggles with the '#' key, off at start.
// v5.0 -  3rd Oct.  FIX E/W transposition,  Fixed low water plotting bug
// v4.0 -  Sep2011.  Resizable.
//
