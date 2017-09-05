#ifndef VIS_STATE_H
#define VIS_STATE_H

/* -------------------------------------------------------------------------
 * Initialise global variables, and to set some sensible defaults (can all
 * be overriden in visparam.ini)
 *
 * USERS SHOULD NOT EDIT THIS SECTION - use visparam.ini please!
 */

// params below set this to a 48-chip HEATDEMO

static char TITLE[255];			// title used at the top of the screen

static unsigned XDIMENSIONS = 32, YDIMENSIONS = 32,
	EACHCHIPX = 4, EACHCHIPY = 4;	// canvas size defaults
// defaults to a chipwise display
static unsigned XCHIPS = XDIMENSIONS / EACHCHIPX, YCHIPS = YDIMENSIONS / EACHCHIPY;

static const float TIMEWINDOW = 3.5;
static unsigned HISTORYSIZE = 3500;	// data set sizes

// defaults for window sizing
static const unsigned WINBORDER = 110, WINHEIGHT = 700, WINWIDTH = 850;
static const unsigned keyWidth = 50;
static unsigned windowBorder = WINBORDER, windowHeight = WINHEIGHT,
	windowWidth = WINWIDTH+keyWidth;// startup for window sizing

static const double HIWATER = 10, LOWATER = 0;// default hi and lo water
static double lowwatermark = HIWATER, highwatermark = LOWATER;

// default to no translations of the data
static unsigned xflip = 0, yflip = 0, vectorflip = 0, rotateflip = 0;

static unsigned MAXFRAMERATE = 25;	// graphics frame updates
static unsigned short SDPPORT = 17894;	// which UDP port are we expecting our SDP traffic on
static unsigned short FIXEDPOINT = 16;	// number of bits in word of data that are to the right of
					// the decimal place
static float FixedPointFactor;		// multiplier created from FIXEDPOINT value, above
static double ALTERSTEPSIZE = 1.0;	// the step size used when altering the data to send

// ------------------------------------------------------------------------

static float** history_data;		// this stores the historic value the plotted points
					// (double the initial width should be sufficient)
static float** history_data_set2;	// 2nd set of data for ancillary raster plot window
static float* immediate_data;		// this creates a buffer tally for the Ethernet packets
					// (1 ID = one plotted point)

// ------------------------------------------------------------------------

// GLOBAL VARIABLES, my bad.
static char plotvaluesinblocks = 0;	// set non-zero if you just want the coloured blocks with
					// values, (tiles/histogram only)
static char somethingtoplot = 0;	// determines when we should update the screen (no point
					// in plotting no change eh?)
static char freezedisplay = 0;		// whether we should pause the display updates (and send a
					// pause packet to the sim)
static int64_t freezetime;		// when pausing the simulation we hold time at the time of
					// pausing (for screen display purposes)
static const unsigned boxsize = 40, gap = 5;	// used for button creation and gaps between these
						// boxes and the edge of the screen
static unsigned xdim;// = XDIMENSIONS;	// number of items to plot in the x dimension
static unsigned ydim;// = YDIMENSIONS;	// number of items to plot in the y dimension
static unsigned plotWidth, printlabels;

static int fullscreen = 0;		// toggles to get rid of menus/labels/axes/key/controls etc
static int oldwindowBorder = 0;		// used as border disappears when going full-screen
static int gridlines = 0;		// toggles gridlines, starts off

static int RHMouseMenu = 0;		// used for menu generation/regeneration.
static char needtorebuildmenu = 0;	// if a menu is open we can't reconfigure it. So we queue
					// the request.
static char menuopen = 0;		// a callback populates this as 1 or 0 depened on whether
					// a menu is open or not.
static char editmode = 1, livebox = -1;	// for user feedback - box selection and whether edit mode
					// is toggled on/off.

// GLOBAL VARIABLES, per visualiser option

// default starting temperatures, and in-flight editing values for the 4 edges
static float alternorth = 40.0, altereast = 10.0, altersouth = 10.0,
	alterwest = 40.0;
static const unsigned controlboxes = 3;	// grid of control boxes to build (3x3)
static unsigned yorigin = gap;		// Base coordinate of where to plot the compass control box
static unsigned xorigin;		// for the control box

static int counter = 0;			// number of times the display loop has been entered
static int64_t printpktgone = 0;	// if set non zero, this is the time the last Eth packet
					// message was sent, idle function checks for 1s before
					// stopping displaying it
static int64_t starttimez, firstreceivetimez = 0; // storage of persistent times in us

static int safelyshutcalls = 0;		// sometimes the routine to close (and free memory) is
					// called > once, this protects

/* -------------------------------------------------------------------------
 * network parameters for the SDP and SpiNNaker protocols
 */

#define MAXBLOCKSIZE	364		// maximum possible Ethernet payload words for a packet-
					// (SpiNN:1500-20-8-18) (SDP:1500-20-8-26)
#define SPINN_HELLO	0x41		// SpiNNaker raw format uses this as a discovery protocol
#define P2P_SPINN_PACKET 0x3A		// P2P SpiNNaker output packets (Stimulus from SpiNNaker
					// to outside world)
#define STIM_IN_SPINN_PACKET 0x49	// P2P SpiNNaker input packets (Stimulus from outside
					// world)
#define MTU		1515		// Maximum size of packet (waaaaaaaaaaay too big, but not
					// a problem here)

// stop alignment in structure: word alignment would be nasty here, byte alignment reqd
#pragma pack(1)

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

#pragma pack()

//global variables for SDP packet receiver
static int sockfd_input, sockfd;
static sdp_msg *scanptr;

// buffer for network packets
static unsigned char buffer_input[MTU];

/* -------------------------------------------------------------------------
 * End of variables for sdp spinnaker packet receiver - some could be local
 * really - but with pthread they may need to be more visible.
 */

#endif //VIS_STATE_H
