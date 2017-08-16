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
// -------------------------------------------------------------------------
//  Select your simulation via a specific or visparam.ini file in the local
//  directory
// -------------------------------------------------------------------------
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

// -------------------------------------------------------------------------
// This section checks to see whether the compilation is on a 32- or a 64-bit architecture.

#if defined(__i386__)
#define MACHINEBITS 32
#elif defined(__x86_64__)
#define MACHINEBITS 64
#else
#define MACHINEBITS 0
#endif

// -------------------------------------------------------------------------
// Macro to make a variable be used even when it otherwise wouldn't be
#ifndef use
#define use(x)	do {} while ((x)!=(x))
#endif

// -------------------------------------------------------------------------
// Key enum types

enum directionbox_t {
    // which box in the grid is used for each edge + middle, Ordered:  BtmR->TopR .. BtmL->TopR
    NORTH = 5,
    EAST = 1,
    SOUTH = 3,
    WEST = 7,
    CENTRE = 4
};

// -------------------------------------------------------------------------
// These defines used to represent max/min and invalid data values for
//    checking out of range etc. As data is typically all floating point,
//    these INT versions are rarely (if ever) used.

static const float MAXDATAFLOAT	= 65535;
static const float MINDATAFLOAT	= -65535;
static const float NOTDEFINEDFLOAT = -66666;

#include "state.h"

// prototypes for functions below
static int init_sdp_listening(void);
static void* input_thread_SDP(void *ptr);
static int init_sdp_sender(void);
static void sdp_sender(
	unsigned short dest_add,
	unsigned char dest_port,
	unsigned int command,
	unsigned int arg1,
	unsigned int arg2,
	unsigned int arg3,
	unsigned char extrawords,
	...);
static void rebuildmenu(void);
static void safelyshut(void);
static void finalise_memory(void);

//-------------------------------------------------------------------

static inline int64_t timestamp(void)
{
    struct timeval stopwatchus;

    gettimeofday(&stopwatchus, NULL);			// grab current time
    return (1000000 * (int64_t) stopwatchus.tv_sec)
	    + (int64_t) stopwatchus.tv_usec;		// get time now in us
}

template<class ...Ts>
static inline void send_to_chip(
	int id,
	unsigned char port,
	unsigned int command,
	Ts ... args)
{
    unsigned short x = id / (XDIMENSIONS / EACHCHIPX);
    unsigned short y = id % (YDIMENSIONS / EACHCHIPY);
    unsigned short dest = 256 * x + y;
    sdp_sender(dest, port, command, args...);
}

static inline int all_desired_chips(void)
{
    // if messages need to go to all chips, flip the #def below
#ifndef SEND_TO_ALL_CHIPS
    return 1;
#else
    return (XDIMENSIONS * YDIMENSIONS) / (EACHCHIPX * EACHCHIPY);
#endif // SEND_TO_ALL_CHIPS
}

template<typename T>
static inline T clamp(T low, T value, T high)
{
    if (value < low) {
	return low;
    }
    if (value > high) {
	return high;
    }
    return value;
}

template<typename T>
static inline T rand(T limit)
{
    // Bleah!
    double random_number = double(rand()) / RAND_MAX;
    return (T) (limit * random_number);
}

template<typename T>
static inline T rand(T low, T high)
{
    return low + rand(high - low);
}

static inline void glRectVertices(float x1, float y1, float x2, float y2)
{
    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);
}

static inline void glOpenBoxVertices(float x1, float y1, float x2, float y2)
{
    glVertex2f(x1, y1);
    glVertex2f(x1, y2);
    glVertex2f(x2, y2);
    glVertex2f(x2, y1);
}

static inline bool is_defined(float f)
{
    return f > NOTDEFINEDFLOAT + 1;
}

static inline void trigger_display_refresh(void)
{
    somethingtoplot = 1;
}

enum ui_colors_t {
    BLACK, WHITE, RED, GREEN, CYAN, GREY
};

static inline void color(float r, float g, float b)
{
    glColor4f(r, g, b, 1.0);
}

static inline void color(ui_colors_t colour_name)
{
    switch (colour_name) {
    case BLACK:
	color(0.0, 0.0, 0.0);
	break;
    case WHITE:
	color(1.0, 1.0, 1.0);
	break;
    case RED:
	color(1.0, 0.0, 0.0);
	break;
    case GREEN:
	color(0.0, 0.6, 0.0);
	break;
    case CYAN:
	color(0.0, 1.0, 1.0);
	break;
    case GREY:
	color(0.8, 0.8, 0.8);
	break;
    }
}

template<typename T>
static inline void start_thread(T *fun)
{
    pthread_t pt;
    pthread_create(&pt, NULL, fun, NULL);
}

static inline void set_heatmap_cell(
	int id,
	float north,
	float east,
	float south,
	float west)
{
    send_to_chip(id, 0x21, 1, 0, 0, 0, 4, int(north * 65536),
	    int(east * 65536), int(south * 65536), int(west * 65536));
}

//-------------------------------------------------------------------

static void cleardown(void)
{
    for (unsigned i = 0 ; i < xdim * ydim ; i++) {
	immediate_data[i] = NOTDEFINEDFLOAT;
    }
    // reset for auto-scaling of plot colours, can dynamically alter these
    // value (255.0 = top of the shop)
    highwatermark = HIWATER;
    lowwatermark = LOWATER;
    xflip = 0;
    yflip = 0;
    vectorflip = 0;
    rotateflip = 0;
}

#include "sdp.cpp"
#include "display.cpp"
#include "events.cpp"
#include "menu.cpp"
#include "config.cpp"

static void safelyshut(void)
{
    // in some circumstances this gets run twice, therefore we check for
    // this (particularly the frees!)
    if (!safelyshutcalls) {
	safelyshutcalls = 0;	// note that this routine has been run before
	if (is_board_port_set()) {
	    for (int i = 0 ; i < all_desired_chips() ; i++) {
		// send exit packet out if we are interacting
		send_to_chip(i, 0x21, 0, 0, 0, 0, 4, 0, 0, 0, 0);
	    }
	}

	finalise_memory();
    }
    exit(0);				// kill program dead
}

static inline void run_GUI(int argc, char **argv)
{
    glutInit(&argc, argv);		// Initialise OpenGL

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(windowWidth + keyWidth, windowHeight);
    glutInitWindowPosition(0, 100);
    glutCreateWindow("VisRT - plotting your network data in real time");

    glClearColor(0.0, 0.0, 0.0, 1.0);
    color(WHITE);
    glShadeModel (GL_SMOOTH); // permits nice shading between plot points for interpolation if required

    rebuildmenu();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutIdleFunc(idleFunction);
    glutKeyboardFunc(keyDown);
    glutMouseFunc(mousehandler);
    // register what to do when the use kills the window via the frame object
    glutCloseFunc(safelyshut);
    // this keeps an eye on whether a window is open (as can't alter when open!)
    glutMenuStatusFunc(logifmenuopen);

    glutMainLoop();			// Enter the main OpenGL loop
}

int main(int argc, char **argv)
{
    const char *configfn;

    // read and check the command line arguments
    parse_arguments(argc, argv, configfn);
    if (configfn == nullptr) {
	// default filename if not supplied by the user
	configfn = "visparam.ini";
    }

    // recover the parameters from the file used to configure this visualisation
    paramload(configfn);

    cleardown(); // reset the plot buffer to something sensible (i.e. 0 to start with)
    starttimez = timestamp();

    for (unsigned j = 0 ; j < HISTORYSIZE ; j++) {
	for (unsigned i = 0 ; i < xdim * ydim ; i++) {
	    history_data[j][i] = NOTDEFINEDFLOAT;
	}
    }

    // this sets up the thread that can come back to here from type
    init_sdp_listening();//initialization of the port for receiving SDP frames
    start_thread(input_thread_SDP);	// away the SDP network receiver goes

    run_GUI(argc, argv);		// Initialise and run the GUI
    printf("goodbye");
    return 0;
}
