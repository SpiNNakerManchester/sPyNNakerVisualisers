#ifndef VIS_STATE_H
#define VIS_STATE_H

// which visualiser option
enum visopt_t {
    HEATMAP = 1,
    RATEPLOT,
    RETINA,
    INTEGRATORFG,
    RATEPLOTLEGACY,
    MAR12RASTER,
    SEVILLERETINA,
    LINKCHECK,
    SPIKERVC,
    CHIPTEMP,
    CPUUTIL,
    RETINA2,
    COCHLEA
};

// different colour maps available
enum colormaps_t {
    MULTI = 1,
    GREYS,
    REDS,
    GREENS,
    BLUES,
    THERMAL,
    RED,
    BLUE
};

// view mode
enum viewmodes_t {
    TILED = 1,
    INTERPOLATED,
    HISTOGRAM,
    LINES,
    RASTER,
    EEGSTYLE
};


/* --------------------------------------------------------------------------------------------------
 * Initialise global variables, and to set some sensible defaults (can all
 * be overriden in visparam.ini)
 *
 * USERS SHOULD NOT EDIT THIS SECTION - use visparam.ini please!
 */

int SIMULATION = HEATMAP;			// defaults to HEATDEMO
// params below set this to a 48-chip HEATDEMO

char TITLE[50];					// title used at the top of the screen

int STARTMODE = TILED, STARTCOLOUR = MULTI;	// start with requested mode (default tiled, multicolour)
int displaymode = STARTMODE, colourused = STARTCOLOUR;
int BLACKBACKGROUND = 0;
int INITZERO = 0;
int XDIMENSIONS = 32, YDIMENSIONS = 32,
	EACHCHIPX = 4, EACHCHIPY = 4;		// canvas size defaults
int XCHIPS = (XDIMENSIONS/EACHCHIPX),
	YCHIPS = (XDIMENSIONS/EACHCHIPX);	// defaults to a chipwise display

double TIMEWINDOW = 3.5;			// default time width of a window
float displayWindow = TIMEWINDOW;
int HISTORYSIZE = 3500,
	MAXRASTERISEDNEURONS = 1024;		// data set sizes

int WINBORDER = 110, WINHEIGHT = 700, WINWIDTH = 850; // defaults for window sizing
int DISPLAYKEY = 1, KEYWIDTH = 50;
int DISPLAYXLABELS = 1, DISPLAYYLABELS = 1;	// gives options to suspend printing of x and y axis labels if required
int keyWidth = KEYWIDTH;
int windowBorder = WINBORDER, windowHeight = WINHEIGHT,
	windowWidth = WINWIDTH+keyWidth;	// startup for window sizing

double HIWATER = 10, LOWATER = 0;		// default hi and lo water
double lowwatermark = HIWATER, highwatermark = LOWATER;
int DYNAMICSCALE = 1;				// default to permit dynamic scaling
int PERCENTAGESCALE = 0;			// set this to 1 if you don't care about values, only relative %ages

int LABELBYCHIP = 0;				// just print one label per chip and not sub-chip/core
int PLAYPAUSEXIT = 1, DISPLAYMINIPLOT = 1, INTERACTION = 1; // options for screen output

int MAXFRAMERATE = 25, PLOTONLYONDEMAND = 0;	// graphics frame updates

int XFLIP = 0, YFLIP = 0, VECTORFLIP = 0, ROTATEFLIP = 0; // default to no translations of the data
int xflip = XFLIP, yflip = YFLIP, vectorflip = VECTORFLIP, rotateflip = ROTATEFLIP;

int SDPPORT = 17894;				// which UDP port are we expecting our SDP traffic on

int FIXEDPOINT = 16;				// number of bits in word of data that are to the right of the decimal place

long int BITSOFPOPID = 0;			// number of bits of population in each core (pow of 2); 0 for implicit core==popID

double ALTERSTEPSIZE = 1.0;			// the step size used when altering the data to send
double DECAYPROPORTION = 0.0;			// how quickly does the raster plot diminish


//float* immediate_data;	// this stores the value of each plotted point data (time == now)  - superfluous
float** history_data;		// this stores the historic value the plotted points (double the initial width should be sufficient)
float** history_data_set2;	// 2nd set of data for ancillary raster plot window
float* immediate_data;		// this creates a buffer tally for the Ethernet packets (1 ID = one plotted point)

int** maplocaltoglobal;		// always 2 wide.  Size of mapping from X&Y coords #of pops
int** mapglobaltolocal;		// and the reverse from the 2nd file


// GLOBAL VARIABLES, my bad.
char plotvaluesinblocks = 0;	// set non-zero if you just want the coloured blocks with values, (tiles/histogram only)
char somethingtoplot = 0;	// determines when we should update the screen (no point in plotting no change eh?)
char freezedisplay = 0;		// whether we should pause the display updates (and send a pause packet to the sim)
int64_t freezetime;		// when pausing the simulation we hold time at the time of pausing (for screen display purposes)
int boxsize = 40, gap = 5;	// used for button creation and gaps between these boxes and the edge of the screen
int win1 = 0;			// the main graphics window used by OpenGL
int win2 = 0;			// a test for windows spawned from the main window
int windowToUpdate;		// used to know which window to update
int xdim;// = XDIMENSIONS;	// number of items to plot in the x dimension
int ydim;// = YDIMENSIONS;	// number of items to plot in the y dimension
int plotWidth, printlabels;

int fullscreen = 0;		// toggles to get rid of menus/labels/axes/key/controls etc.
int oldwindowBorder = 0;	// used as border disappears when going full-screen
int gridlines = 0;		// toggles gridlines, starts off

int RHMouseMenu = 0;		// used for menu generation/regeneration.
int modesubmenu = 0;		// for mode submenu
int coloursubmenu = 0;		// for colours submenu
int transformsubmenu = 0;	// for plot transformation submenu
int filesubmenu = 0;		// for save file submenu
char needtorebuildmenu = 0;	// if a menu is open we can't reconfigure it. So we queue the request.
char menuopen = 0;		// a callback populates this as 1 or 0 depened on whether a menu is open or not.
char editmode = 1, livebox = -1;// for user feedback - box selection and whether edit mode is toggled on/off.

// GLOBAL VARIABLES, per visualiser option

// SIMULATION == HEATMAP
float alternorth = 40.0, altereast = 10.0, altersouth = 10.0,
	alterwest = 40.0;	// default starting temperatures, and in-flight editing values for the 4 edges
int controlboxes = 3;		// grid of control boxes to build (3x3)
char NORTH = 5, EAST = 1, SOUTH = 3, WEST = 7,
	CENTRE = 4;		// which box in the grid is used for each edge + middle, Ordered:  BtmR->TopR .. BtmL->TopR
int yorigin = gap;		// Base coordinate of where to plot the compass control box
int xorigin;			// for the control box
//int xorigin=(windowWidth+keyWidth)-(controlboxes*(boxsize+gap));    // for the control box
// CP made dynamic


// SIMULATION == RATEPLOT or RATEPLOTLEGACY
float biasediting;		// keeps the transient biascurrent as it's edited so it can be sent
int rasterpopulation = -1;	// keeps the population ID that we're running rasters on (-1 if not in use)
float* biascurrent;		// array used to know what we're starting from when adjusting
//float biascurrent[XDIMENSIONS*YDIMENSIONS];    // used to know what we're starting from when adjusting
//CP made dynamic


// SIMULATION == RETINA2
int N_PER_PROC;
int ID_OFFSET;

// FILE OPERATIONS
float playbackmultiplier = 1.0;	// when using a recorded input file, 1=realtime, 0.25=quarter speed, 15=15x speed (specified by optional CLI argument)
FILE *fileinput = NULL;		// if the user chooses to provide data as input this is the handle

char outputfileformat = 0;	// 3 states. 0 = no writing, 1=.spinn UDP payload format, 2 = neurotools format
volatile char writingtofile = 0;// 3 states.  1=busy writing, 2=paused, 0=not paused, not busy.
FILE *fileoutput = NULL;


volatile int mappingfilesread = 0;
int maplocaltoglobalsize, mapglobaltolocalsize;	// logs how bug each array actually gets (might not be full!)


int lasthistorylineupdated = 0;	// this is stored so that rows that have not been updated between then and now can be cleared out

int counter = 0;		// number of times the display loop has been entered
int pktcount = 0;		// total aggregate of packets received and processed
int64_t printpktgone = 0;	// if set non zero, this is the time the last Eth packet message was sent, idle function checks for 1s before stopping displaying it
struct timeval startimeus;	// for retrieval of the time in us at the start of the simulation
int64_t starttimez, firstreceivetimez = 0; // storage of persistent times in us
int64_t keepalivetime;		// used by code to send out a packet every few seconds to keep ARP entries alive
unsigned int minneuridrx = 10000000; // we only need to raster plot the number of neurons firing in a raster plot, (smallest neurid received).
unsigned int maxneuridrx = 0;	// we only need to raster plot the number of neurons firing in a raster plot, (largest neurid received).

int safelyshutcalls = 0;	// sometimes the routine to close (and free memory) is called > once, this protects

/* --------------------------------------------------------------------------------------------------
 * network parameters for the SDP and SpiNNaker protocols
 */

#define MAXBLOCKSIZE	364	// maximum possible Ethernet payload words for a packet- (SpiNN:1500-20-8-18) (SDP:1500-20-8-26)
#define SPINN_HELLO	0x41	// SpiNNaker raw format uses this as a discovery protocol
#define P2P_SPINN_PACKET 0x3A	// P2P SpiNNaker output packets (Stimulus from SpiNNaker to outside world)
#define STIM_IN_SPINN_PACKET 0x49 // P2P SpiNNaker input packets (Stimulus from outside world)

#pragma pack(1)			// stop alignment in structure: word alignment would be nasty here, byte alignment reqd

struct spinnpacket {
   unsigned short version;
   unsigned int cmd_rc;
   unsigned int arg1;
   unsigned int arg2;
   unsigned int arg3;
   unsigned int data[MAXBLOCKSIZE];
};     // a structure that holds SpiNNaker packet data (inside UDP segment)

struct sdp_msg {		// SDP message (<=292 bytes)
   unsigned char ip_time_out;
   unsigned char pad;
   // sdp_hdr_t
   unsigned char flags;		// SDP flag byte
   unsigned char tag;		// SDP IPtag
   unsigned char dest_port;	// SDP destination port
   unsigned char srce_port;	// SDP source port
   unsigned short dest_addr;	// SDP destination address
   unsigned short srce_addr;	// SDP source address
   // cmd_hdr_t (optional, but tends to be there!)
   unsigned short cmd_rc;	// Command/Return Code
   unsigned short seq;		// seq (new per ST email 27th Oct 2011)
   unsigned int arg1;		// Arg 1
   unsigned int arg2;		// Arg 2
   unsigned int arg3;		// Arg 3
   // user data (optional)
   unsigned int data[MAXBLOCKSIZE]; // User data (256 bytes)
};

struct spinnaker_saved_file_t {
   int64_t filesimtimeoffset;
   short incoming_packet_size;
   unsigned char payload[];
};

#pragma pack()

//global variables for SDP packet receiver
int sockfd_input, sockfd;
char portno_input[6];
struct addrinfo hints_input, hints_output, *servinfo_input, *p_input,
    *servinfo, *p;
struct sockaddr_storage their_addr_input;
int rv_input;
int numbytes_input;
struct sdp_msg *scanptr;
struct spinnpacket *scanptrspinn;
in_addr spinnakerboardip;
int spinnakerboardport = 0;
char spinnakerboardipset = 0;
#define MTU	1515		// Maximum size of packet
unsigned char buffer_input[MTU]; // buffer for network packets (waaaaaaaaaaay too big, but not a problem here)

/* --------------------------------------------------------------------------------------------------
 * End of variables for sdp spinnaker packet receiver - some could be local
 * really - but with pthread they may need to be more visible.
 */

#endif //VIS_STATE_H
