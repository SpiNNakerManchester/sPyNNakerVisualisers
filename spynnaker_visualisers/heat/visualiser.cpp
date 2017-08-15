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

#define MAXDATAINT	(65535)
#define MAXDATAFLOAT	(65535.0F)

#define MINDATAINT	(-65535)
#define MINDATAFLOAT	(-65535.0F)

#define NOTDEFINEDINT	(-66666)
#define NOTDEFINEDFLOAT	(-66666.0F)

#include "state.h"

// prototypes for functions below
void error(char *msg);
//void init_udp_server_spinnaker();
void init_sdp_listening(void);
//void* input_thread (void *ptr);
void* input_thread_SDP(void *ptr);
void init_sdp_sender(void);
void sdp_sender(
	unsigned short dest_add,
	unsigned char dest_port,
	unsigned int command,
	unsigned int arg1,
	unsigned int arg2,
	unsigned int arg3,
	unsigned char extrawords,
	...);
void create_new_window(void);
void destroy_new_window(void);
void filemenu(void);
void transformmenu(void);
void modemenu(void);
void colmenu(void);
void mymenu(int value);
void rebuildmenu(void);
void safelyshut(void);
void open_or_close_output_file(void);
int paramload(void);
void finalise_memory(void);
// end of prototypes

static inline int64_t timestamp(void)
{
    struct timeval stopwatchus;

    gettimeofday(&stopwatchus, NULL);			// grab current time
    return (1000000 * (int64_t) stopwatchus.tv_sec)
	    + (int64_t) stopwatchus.tv_usec;		// get time now in us
}

#include "sdp.cpp"

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

void readmappings(const char* filenamea, const char* filenameb)
{
    size_t i, j, k;
    char buffer[BUFSIZ], *ptr;

#define ARRAY_LENGTH(ary)	(sizeof(*(ary)) / sizeof(**(ary)))
    FILE *filea = fopen(filenamea, "r");
    if (!filea) {
	perror(filenamea);
	exit(1);
    }
    for (i = 0; fgets(buffer, sizeof buffer, filea) ; ++i) {
	for (j = 0, ptr = buffer; j < ARRAY_LENGTH(maplocaltoglobal) ;
		++j, ++ptr) {
	    if (i < XDIMENSIONS * YDIMENSIONS) {
		maplocaltoglobal[i][j] = int(strtol(ptr, &ptr, 10));
		maplocaltoglobalsize = i;
	    }
	}
    }
    fclose(filea);

    FILE *fileb = fopen(filenameb, "r");
    if (!fileb) {
	perror(filenameb);
	exit(1);
    }
    for (i = 0; fgets(buffer, sizeof buffer, fileb) ; ++i) {
	for (j = 0, ptr = buffer; j < ARRAY_LENGTH(mapglobaltolocal) ;
		++j, ++ptr) {
	    if (i < XDIMENSIONS * YDIMENSIONS) {
		mapglobaltolocal[i][j] = int(strtol(ptr, &ptr, 10));
		mapglobaltolocalsize = i;
	    }
	}
    }
    fclose(fileb);

    for (j = 0; j <= mapglobaltolocalsize ; ++j) {
	printf("mapglobaltolocal[%lu]: ", (long unsigned) j);
	for (k = 0; k < ARRAY_LENGTH(mapglobaltolocal) ; ++k) {
	    printf("%4d ", mapglobaltolocal[j][k]);
	}
	putchar('\n');
    }
    for (j = 0; j <= maplocaltoglobalsize ; ++j) {
	printf("maplocaltoglobal[%lu]: ", (long unsigned) j);
	for (k = 0; k < ARRAY_LENGTH(maplocaltoglobal) ; ++k) {
	    printf("%4d ", maplocaltoglobal[j][k]);
	}
	putchar('\n');
    }
#undef ARRAY_LENGTH
}

void* load_stimulus_data_from_file(void *ptr)
{
    use(ptr);
    int64_t nowtime, filestarttime = -1, howlongtowait;	// for timings
    struct timespec ts; // used for calculating how long to wait for next frame
    int numbytes;
    short fromfilelenproto; // allocate new heap memory for a buffer for reading up to 100k packets in
    int64_t fromfileoffsetproto; // allocate new heap memory for a buffer for reading up to 100k packets in
    sdp_msg fromfileproto; // allocate new heap memory for a buffer for reading up to 100k packets in
    uint numberofpackets = 0;
    int64_t startimer = -1, endtimer = -1;

    printf("\nChecking File Length...\n");

    while (fread(&fromfilelenproto, sizeof fromfilelenproto, 1, fileinput)) {
	fread(&fromfileoffsetproto, sizeof fromfileoffsetproto, 1, fileinput);
	if (startimer == -1) {
	    startimer = fromfileoffsetproto;
	}
	if (endtimer < fromfileoffsetproto) {
	    endtimer = fromfileoffsetproto;
	}
	fread(&fromfileproto, fromfilelenproto, 1, fileinput);
	numberofpackets++;
    }

    fseek(fileinput, 0, SEEK_SET);	// reset position
    printf("Detected: %d packets in input file over %3.1fs. "
	    "Allocating memory and loading...\n", numberofpackets - 1,
	    float(endtimer - startimer) / 1000000.0);

    unsigned buffsize = 100000;	// max number of packets to load each time
    if (numberofpackets < buffsize) {
	buffsize = numberofpackets;// size for the number of packets we have
    }

    short *fromfilelen = new short[buffsize]; // allocate new heap memory for a buffer for reading packets into
    int64_t *fromfileoffset = new int64_t[buffsize]; // allocate new heap memory for a buffer for reading packets into
    sdp_msg *fromfile = new sdp_msg[buffsize]; // allocate new heap memory for a buffer for reading packets into
    printf("Memory Chunk Allocated:..*%d. Now transmitting...\n", buffsize);

    int stilltosend = numberofpackets - 1;  // keep a tally of how many to go!
    int desync_count = 0; // work out if we are keeping up, if we fall 1+ sec behind on playback print an inaccuracy warning.
    while (stilltosend > 0) {
	int chunktosend = min(100000, stilltosend);
	for (int i = 0 ; i < chunktosend ; i++) {
	    fread(&fromfilelen[i], sizeof fromfilelen[i], 1, fileinput);
	    fread(&fromfileoffset[i], sizeof fromfileoffset[i], 1, fileinput);
	    fread(&fromfile[i], fromfilelen[i], 1, fileinput);
	}

	for (int i = 0 ; i < chunktosend ; i++) {
	    short extradata = fromfilelen[i];
	    int64_t targettime = (int64_t)(
		    fromfileoffset[i] / float(playbackmultiplier));

	    nowtime = timestamp();			// get time now in us

	    if (filestarttime == -1) {
		filestarttime = nowtime - targettime;// if 1st packet then note it's arrival (typically timeoffset==0 for 1st packet)
	    }

	    howlongtowait = (filestarttime + targettime) - nowtime; // how long in us until we need to send the next packet

	    if (howlongtowait > 0) {
		ts.tv_sec = howlongtowait / 1000000;		// # seconds
		ts.tv_nsec = (howlongtowait % 1000000) * 1000;// us * 1000 = nano secs
		nanosleep(&ts, NULL); // if we are ahead of schedule sleep for a bit
	    }

	    // if we fall more than 1sec behind where we should be
	    if (howlongtowait < -1000000 && desync_count++ == 0) {
		printf("\n\n\n***** Warning having trouble keeping up - "
			"times may be inaccurate *****\n");
	    }

	    if (spinnakerboardipset != 0) { // if we don't know where to send don't send!
		numbytes = sendto(sockfd, &fromfile[i], extradata, 0,
			p->ai_addr, p->ai_addrlen);
		if (numbytes == -1) {
		    perror("oh dear - we didn't send our data!\n");
		    exit(1);
		}
	    }    // write to the Ethernet (127.0.0.1 and relevant port number)
	}
	stilltosend -= chunktosend; // reduce the number of packets still to send
    }

    fclose(fileinput);  // we've now send all the data,
    fileinput = nullptr;

    delete[] fromfilelen;
    delete[] fromfileoffset;
    delete[] fromfile;    // free up buffer space used

    printf("\nAll packets in the file were sent. Finished.\n\n");
    freezedisplay = 1;
    freezetime = timestamp();			// get time now in us
    return nullptr;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void cleardown(void)
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

#include "display.cpp"
#include "events.cpp"

void myinit(void)
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glColor3f(1.0, 1.0, 1.0);
    glShadeModel (GL_SMOOTH); // permits nice shading between plot points for interpolation if required

    filemenu();
    transformmenu();
    rebuildmenu();
}

#include "menu.cpp"

void safelyshut(void)
{
    // in some circumstances this gets run twice, therefore we check for
    // this (particularly the frees!)
    if (!safelyshutcalls) {
	safelyshutcalls++;	// note that this routine has been run before
	if (spinnakerboardport != 0) {
	    for (int i = 0 ; i < all_desired_chips() ; i++) {
		// send exit packet out if we are interacting
		send_to_chip(i, 0x21, 0, 0, 0, 0, 4, 0, 0, 0, 0);
	    }
	}

	open_or_close_output_file();	// Close down any open output file
	if (fileinput != nullptr) {
	    fclose(fileinput);		// deal with input file
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
    windowToUpdate = glutCreateWindow(
	    "VisRT - plotting your network data in real time");

    myinit();

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

void open_or_close_output_file(void)
{
    if (fileoutput == nullptr) {// If file isn't already open, so this is a request to open it
	time_t rawtime;
	time(&rawtime);
	struct tm * timeinfo = localtime(&rawtime);
	char filename[80];

	if (outputfileformat == 2) {		//SAVE AS NEUROTOOLS FORMAT
	    strftime(filename, 80, "packets-%Y%b%d_%H%M.neuro", timeinfo);
	    printf("Saving spike packets in this file:\n\t%s\n", filename);
	    fileoutput = fopen(filename, "w");
	    fprintf(fileoutput, "# first_id =          \n");// ! writing header for neurotools format file
	    fprintf(fileoutput, "# n =          \n");// ! writing header for neurotools format file
	    fprintf(fileoutput, "# dt = 1.0\n");// ! writing header for neurotools format file
	    fprintf(fileoutput, "# dimensions = [          ]\n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# last_id =          \n"); // ! writing header for neurotools format file
	} else if (outputfileformat == 1) { //SAVE AS SPINN (UDP Payload) FORMAT
	    strftime(filename, 80, "packets-%Y%b%d_%H%M.spinn", timeinfo);
	    printf("Saving all input data in this file:\n\t%s\n", filename);
	    fileoutput = fopen(filename, "wb");
	}
    } else {			// File is open already, so we need to close
	if (outputfileformat == 2) {	// File was in neurotools format
	    do {
	    } while (writingtofile == 1);// busy wait for file to finish being updated if in-flight
	    writingtofile = 2;// stop anybody else writing the file, pause further updating

	    fseek(fileoutput, 13, SEEK_SET);		// pos 13 First ID
	    fprintf(fileoutput, "%d", minneuridrx);// write lowest detected neurid
	    fseek(fileoutput, 29, SEEK_SET);// pos 29 n number of neurons-1
	    fprintf(fileoutput, "%d", maxneuridrx - minneuridrx); // write number of neurons in range
	    fseek(fileoutput, 67, SEEK_SET); // pos 67 dimensions number of neurons-1
	    fprintf(fileoutput, "%d", maxneuridrx - minneuridrx); // write number of neurons in range
	    fseek(fileoutput, 90, SEEK_SET);		// pos 90 Last ID
	    fprintf(fileoutput, "%d", maxneuridrx);// write highest detected neurid
	}
	fflush(fileoutput);
	fclose(fileoutput);
	printf("File Save Completed\n");
	fileoutput = nullptr;
	outputfileformat = 0;
	writingtofile = 0;
    }
}

#include "config.cpp"

int main(int argc, char **argv)
{
    // read and check the command line arguments
    const char *configfn, *replayfn, *l2gfn, *g2lfn;
    float replayspeed = 1.0;

    parse_arguments(argc, argv, configfn, replayfn, l2gfn, g2lfn,
	    replayspeed);

    if (configfn == nullptr) {
	// default filename if not supplied by the user
	configfn = "visparam.ini";
    }

    // recover the parameters from the file used to configure this visualisation
    paramload(configfn);

    if (l2gfn != nullptr && g2lfn != nullptr) {	// if both translations are provided
	readmappings(l2gfn, g2lfn);	// read mappings file into array
    }

    cleardown(); // reset the plot buffer to something sensible (i.e. 0 to start with)
    starttimez = timestamp();
    keepalivetime = starttimez;

    for (unsigned j = 0 ; j < HISTORYSIZE ; j++) {
	for (unsigned i = 0 ; i < xdim * ydim ; i++) {
	    history_data[j][i] = NOTDEFINEDFLOAT;
	}
    }

    if (replayfn == nullptr) {
	fprintf(stderr, "No Input File provided. "
		"Using Ethernet Frames Only\n");
    } else {
	playbackmultiplier = replayspeed;
	if (playbackmultiplier == 0.0) {
	    playbackmultiplier = 1.0;	// if mis-understood set to default 1
	}
	playbackmultiplier = clamp(0.01f, playbackmultiplier, 100.0f);
	if (playbackmultiplier != 1.0) {
	    printf("    Requested Playback speed will be at %3.2f rate.\n",
		    playbackmultiplier);
	}
	fileinput = fopen(replayfn, "rb");
	if (fileinput == nullptr) {
	    fprintf(stderr, "cannot read replay file \"%s\"\n", replayfn);
	    exit(2);
	}  // check if file is readable

	// setup with target 127.0.0.1 on right port if not already set
	if (!spinnakerboardipset) {
	    inet_aton("127.0.0.1", &spinnakerboardip);
	}
	spinnakerboardport = SDPPORT;	// SDPPORT is used for outgoing cnnx
	spinnakerboardipset++;
	init_sdp_sender();
	printf("Set up to receive internally from %s on port: %d\n",
		inet_ntoa(spinnakerboardip), SDPPORT);
	// Launch the file receiver
	start_thread(load_stimulus_data_from_file);
    }

    // this sets up the thread that can come back to here from type
    init_sdp_listening();//initialization of the port for receiving SDP frames
    start_thread(input_thread_SDP);	// away the SDP network receiver goes

    run_GUI(argc, argv);		// Initialise and run the GUI
    printf("goodbye");
    return 0;
}
