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

#include "state.h"

// prototypes for functions below
void error(char *msg);
//void init_udp_server_spinnaker();
void init_sdp_listening(void);
//void* input_thread (void *ptr);
void* input_thread_SDP(void *ptr);
void init_sdp_sender(void);
// void sdp_sender(unsigned short dest_add, unsigned int command, unsigned int arg1, unsigned int arg2, unsigned int arg3, float north, float east, float south, float west);
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
void display_win2(void);
void filemenu(void);
void transformmenu(void);
void modemenu(void);
void colmenu(void);
void mymenu(int value);
void rebuildmenu(void);
void safelyshut(void);
void open_or_close_output_file(void);
int paramload(void);
// end of prototypes

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
    sdp_sender(256 * x + y, port, command, &args...);
}

static inline int64_t timestamp(void)
{
    struct timeval stopwatchus;

    gettimeofday(&stopwatchus, NULL);			// grab current time
    return (1000000 * (int64_t) stopwatchus.tv_sec)
	    + (int64_t) stopwatchus.tv_usec;		// get time now in us
}

static inline void sleeplet(void)
{
    int rubbish = 0;
    for (int j = 0 ; j < 10000000 ; j++) {
	rubbish++;
    }
}

void readmappings(char* filenamea, char* filenameb)
{
    FILE *filea = fopen(filenamea, "r");
    if (filea) {
	size_t i, j, k;
	char buffer[BUFSIZ], *ptr;
	for (i = 0; fgets(buffer, sizeof buffer, filea) ; ++i) {
	    for (j = 0, ptr = buffer;
		    j < sizeof *maplocaltoglobal / sizeof **maplocaltoglobal ;
		    ++j, ++ptr) {
		if (i < XDIMENSIONS * YDIMENSIONS) {
		    maplocaltoglobal[i][j] = (int) strtol(ptr, &ptr, 10);
		    maplocaltoglobalsize = i;
		}
	    }
	}
    } else {
	perror(filenamea);
    }
    fclose(filea);

    FILE *fileb = fopen(filenameb, "r");
    if (fileb) {
	size_t i, j, k;
	char buffer[BUFSIZ], *ptr;
	for (i = 0; fgets(buffer, sizeof buffer, fileb) ; ++i) {
	    for (j = 0, ptr = buffer;
		    j < sizeof *mapglobaltolocal / sizeof **mapglobaltolocal ;
		    ++j, ++ptr) {
		if (i < XDIMENSIONS * YDIMENSIONS) {
		    mapglobaltolocal[i][j] = (int) strtol(ptr, &ptr, 10);
		    mapglobaltolocalsize = i;
		}
	    }
	}
    } else {
	perror(filenameb);
    }
    fclose(fileb);

    size_t i, j, k;
    for (j = 0; j <= mapglobaltolocalsize ; ++j) {
	printf("mapglobaltolocal[%lu]: ", (long unsigned) j);
	for (k = 0; k < sizeof *mapglobaltolocal / sizeof **mapglobaltolocal ;
		++k) {
	    printf("%4d ", mapglobaltolocal[j][k]);
	}
	putchar('\n');
    }
    for (j = 0; j <= maplocaltoglobalsize ; ++j) {
	printf("maplocaltoglobal[%lu]: ", (long unsigned) j);
	for (k = 0; k < sizeof *maplocaltoglobal / sizeof **maplocaltoglobal ;
		++k) {
	    printf("%4d ", maplocaltoglobal[j][k]);
	}
	putchar('\n');
    }
}

void* load_stimulus_data_from_file(void *ptr)
{
    int64_t sincefirstpacket, nowtime, filestarttime = -1;
    int64_t howlongrunning, howlongtowait;                // for timings
    struct timespec ts; // used for calculating how long to wait for next frame
    int numbytes;
    char sdp_header_len = 26;

    short fromfilelenproto; // allocate new heap memory for a buffer for reading up to 100k packets in
    int64_t fromfileoffsetproto; // allocate new heap memory for a buffer for reading up to 100k packets in
    sdp_msg fromfileproto; // allocate new heap memory for a buffer for reading up to 100k packets in

    uint numberofpackets = 0;
    uint filesimtime; // time in ms offset from 1st packet at beginning of file
    int64_t startimer = -1, endtimer = -1;

    printf("\nChecking File Length...\n", numberofpackets - 1);

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

    fseek(fileinput, 0, SEEK_SET);                    // reset position
    printf(
	    "Detected: %d packets in input file over %3.1fs. Allocating memory and loading...\n",
	    numberofpackets - 1, (float) (endtimer - startimer) / 1000000.0);

    int buffsize = 100000;          // max number of packets to load each time
    if (numberofpackets < buffsize) {
	buffsize = numberofpackets;  // size for the number of packets we have
    }

    short *fromfilelen = new short[buffsize]; // allocate new heap memory for a buffer for reading packets into
    int64_t *fromfileoffset = new int64_t[buffsize]; // allocate new heap memory for a buffer for reading packets into
    sdp_msg *fromfile = new sdp_msg[buffsize]; // allocate new heap memory for a buffer for reading packets into
    printf("Memory Chunk Allocated:..*%d. Now transmitting...\n", buffsize);

    int stilltosend = numberofpackets - 1;  // keep a tally of how many to go!
    int keepyuppyproblemo = 0; // work out if we are keeping up, if we fall 1+ sec behind on playback print an inaccuracy warning.
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
		    (float) fromfileoffset[i] / (float) playbackmultiplier);

	    nowtime = timestamp();				// get time now in us

	    if (filestarttime == -1) {
		filestarttime = nowtime - targettime;		// if 1st packet then note it's arrival (typically timeoffset==0 for 1st packet)
	    }

	    howlongtowait = (filestarttime + targettime) - nowtime; // how long in us until we need to send the next packet

	    if (howlongtowait > 0) {
		ts.tv_sec = howlongtowait / 1000000;		// # seconds
		ts.tv_nsec = (howlongtowait % 1000000) * 1000;	// us * 1000 = nano secs
		nanosleep(&ts, NULL); // if we are ahead of schedule sleep for a bit
	    }

	    if (howlongtowait < -1000000 && keepyuppyproblemo++ == 0) {
		printf(
			"\n\n\n***** Warning having trouble keeping up - times may be inaccurate *****\n"); // if we fall more than 1sec behind where we should be
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
    fileinput = NULL;

    delete[] fromfilelen;
    delete[] fromfileoffset;
    delete[] fromfile;    // free up buffer space used

    printf("\nAll packets in the file were sent. Finished.\n\n");
    freezedisplay = 1;
    freezetime = timestamp();			// get time now in us
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void cleardown(void)
{
    for (int i = 0 ; i < (xdim * ydim) ; i++) {
	immediate_data[i] = INITZERO ? 0.0 : NOTDEFINEDFLOAT;
    }
    highwatermark = HIWATER; // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
    lowwatermark = LOWATER; // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
    xflip = XFLIP;
    yflip = YFLIP;
    vectorflip = VECTORFLIP;
    rotateflip = ROTATEFLIP;
}

//-------------------------------------------------------------------------
//  Draws a string at the specified coordinates.
//-------------------------------------------------------------------------
void printgl(float x, float y, void *font_style, char* format, ...)
{
    va_list arg_list;
    char str[256];
    int i;

    /*
     * font options:  GLUT_BITMAP_8_BY_13 GLUT_BITMAP_9_BY_15
     * GLUT_BITMAP_TIMES_ROMAN_10 GLUT_BITMAP_HELVETICA_10
     * GLUT_BITMAP_HELVETICA_12 GLUT_BITMAP_HELVETICA_18
     * GLUT_BITMAP_TIMES_ROMAN_24
     */

    va_start(arg_list, format);
    vsprintf(str, format, arg_list);
    va_end(arg_list);

    glRasterPos2f(x, y);

    for (i = 0; str[i] != '\0' ; i++) {
	glutBitmapCharacter(font_style, str[i]);
    }
}

void printglstroke(
	float x,
	float y,
	float size,
	float rotate,
	char* format,
	...)
{
    va_list arg_list;
    char str[256];
    int i;
    GLvoid *font_style = GLUT_STROKE_ROMAN;

    va_start(arg_list, format);
    vsprintf(str, format, arg_list);
    va_end(arg_list);

    glPushMatrix();
    glEnable (GL_BLEND);   // antialias the font
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_LINE_SMOOTH);
    glLineWidth(1.5);   // end setup for antialiasing
    glTranslatef(x, y, 0);
    glScalef(size, size, size);
    glRotatef(rotate, 0.0, 0.0, 1.0);
    for (i = 0; str[i] != '\0' ; i++) {
	glutStrokeCharacter(GLUT_STROKE_ROMAN, str[i]);
    }
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
    glPopMatrix();
}

void convert_index_to_coord(int index, int *x, int *y)
{
    int elementid = (index % (EACHCHIPX * EACHCHIPY));
    int elementx = elementid / EACHCHIPY;
    int elementy = elementid % EACHCHIPY;
    int tileid = (index / (EACHCHIPX * EACHCHIPY));
    int tilex = tileid / (YDIMENSIONS / EACHCHIPY);
    int tiley = tileid % (YDIMENSIONS / EACHCHIPY);

    int xcord = (tilex * EACHCHIPX) + elementx;
    int ycord = (tiley * EACHCHIPY) + elementy;

    *x = xcord;
    *y = ycord;
}
// call with: convert_index_to_coord(index, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)

int convert_coord_to_index(int x, int y)
{
    int elementx = x % EACHCHIPX;	// x position in tile
    int elementy = y % EACHCHIPY;	// y position in tile
    int tilex = x / EACHCHIPX;		// x tile
    int tiley = y / EACHCHIPY;		// y tile

    int elementid = (elementx * EACHCHIPY) + elementy;
    int index = (EACHCHIPX * EACHCHIPY) * ((tilex * YCHIPS) + tiley)
	    + elementid; // CP 16thJan to support #chips rather than calculated

    return index;
}
// call with: index = convert_coord_to_index(xcoordinate, ycoordinate);

int coordinate_manipulate(int ii)
{
    int i = ii;     // begin with the assumption of no flipping of coordinates
    int xcoordinate, ycoordinate;

    if (yflip || xflip || vectorflip || rotateflip) {
	int elementid = i % (EACHCHIPX * EACHCHIPY);
	int elementx = elementid / EACHCHIPY;
	int elementy = elementid % EACHCHIPY;
	int tileid = i / (EACHCHIPX * EACHCHIPY);
	int tilex = tileid / (YDIMENSIONS / EACHCHIPY);
	int tiley = tileid % (YDIMENSIONS / EACHCHIPY);
	if (yflip) {
	    elementy = (EACHCHIPY - 1) - elementy;        // flip ycords
	    tiley = ((YDIMENSIONS / EACHCHIPY) - 1) - tiley;
	}
	if (xflip) {
	    elementx = (EACHCHIPX - 1) - elementx;        // flip xcoords
	    tilex = ((XDIMENSIONS / EACHCHIPX) - 1) - tilex;
	}

	elementid = (elementx * EACHCHIPY) + elementy;
	i = (EACHCHIPX * EACHCHIPY)
		* ((tilex * (XDIMENSIONS / EACHCHIPX)) + tiley) + elementid;

	if (vectorflip) {
	    i = (YDIMENSIONS * XDIMENSIONS) - 1 - i; // go back to front (cumulative)
	}
	if (rotateflip) {
	    convert_index_to_coord(i, &xcoordinate, &ycoordinate);   // rotate
	    i = convert_coord_to_index(ycoordinate,
		    XDIMENSIONS - 1 - xcoordinate);
	}
    }
    return i;                            // return cumulative reorientation
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

float colour_calculator(float inputty, float hiwater, float lowater)
{
    float scalingfactor = 0;
    float fillcolour = 1.0;
    float diff = hiwater - lowater;

    inputty = clamp(lowater, inputty, hiwater);
    // for truncated plots or if data supplied is out of range.

    if (diff <= 0.0001) {
	fillcolour = 1.0; // if in error, or close to a divide by zero (no intensity plotted)
    } else {
	scalingfactor = 1 / diff; // work out how to scale the input data depending on low and highwater values
	fillcolour = (scalingfactor * (inputty - lowater)); // calculate the colour to plot
    }
    fillcolour = clamp(0.0F, fillcolour, 1.0F);
    // must always range between 0 and 1 floating point

    switch (colourused) {
    case 1: {			// colours option
#define COLOURSTEPS 6    // 6 different RGB colours, Black, Blue, Cyan, Green, Yellow, Red
	int gamut[COLOURSTEPS][3] = { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, {
		0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 } };

	int colourindex = (float) fillcolour * (float) (COLOURSTEPS - 1);
	float colouroffset = (float) (colourindex + 1)
		- (fillcolour * (float) (COLOURSTEPS - 1)); // how far away from higher index (range between 0 and 1).
	float R = ((1 - colouroffset) * gamut[colourindex + 1][0])
		+ (colouroffset * gamut[colourindex][0]);
	float G = ((1 - colouroffset) * gamut[colourindex + 1][1])
		+ (colouroffset * gamut[colourindex][1]);
	float B = ((1 - colouroffset) * gamut[colourindex + 1][2])
		+ (colouroffset * gamut[colourindex][2]);
	//spilt into n sections. specify colours for each section. how far away from top of section is it
	//multiply R,G,B difference by this proportion

	glColor4f(R, G, B, 1.0);
	break;
    }
    case 2:			// greyscales option
	glColor4f(fillcolour, fillcolour, fillcolour, 1.0);
	break;
    case 3:			// redscales only
	glColor4f(fillcolour, 0.0, 0.0, 1.0);
	break;
    case 4:			// greenscales option
	glColor4f(0.0, fillcolour, 0.0, 1.0);
	break;
    case 5:			// bluescales option
	glColor4f(0.0, 0.0, fillcolour, 1.0);
	break;
    case 6: {			// alternate colours option
#define COLOURSTEPSB 5    //  black, purpleymagenta, red, yellow, white (RGB)
	int gamut[COLOURSTEPSB][3] = { { 0, 0, 0 }, { 1, 0, 1 }, { 1, 0, 0 },
		{ 1, 1, 0 }, { 1, 1, 1 } };

	int colourindex = (float) fillcolour * (float) (COLOURSTEPSB - 1);
	float colouroffset = (float) (colourindex + 1)
		- (fillcolour * (float) (COLOURSTEPSB - 1)); // how far away from higher index (range between 0 and 1).
	float R = ((1 - colouroffset) * gamut[colourindex + 1][0])
		+ (colouroffset * gamut[colourindex][0]);
	float G = ((1 - colouroffset) * gamut[colourindex + 1][1])
		+ (colouroffset * gamut[colourindex][1]);
	float B = ((1 - colouroffset) * gamut[colourindex + 1][2])
		+ (colouroffset * gamut[colourindex][2]);

	glColor4f(R, G, B, 1.0);
	break;
    }
    case 7:			// everything is red option except v close to 0 (to get rid of flickery colour in line mode) etc.
	glColor4f(fillcolour < 0.01 ? 0.0 : 1.0, 0.0, 0.0, 1.0);
	break;
    case 8:			// everything is white option except v close to 0 (to get rid of flickery colour in line mode etc.
	glColor4f(0.0, 0.0, fillcolour < 0.01 ? 0.0 : 1.0, 1.0);
	break;
    }

    return fillcolour;
}

void displayb(void)			// not currently used
{
    glLoadIdentity();
    glutSetWindow(windowToUpdate);	// this used to give a blank screen for the 2nd screen when loaded (so it doens't flash scr1)
    glLoadIdentity();
    //glutPostRedisplay();
    glClearColor(0, 0, 0, 1.0);		// background colour - grey surround
    glClear (GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();			// no flickery gfx
}

// display function, called whenever the display window needs redrawing
void display(void)
{

    int64_t nowtime;
    float timeperindex = displayWindow / (float) plotWidth; // time in seconds per history index in use

    glPointSize(1.0);

    counter++;				// how many frames have we plotted in our history

    glLoadIdentity();
    glutSetWindow(windowToUpdate);	// specifically look at our plotting window
    glLoadIdentity();

    //glutPostRedisplay();

    glClearColor(0.8, 0.8, 0.8, 1.0);	// background colour - grey surround
    glClear (GL_COLOR_BUFFER_BIT);

    // if we want a forced black background then clear it now
    if (BLACKBACKGROUND && displaymode == TILED) {
	glColor4f(0.0, 0.0, 0.0, 1.0);	// Black for filling
	glBegin (GL_QUADS);
	glVertex2f(windowBorder, windowBorder);  //btm left
	glVertex2f(windowWidth - windowBorder - keyWidth, windowBorder); //btm right
	glVertex2f(windowWidth - windowBorder - keyWidth,
		windowHeight - windowBorder);  // top right
	glVertex2f(windowBorder, windowHeight - windowBorder); // top left
	glEnd();
    }

    glColor4f(0.0, 0.0, 0.0, 1.0);                    // Black Text for Labels

    if (printlabels && !fullscreen) { // titles and labels are only printed if border is big enough
	printgl((windowWidth / 2) - 200, windowHeight - 50,
		GLUT_BITMAP_TIMES_ROMAN_24, (char*) TITLE); // Print Title of Graph
	char stringy3[] =
		"Colour Map (1,2,3..), Mode: (t)iled, (h)istogram, (i)nterpolation, (l)ines, (r)aster, Menu: right click.";
	printgl((windowWidth / 2) - 250, windowHeight - 80,
		GLUT_BITMAP_HELVETICA_12, stringy3); // Print subtitle of Graph
	// Graph Title

	char stringy4[] = "%d";

	// X Axis
	if (DISPLAYXLABELS) {
	    if (displaymode == HISTOGRAM) {
		char stringy1[] = "Tile (X,Y) Coords";
		printglstroke((windowWidth / 2) - 40, 20, 0.12, 0, stringy1);
	    } else if (displaymode == LINES || displaymode == RASTER
		    || displaymode == EEGSTYLE) {
		char stringy1[] =
			"Simulation Time. Window Width=%3.1f secs ('<' '>')";
		printglstroke((windowWidth / 2) - 200, 20, 0.12, 0, stringy1,
			displayWindow * playbackmultiplier);
	    } else {
		char stringy1[] = "X Coord";
		printglstroke((windowWidth / 2) - 25, 20, 0.12, 0, stringy1);
		int xlabels = xdim;
		float xspacing = ((float) (plotWidth) / xdim);
		if (LABELBYCHIP) {
		    xlabels = xdim / EACHCHIPX;
		    xspacing *= EACHCHIPX;
		}
		int xplotted, spacing = 24, lastxplotted = -100;
		for (int i = 0 ; i < xlabels ; i++) {                // X-Axis
		    if (i > 100)
			spacing = 32;
		    xplotted = (i * xspacing) + windowBorder
			    + ((xspacing - 8) / 2) - 3;	// what will be the next x coordinate
		    if (xplotted > lastxplotted + spacing) { // plot if enough space to not overlap labels.
			printgl(xplotted, 60, GLUT_BITMAP_HELVETICA_18,
				stringy4, i);		// Print X Axis Labels at required intervals
			lastxplotted = xplotted;	// record last x coordinate plotted to
		    }
		}
	    }
	}
	// X Axis

	if (DISPLAYYLABELS) {
	    // Y Axis
	    if (displaymode == HISTOGRAM || displaymode == LINES) {
		glColor4f(0.0, 0.0, 0.0, 1.0);		// Black Text for Labels
		char stringy5[] = "Data Value";
		printglstroke(25, (windowHeight / 2) - 50, 0.12, 90,
			stringy5);			// Print Y-Axis label for Graph
		char stringy8[] = "%.2f";
		if (PERCENTAGESCALE) {
		    char stringy8[] = "%.2f%%";
		    printgl(55, windowHeight - (windowBorder) - 5,
			    GLUT_BITMAP_HELVETICA_12, stringy8, 100.0); // Print HighWaterMark Value
		    printgl(55, windowBorder - 5, GLUT_BITMAP_HELVETICA_12,
			    stringy8, 0.0);		// Print LowWaterMark Value
		} else {
		    printgl(55, windowHeight - (windowBorder) - 5,
			    GLUT_BITMAP_HELVETICA_12, stringy8,
			    highwatermark);       // Print HighWaterMark Value
		    printgl(55, windowBorder - 5, GLUT_BITMAP_HELVETICA_12,
			    stringy8, lowwatermark); // Print LowWaterMark Value
		}
		glColor4f(0.6, 0.6, 0.6, 1.0);
		glLineWidth(1.0);
		glBegin (GL_LINES);
		glVertex2f(windowWidth - windowBorder - keyWidth + 10,
			windowBorder); // rhs
		glVertex2f(windowBorder - 10, windowBorder); // inside
		glEnd();
		float interval, difference = highwatermark - lowwatermark;
		for (float i = 10000 ; i >= 0.1 ; i /= 10.0) {
		    if (difference < i) {
			interval = i / (difference < i / 2 ? 20.0 : 10.0);
		    }
		}
		int multipleprinted = 1;
		float linechunkiness = (float) (windowHeight
			- (2 * windowBorder))
			/ (float) (highwatermark - lowwatermark);
		for (int i = 0 ; i < windowHeight - (2 * windowBorder) ;
			i++) {
		    float temperaturehere = ((float) i / linechunkiness)
			    + lowwatermark;
		    float positiveoffset = temperaturehere - lowwatermark;
		    if (PERCENTAGESCALE)
			positiveoffset = (positiveoffset / difference)
				* 100.0; // scale it to a range of 0-100
		    if (positiveoffset >= (interval * multipleprinted)) {
			glColor4f(0.6, 0.6, 0.6, 1.0);
			glLineWidth(1.0);
			glBegin(GL_LINES);
			glVertex2f(windowWidth - windowBorder - keyWidth + 10,
				i + windowBorder); // rhs
			glVertex2f(windowBorder - 10, i + windowBorder); // inside
			glEnd();
			glColor4f(0.0, 0.0, 0.0, 1.0);
			printgl(55, i + windowBorder - 5,
				GLUT_BITMAP_HELVETICA_12, stringy8,
				lowwatermark + (multipleprinted * interval));
			multipleprinted++;
		    }
		}
	    } else if (displaymode == RASTER || displaymode == EEGSTYLE) {
		// plotted sequentially up and down
		if (displaymode == RASTER) {
		    char stringy5[] = "Raster Activity Plot";
		    printglstroke(25, (windowHeight / 2) - 70, 0.12, 90,
			    stringy5);         // Print Y-Axis label for Graph
		} else {
		    char stringy5[] = "Data Value";
		    printglstroke(25, (windowHeight / 2) - 50, 0.12, 90,
			    stringy5);         // Print Y-Axis label for Graph
		}

		int numberofrasterplots = xdim * ydim;
		if (windowToUpdate == win2)
		    numberofrasterplots = maxneuridrx; // bespoke for Discovery demo
		float yspacing = (float) (windowHeight - (2 * windowBorder))
			/ (float) (numberofrasterplots); // how many pixels per neuron ID

		int lastyplotted = -100;
		for (int i = 0 ; i < numberofrasterplots ; i++) { // for all plottable items
		    char stringxycoords[] = "(%d,%d)";
		    int xcord, ycord;
		    convert_index_to_coord(i, &xcord, &ycord); // (where xcoordinate and ycoordinate are ints)
		    if (windowToUpdate == win2) {
			xcord = 0;   // bespoke for Discovery demo
			ycord = i;
		    }
		    int yplotted = (i * yspacing) + windowBorder
			    + ((yspacing - 18) / 2) + 6; // what will be the next y coordinate
		    if (yplotted > lastyplotted + 12) { // plot only if enough space to not overlap labels.
			lastyplotted = yplotted; // we were the last one plotted
			if (windowToUpdate == win2) {
			    printgl(60, yplotted, GLUT_BITMAP_HELVETICA_12,
				    stringy4, i + 1); // Print Y Axis Labels at required intervals (linear format)
			} else {
			    printgl(60, yplotted, GLUT_BITMAP_HELVETICA_12,
				    stringxycoords, xcord, ycord); // Print Y Axis Labels at required intervals (X,Y format)
			}
		    }
		}
	    } else {
		char stringy5[] = "Y Coord";
		printglstroke(25, (windowHeight / 2) - 50, 0.12, 90,
			stringy5);             // Print Y-Axis label for Graph
		int ylabels = ydim;
		float yspacing = ((float) (windowHeight - (2 * windowBorder))
			/ ydim);
		if (LABELBYCHIP) {
		    ylabels = ydim / EACHCHIPY;
		    yspacing *= EACHCHIPY;
		}
		int yplotted, lastyplotted = -100;
		for (int i = 0 ; i < ylabels ; i++) {                // Y-Axis
		    yplotted = (i * yspacing) + windowBorder
			    + ((yspacing - 18) / 2) + 2; // what will be the next y coordinate
		    if (yplotted > (lastyplotted + 16)) { // plot only if enough space to not overlap labels.
			printgl(60,
				(i * yspacing) + windowBorder
					+ ((yspacing - 18) / 2) + 2,
				GLUT_BITMAP_HELVETICA_18, stringy4, i); // Print Y Axis Label
			lastyplotted = yplotted; // record where last label plotted on the Y axis
		    }
		}
	    }
	}
	// Y Axis
    }   // titles and labels are only printed if border is big enough

    for (int i = 0 ; i < (xdim * ydim) ; i++) {
	if (immediate_data[i] > (NOTDEFINEDFLOAT + 1)) {    // is valid
	    if (immediate_data[i] > MAXDATAFLOAT)
		immediate_data[i] = MAXDATAFLOAT; // check: can't increment above saturation level
	    if (immediate_data[i] < MINDATAFLOAT)
		immediate_data[i] = MINDATAFLOAT; // check: can't decrement below saturation level
	    if (DYNAMICSCALE) {
		if (immediate_data[i] > highwatermark && spinnakerboardipset)
		    highwatermark = immediate_data[i]; // only alter the high water mark when using dynamic scaling & data received
		if (immediate_data[i] < lowwatermark && spinnakerboardipset)
		    lowwatermark = immediate_data[i]; // only alter the low water mark when using dynamic scaling & data received
	    }
	}
    }  // scale all the values to plottable range

    float xsize = plotWidth / (float) xdim; // changed for dynamic reshaping
    if (xsize < 1.0) {
	xsize = 1.0;
    }
    float ysize = ((float) (windowHeight - (2 * windowBorder)) / ydim); // changed for dynamic reshaping
    float tileratio = xsize / ysize;

    for (int i = 0 ; i < xdim * ydim ; i++) {                  //
	int ii = coordinate_manipulate(i); // if any manipulation of how the data is to be plotted is required, do it
	int xcord, ycord;
	convert_index_to_coord(i, &xcord, &ycord); // find out the (x,y) coordinates of where to plot this data

	float magnitude = colour_calculator(immediate_data[ii], highwatermark,
		lowwatermark); // work out what colour we should plot - sets 'ink' plotting colour

	// if required, plot tiled mini version in bottom left
	if (DISPLAYMINIPLOT) {
	    if (fullscreen == 0) {
		float ysize = max((float) 1.0,
			(float) (windowBorder - (6 * gap)) / (float) ydim);
		float xsize = max((float) 1.0, ysize * tileratio); // draw little / mini tiled version in btm left - pixel size
		if (immediate_data[ii] > (NOTDEFINEDFLOAT + 1)) { // only plot if data is valid
		    glBegin (GL_QUADS); // draw little tiled version in btm left
		    glVertex2f((2 * gap) + (xcord * xsize),
			    (2 * gap) + (ycord * ysize));          //btm left
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize),
			    (2 * gap) + (ycord * ysize));     //btm right
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize),
			    (2 * gap) + ((ycord + 1) * ysize));   // top right
		    glVertex2f((2 * gap) + (xcord * xsize),
			    (2 * gap) + ((ycord + 1) * ysize));    // top left
		    glEnd(); // this plots the basic quad box filled as per colour above
		}

		if (livebox == i) { // draw outlines for selected box in little / mini version
		    glLineWidth(1.0);
		    glColor4f(0.0, 0.0, 0.0, 1.0);

		    glBegin (GL_LINE_LOOP);
		    glVertex2f((2 * gap) + (xcord * xsize),
			    (2 * gap) + (ycord * ysize));          //btm left
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize),
			    (2 * gap) + (ycord * ysize));         //btm right
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize),
			    (2 * gap) + ((ycord + 1) * ysize));   // top right
		    glVertex2f((2 * gap) + (xcord * xsize),
			    (2 * gap) + ((ycord + 1) * ysize));    // top left
		    glEnd(); // this plots the external black outline of the selected tile

		    glColor4f(1.0, 1.0, 1.0, 1.0);

		    glBegin(GL_LINE_LOOP);
		    glVertex2f(1 + (2 * gap) + (xcord * xsize),
			    1 + (2 * gap) + (ycord * ysize));       //btm left
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize) - 1,
			    1 + (2 * gap) + (ycord * ysize));     //btm right
		    glVertex2f((2 * gap) + ((xcord + 1) * xsize) - 1,
			    (2 * gap) + ((ycord + 1) * ysize) - 1); // top right
		    glVertex2f(1 + (2 * gap) + (xcord * xsize),
			    (2 * gap) + ((ycord + 1) * ysize) - 1); // top left
		    glEnd(); // this plots the internal white outline of the selected tile

		    glLineWidth(1.0);
		}
		if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
			&& INTERACTION && rasterpopulation == i) {
		    if (magnitude > 0.6) {
			glColor4f(0.0, 0.0, 0.0, 1.0);
		    } else {
			glColor4f(1.0, 1.0, 1.0, 1.0);
		    }
		    //print R
		    char stringR[] = "R";
		    printgl(2 * gap + xcord * xsize + (xsize - 8) / 2,
			    2 * gap + ycord * ysize + (ysize - 10) / 2,
			    GLUT_BITMAP_HELVETICA_12, stringR); // print an R in the box to indicate this is box that's raster plotting
		}
	    }
	}

	xsize = plotWidth / (float) xdim;
	if (xsize < 1.0) {
	    xsize = 1.0;
	}
	ysize = (windowHeight - 2 * windowBorder) / (float) ydim; // changed for dynamic reshaping

	if (displaymode == HISTOGRAM) {
	    ycord = 0;                       // for histogram always at origin
	    xcord = i;                    // just print in order, easy peasey!

	    xsize = plotWidth / (float) (xdim * ydim); // Histogram means all across X axis
	    ysize = magnitude * (float) (windowHeight - 2 * windowBorder); // Histogram means height of block adjusts based on value
	}

	magnitude = colour_calculator(immediate_data[ii], highwatermark,
		lowwatermark); // work out what colour we should plot - sets 'ink' plotting colour

	if (displaymode == HISTOGRAM || displaymode == TILED) { // basic plot if not using triangular interpolation
	    char stringnums[] = "%3.2f";
	    if (immediate_data[ii] > (NOTDEFINEDFLOAT + 1)) {
		glBegin (GL_QUADS);
		glVertex2f(windowBorder + (xcord * xsize),
			windowBorder + (ycord * ysize));  //btm left
		glVertex2f(windowBorder + ((xcord + 1) * xsize),
			windowBorder + (ycord * ysize)); //btm right
		glVertex2f(windowBorder + ((xcord + 1) * xsize),
			windowBorder + ((ycord + 1) * ysize));  // top right
		glVertex2f(windowBorder + (xcord * xsize),
			windowBorder + ((ycord + 1) * ysize)); // top left
		glEnd(); // this plots the basic quad box filled as per colour above
	    }

	    if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
		    && INTERACTION && livebox == i) {
		// draw 2 borders around box
		glLineWidth((float) gap);
		glColor4f(1.0, 1.0, 1.0, 1.0);

		glBegin (GL_LINE_LOOP);
		glVertex2f((gap * 1.5) + windowBorder + (xcord * xsize),
			(gap * 1.5) + windowBorder + (ycord * ysize)); //btm left
		glVertex2f(windowBorder + ((xcord + 1) * xsize) - (gap * 1.5),
			(gap * 1.5) + windowBorder + (ycord * ysize)); //btm right
		glVertex2f(windowBorder + ((xcord + 1) * xsize) - (gap * 1.5),
			windowBorder + ((ycord + 1) * ysize) - (gap * 1.5)); // top right
		glVertex2f((gap * 1.5) + windowBorder + (xcord * xsize),
			windowBorder + ((ycord + 1) * ysize) - (gap * 1.5)); // top left
		glEnd(); // this plots the internal white outline of the selected tile

		glColor4f(0.0, 0.0, 0.0, 1.0);

		glBegin(GL_LINE_LOOP);
		glVertex2f((gap * 0.5) + windowBorder + (xcord * xsize),
			(gap * 0.5) + windowBorder + (ycord * ysize)); //btm left
		glVertex2f(windowBorder + ((xcord + 1) * xsize) - (gap * 0.5),
			(gap * 0.5) + windowBorder + (ycord * ysize)); //btm right
		glVertex2f(windowBorder + ((xcord + 1) * xsize) - (gap * 0.5),
			windowBorder + ((ycord + 1) * ysize) - (gap * 0.5)); // top right
		glVertex2f((gap * 0.5) + windowBorder + (xcord * xsize),
			windowBorder + ((ycord + 1) * ysize) - (gap * 0.5)); // top left
		glEnd(); // this plots the external black outline of the selected tile

		glLineWidth(1.0);
	    }

	    if (plotvaluesinblocks != 0 && xsize > 8 && // if we want to plot numbers / values in blocks (& blocks big enough)
		    immediate_data[ii] > NOTDEFINEDFLOAT + 1) {
		if (magnitude > 0.6) {
		    glColor4f(0.0, 0.0, 0.0, 1.0);
		} else {
		    glColor4f(1.0, 1.0, 1.0, 1.0); // choose if light or dark labels
		}
		if (displaymode == HISTOGRAM) {
		    printglstroke(windowBorder + 5 + (xcord + 0.5) * xsize,
			    windowBorder + (ycord + 0.5) * ysize, 0.08, 90,
			    stringnums, immediate_data[ii]); // sideways if histogram
		} else {
		    printglstroke(windowBorder - 20 + (xcord + 0.5) * xsize,
			    windowBorder - 6 + (ycord + 0.5) * ysize, 0.12, 0,
			    stringnums, immediate_data[ii]); // normal
		}
	    }
	}

	if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
		&& INTERACTION) {
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    if (livebox == i) {
		char stringnums[] = "%3.2f";
		char stringbiascurrent[] = "Bias Curr:";

		printgl(windowWidth - windowBorder, windowBorder,
			GLUT_BITMAP_HELVETICA_18, stringbiascurrent);
		printgl(windowWidth - windowBorder, windowBorder - 30,
			GLUT_BITMAP_HELVETICA_18, stringnums, biasediting);
		if (!(biasediting - 0.01 < biascurrent[livebox]
			&& biasediting + 0.01 > biascurrent[livebox])) {
		    char stringNAK[] = "NAK:(last:%3.2f)";
		    printgl(windowWidth - windowBorder, windowBorder - 45,
			    GLUT_BITMAP_HELVETICA_12, stringNAK,
			    biascurrent[livebox]);
		    // print 'NAK' if the bias here is different to the last received value
		}
	    }
	    if (rasterpopulation != -1) {
		if (displaymode == HISTOGRAM) {
		    char stringraster[] = "'R'aster: (%d)"; // rasterpopulation is the active raster ID.
		    printgl(windowWidth - windowBorder, windowBorder - 60,
			    GLUT_BITMAP_HELVETICA_12, stringraster,
			    rasterpopulation % xdim + xdim * (ydim - 1)
				    - ydim * (rasterpopulation / xdim));
		} else {
		    char stringraster[] = "'R'aster: (%d,%d)"; // rasterpopulation is the active raster ID.
		    int xcoordinate, ycoordinate;
		    convert_index_to_coord(rasterpopulation, &xcoordinate,
			    &ycoordinate); // (where xcoordinate and ycoordinate are ints)
		    printgl(windowWidth - windowBorder, windowBorder - 60,
			    GLUT_BITMAP_HELVETICA_12, stringraster,
			    xcoordinate, ycoordinate);
		}
	    }
	}

	glColor4f(0.0, 0.0, 0.0, 1.0);

	if (displaymode == HISTOGRAM && printlabels && fullscreen == 0) { // titles and labels are only printed if border is big enough
	    char stringxycoords[] = "(%d,%d)";
	    float size = xdim * (float) ydim;
	    if ((plotWidth / size > 40.0)
		    || !(xcord % (int) (40.0 / (plotWidth / size)))) { // only print labels that don't overlap (40pixels)
		glLineWidth(1.0);

		glBegin(GL_LINES);
		glVertex2f(windowBorder + (xcord + 0.5) * xsize,
			windowBorder); // top - used xsize from earlier so this is why below the main plot.
		glVertex2f(windowBorder + (xcord + 0.5) * xsize,
			windowBorder - 10);     // inside
		glEnd();

		int xcoordinate, ycoordinate;
		convert_index_to_coord(i, &xcoordinate, &ycoordinate); // (where xcoordinate and ycoordinate are ints)
		printglstroke(windowBorder + (xcord + 0.5) * xsize, 60, 0.09,
			45, stringxycoords, xcoordinate, ycoordinate);
	    }
	}
    }

    // this (below) plots the triangular interpolated view. Each point smoothly blends fill between 3 vertex values 2 are real points (edges),
    // and the third a pseudo vertex in the centre of a block (average between 4 surrounding real vertices).
    if (displaymode == INTERPOLATED) {
	for (int i = 0 ; i < ydim - 1 ; i++) {                    // (y coord)
	    int yc = ydim - 2 - i;
	    for (int j = 0 ; j < xdim - 1 ; j++) {                // (x coord)
		int lowerleftvertex = coordinate_manipulate(
			convert_coord_to_index(j, i)); // if any manupulation of how the data is to be plotted is required, do it
		int upperleftvertex = coordinate_manipulate(
			convert_coord_to_index(j, i + 1));
		int upperrightvertex = coordinate_manipulate(
			convert_coord_to_index(j + 1, i + 1));
		int lowerrightvertex = coordinate_manipulate(
			convert_coord_to_index(j + 1, i));
#define MEANINGFUL(x)	((x) > NOTDEFINEDFLOAT + 1 ? (x) : 0)
		float pseudoaverage =
			(MEANINGFUL(immediate_data[lowerleftvertex])
				+ MEANINGFUL(immediate_data[upperleftvertex])
				+ MEANINGFUL(immediate_data[upperrightvertex])
				+ MEANINGFUL(immediate_data[lowerrightvertex]))
				/ 4.0;
		// if data is invalid then take it out of the average

		glBegin(GL_TRIANGLE_FAN);
		colour_calculator(pseudoaverage, highwatermark, lowwatermark);
		glVertex2f(windowBorder + xsize + j * xsize,
			windowHeight - windowBorder - ysize - yc * ysize); // pseudo vertex
		float base_x = windowBorder + xsize / 2.0;
		float base_y = windowHeight - windowBorder - ysize / 2.0;
		colour_calculator(immediate_data[upperleftvertex],
			highwatermark, lowwatermark);
		glVertex2f(base_x + j * xsize, base_y - yc * ysize); // upper left vertex
		colour_calculator(immediate_data[lowerleftvertex],
			highwatermark, lowwatermark);
		glVertex2f(base_x + j * xsize, base_y - (yc + 1) * ysize); // lower left vertex
		colour_calculator(immediate_data[lowerrightvertex],
			highwatermark, lowwatermark);
		glVertex2f(base_x + (j + 1) * xsize,
			base_y - (yc + 1) * ysize); // lower right vertex
		colour_calculator(immediate_data[upperrightvertex],
			highwatermark, lowwatermark);
		glVertex2f(base_x + (j + 1) * xsize, base_y - yc * ysize); // upper right vertex
		colour_calculator(immediate_data[upperleftvertex],
			highwatermark, lowwatermark);
		glVertex2f(base_x + j * xsize, base_y - yc * ysize); // upper left vertex
		glEnd();   // this plots the triangle fan (a 4 triangle grad)
	    }
	} // we build pseudo points between each 4 points (xsize-1*ysize-1) - average out the values and use this when drawing our triangles
    }

    if ((displaymode == TILED || displaymode == HISTOGRAM)
	    && gridlines != 0) { // scrolling modes x scale and labels and gridlines
	uint xsteps = xdim, ysteps = ydim;
	glColor4f(0.8, 0.8, 0.8, 1.0);            // Grey Colour for Gridlines
	if (displaymode == HISTOGRAM) {
	    ysize = windowHeight - 2 * windowBorder;
	    xsteps = xdim * ydim;
	    ysteps = 1;
	}
	if (xsize > 3.0) {      // if not going to completely obscure the data
	    for (int xcord = 0 ; xcord <= xsteps ; xcord++) { // vertical grid lines
		//xsize
		glBegin(GL_LINES);
		glVertex2f(windowBorder + xcord * xsize, windowBorder); //bottom
		glVertex2f(windowBorder + xcord * xsize,
			windowHeight - windowBorder);     //top
		glEnd();
	    }
	}
	if (ysize > 3.0) {      // if not going to completely obscure the data
	    for (int ycord = 0 ; ycord <= ysteps ; ycord++) { // horizontal grid lines
		glBegin(GL_LINES);
		glVertex2f(windowBorder, windowBorder + ycord * ysize); //left
		glVertex2f(windowWidth - windowBorder - keyWidth,
			windowBorder + ycord * ysize);     //right
		glEnd();
	    }
	}
    }

    if (DISPLAYXLABELS
	    && (displaymode == RASTER || displaymode == LINES
		    || displaymode == EEGSTYLE)) {
	nowtime = timestamp();		// get time now in us

	if (freezedisplay == 1) {
	    nowtime = freezetime;
	}

	// print some labels (no scrolling as vom inducing
	// if less than 1000s then dec place, else full seconds only
	float minlabelinterval = 100; // 1 label at least every 100 pixels (so as not so crowded) - should be a function of screen size?
	float interval = -1; // number of pixels between each label (interval distance)
	float i = 0.01; // interval in seconds being tests to see meets the # of labels required
	float howmanyintervals; // how many labels using this test value would create
	float maxnumberoflabels = plotWidth / (float) minlabelinterval; // what is the target maximum number of labels that we will print

	do {
	    howmanyintervals = displayWindow / i; // how many of the intervals are covered on the screen (eg. 0.01 0.1, 1, 10)
	    if (howmanyintervals <= maxnumberoflabels) { // if we are now less than or equal to our target # of labels
		interval = plotWidth / howmanyintervals;
	    }
	    if (interval < 0) {
		howmanyintervals = displayWindow / (i * 2); // how many of the intervals are covered on the screen (eg. 0.02 0.2, 2, 20)
		if (howmanyintervals <= maxnumberoflabels) { // if we are now less than or equal to our target # of labels
		    interval = plotWidth / howmanyintervals;
		}
	    }
	    if (interval < 0) {
		howmanyintervals = displayWindow / (i * 5); // how many of the intervals are covered on the screen (eg. 0.05 0.5, 5, 50)
		if (howmanyintervals <= maxnumberoflabels) { // if we are now less than or equal to our target # of labels
		    interval = plotWidth / howmanyintervals;
		}
	    }

	    i *= 10;
	} while (interval < 0 && i <= 1000); // while answer not found, but stopping before infinite division

	if (i > 1000) {
	    interval = 10000;                  // only print the 1st label
	}

	if (plotWidth >= 1) { // No labels to print, will cause an overflow
	    for (float j = plotWidth ; j >= 0 ; j -= (int) interval) {
		if (printlabels && fullscreen == 0) { // titles and labels are only printed if border is big enough
		    int64_t timelabel = ((nowtime - starttimez) / 100000)
			    - ((plotWidth - (int) j) * (10 * timeperindex)); // work out how long ago

		    glLineWidth(1.0);
		    glColor4f(0.75, 0.75, 0.75, 1.0); // dull non-distracting grey

		    glBegin (GL_LINES);
		    glVertex2f(windowBorder + (int) j,
			    windowHeight + 10 - windowBorder); // top - used xsize from earlier so this is why below the main plot.
		    glVertex2f(windowBorder + (int) j, windowBorder - 10); // inside
		    glEnd();

		    glColor4f(0.0, 0.0, 0.0, 1.0);            // black
		    int64_t decisecs = timelabel
			    * (int64_t) playbackmultiplier;
		    if (decisecs < 3600) { // if over 10000 deciseconds (1000secs) don't print the decimal.
			char stringtime[] = "%3.1f";
			printgl(windowBorder + (int) j - 18, 60,
				GLUT_BITMAP_HELVETICA_18, stringtime,
				((float) timelabel / 10.0)
					* playbackmultiplier);
		    } else {
			char stringtime[] = "%um%u";
			int64_t mins = decisecs / 600;
			int64_t secs = (decisecs % 600) / 10;
			printgl(windowBorder + (int) j - 18, 60,
				GLUT_BITMAP_HELVETICA_18, stringtime,
				(int) mins, (int) secs);
		    }
		}
	    }
	}
	glPointSize(1.0);
    }

    if (DISPLAYKEY && !fullscreen) {
	// only print if not in fullscreen mode
	glColor4f(0.0, 0.0, 0.0, 1.0);            // Black Text for Labels
	int keybase = windowBorder + (0.20 * (windowHeight - windowBorder)); // bottom of the key
	char stringy8[] = "%.2f";
	if (PERCENTAGESCALE) {
	    char stringy8[] = "%.2f%%";
	    printgl(windowWidth - 55, windowHeight - (1 * windowBorder) - 5,
		    GLUT_BITMAP_HELVETICA_12, stringy8, 100.0); // Print HighWaterMark Value
	    printgl(windowWidth - 55, keybase - 5, GLUT_BITMAP_HELVETICA_12,
		    stringy8, 0.0); // which for percentages is 0-100%
	} else {
	    printgl(windowWidth - 55, windowHeight - (1 * windowBorder) - 5,
		    GLUT_BITMAP_HELVETICA_12, stringy8, highwatermark); // Print HighWaterMark Value
	    printgl(windowWidth - 55, keybase - 5, GLUT_BITMAP_HELVETICA_12,
		    stringy8, lowwatermark); // Print LowWaterMark Value
	}
	float interval, difference = highwatermark - lowwatermark;
	for (float i = 10000 ; i >= 0.1 ; i /= 10.0) {
	    if (difference < i) {
		interval = i / ((difference < i / 2) ? 20.0 : 10.0);
	    }
	}
	if (PERCENTAGESCALE) {
	    interval = 10;  // fixed for percentage viewing
	}
	int multipleprinted = 1;
	float linechunkiness = (windowHeight - (windowBorder + keybase))
		/ (float) (highwatermark - lowwatermark);
	if (windowHeight - windowBorder - keybase > 0) { // too small to print
	    for (uint i = 0 ; i < windowHeight - windowBorder - keybase ;
		    i++) {
		float temperaturehere = 1.0;
		if (linechunkiness > 0.0) {
		    temperaturehere = i / (float) linechunkiness
			    + lowwatermark;
		}
		float magnitude = colour_calculator(temperaturehere,
			highwatermark, lowwatermark);

		glBegin (GL_LINES);
		glVertex2f(windowWidth - 65, i + keybase); // rhs
		glVertex2f(windowWidth - 65 - keyWidth, i + keybase); // lhs
		glEnd();      //draw_line;

		float positiveoffset = temperaturehere - lowwatermark;
		if (PERCENTAGESCALE) {
		    positiveoffset = positiveoffset / difference * 100.0; // scale it to a range of 0-100
		}
		if (positiveoffset >= (interval * multipleprinted)) {
		    glColor4f(0.0, 0.0, 0.0, 1.0);
		    glLineWidth(4.0);

		    glBegin(GL_LINES);
		    glVertex2f(windowWidth - 65, i + keybase); // rhs
		    glVertex2f(windowWidth - 75, i + keybase); // inside
		    glVertex2f(windowWidth - 55 - keyWidth, i + keybase); // inside
		    glVertex2f(windowWidth - 65 - keyWidth, i + keybase); // lhs
		    glEnd();

		    glLineWidth(1.0);
		    printgl(windowWidth - 55, i + keybase - 5,
			    GLUT_BITMAP_HELVETICA_12, stringy8,
			    lowwatermark + multipleprinted * interval);
		    multipleprinted++;
		}
		// if need to print a tag - do it
	    }

	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    glLineWidth(2.0);

	    glBegin (GL_LINE_LOOP);
	    glVertex2f(windowWidth - 65 - keyWidth, keybase); // bottomleft
	    glVertex2f(windowWidth - 65, keybase); // bottomright
	    glVertex2f(windowWidth - 65, windowHeight - windowBorder); // topright
	    glVertex2f(windowWidth - 65 - keyWidth,
		    windowHeight - windowBorder); // topleft
	    glEnd();      //draw_line loop around the key;

	    glLineWidth(1.0);
	} // key is only printed if big enough to print
    }

    // for display of visualisation screen controls
    if (PLAYPAUSEXIT && !fullscreen && windowToUpdate == win1) {
	// only print if not in fullscreen mode & the main window
	for (int boxer = 0 ; boxer < 3 ; boxer++) {
	    int boxsize = 40, gap = 10;
	    int xorigin = windowWidth - 3 * (boxsize + gap), yorigin =
		    windowHeight - gap - boxsize;
	    // local to this scope

	    if ((!freezedisplay && boxer == 0)
		    || (freezedisplay && boxer == 1) || boxer == 2) {
		glColor4f(0.0, 0.0, 0.0, 1.0);   // black is the new black

		glBegin (GL_QUADS);
		glVertex2f(xorigin + (boxer * (boxsize + gap)), yorigin); //btm left
		glVertex2f(xorigin + (boxer * (boxsize + gap)) + boxsize,
			yorigin); //btm right
		glVertex2f(xorigin + (boxer * (boxsize + gap)) + boxsize,
			yorigin + boxsize); // top right
		glVertex2f(xorigin + (boxer * (boxsize + gap)),
			yorigin + boxsize); // top left
		glEnd();

		// now draw shapes on boxes
		if (boxer == 2) {
		    glColor4f(1.0, 0.0, 0.0, 1.0);
		    glLineWidth(15.0);

		    glBegin (GL_LINES);
		    glVertex2f(xorigin + boxer * (boxsize + gap) + gap,
			    yorigin + boxsize - gap); // topleft
		    glVertex2f(
			    xorigin + boxer * (boxsize + gap) + boxsize - gap,
			    yorigin + gap); // bottomright
		    glVertex2f(
			    xorigin + boxer * (boxsize + gap) + boxsize - gap,
			    yorigin + boxsize - gap); // topright
		    glVertex2f(xorigin + boxer * (boxsize + gap) + gap,
			    yorigin + gap); // bottomleft
		    glEnd();

		    glLineWidth(1.0);
		}
		if (boxer == 0) {
		    glColor4f(1.0, 0.0, 0.0, 1.0);
		    glLineWidth(15.0);

		    glBegin(GL_QUADS);
		    glVertex2f(xorigin + gap, yorigin + boxsize - gap); // topleft
		    glVertex2f(xorigin + gap, yorigin + gap); // bottomleft
		    glVertex2f(xorigin + (boxsize + gap) / 2 - gap,
			    yorigin + gap); // bottomright
		    glVertex2f(xorigin + (boxsize + gap) / 2 - gap,
			    yorigin + boxsize - gap); // topright
		    glVertex2f(xorigin + (boxsize - gap) / 2 + gap,
			    yorigin + boxsize - gap); // topleft
		    glVertex2f(xorigin + (boxsize - gap) / 2 + gap,
			    yorigin + gap); // bottomleft
		    glVertex2f(xorigin + boxsize - gap, yorigin + gap); // bottomright
		    glVertex2f(xorigin + boxsize - gap,
			    yorigin + boxsize - gap); // topright
		    glEnd();

		    glLineWidth(1.0);
		}
		if (boxer == 1) {
		    glColor4f(1.0, 0.0, 0.0, 1.0);
		    glLineWidth(15.0);

		    glBegin (GL_TRIANGLES);
		    glVertex2f(xorigin + boxsize + 2 * gap,
			    yorigin + boxsize - gap); // topleft
		    glVertex2f(xorigin + 2 * boxsize, yorigin + boxsize / 2); // centreright
		    glVertex2f(xorigin + boxsize + gap * 2, yorigin + gap); // bottomleft
		    glEnd();

		    glLineWidth(1.0);
		}
	    }
	}

	if (printpktgone != 0) {
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    if (spinnakerboardipset == 0) {
		char stringy12[] = "Target Unknown";
		printgl((windowWidth - 3 * (boxsize + gap)) - 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, stringy12);
	    } else {
		char stringy12[] = "Packet Sent";
		printgl((windowWidth - 3 * (boxsize + gap)) + 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, stringy12);
	    }
	}
    }

    // only print if not in fullscreen mode
    if (SIMULATION == HEATMAP && INTERACTION && fullscreen == 0) {
	// for Heat Map work
	for (int boxer = 0 ; boxer < controlboxes * controlboxes ; boxer++) {
	    int boxx = boxer / controlboxes, boxy = boxer % controlboxes;
	    if (boxx == 1 || boxy == 1) {
		glColor4f(0.0, 0.0, 0.0, 1.0);
		if (boxer == livebox) {
		    glColor4f(0.0, 1.0, 1.0, 1.0);
		}
		char stringy9[] = "%3.1f";
		char stringy10[] = "%c";
		if (boxer == CENTRE || boxer == WEST || boxer == SOUTH
			|| boxer == NORTH || boxer == EAST) { //only plot NESW+centre
		    if (editmode != 0 || boxer == CENTRE) {
			if (boxer == CENTRE && editmode != 0) {
			    glColor4f(0.0, 0.6, 0.0, 1.0); // go button is green!
			}

			glBegin (GL_QUADS);
			glVertex2f(windowWidth - (boxx + 1) * (boxsize + gap),
				yorigin + boxy * (boxsize + gap)); //btm left
			glVertex2f(
				windowWidth - (boxx + 1) * (boxsize + gap)
					+ boxsize,
				yorigin + boxy * (boxsize + gap)); //btm right
			glVertex2f(
				windowWidth - (boxx + 1) * (boxsize + gap)
					+ boxsize,
				yorigin + boxsize + boxy * (boxsize + gap)); // top right
			glVertex2f(windowWidth - (boxx + 1) * (boxsize + gap),
				yorigin + boxsize + boxy * (boxsize + gap)); // top left
			glEnd();  // alter button
		    }
		    if (boxer != CENTRE) {
			glColor4f(0.0, 0.0, 0.0, 1.0);
			if (editmode != 0 && boxer != livebox) {
			    glColor4f(1.0, 1.0, 1.0, 1.0);
			}
			float currentvalue;
			if (boxer == NORTH) {
			    currentvalue = alternorth;
			}
			if (boxer == EAST) {
			    currentvalue = altereast;
			}
			if (boxer == SOUTH) {
			    currentvalue = altersouth;
			}
			if (boxer == WEST) {
			    currentvalue = alterwest;
			}
			printgl(windowWidth - (boxx + 1) * (boxsize + gap),
				yorigin + boxy * (boxsize + gap) + boxsize / 2
					- 5, GLUT_BITMAP_8_BY_13, stringy9,
				currentvalue);
		    } else {
			glColor4f(1.0, 1.0, 1.0, 1.0);
			if (editmode == 0) {
			    char stringy10[] = "Alter";
			    printgl(
				    windowWidth
					    - (boxx + 1) * (boxsize + gap),
				    yorigin + boxy * (boxsize + gap)
					    + boxsize / 2 - 5,
				    GLUT_BITMAP_8_BY_13, stringy10);
			} else {
			    char stringy11[] = " Go!";
			    printgl(
				    windowWidth
					    - (boxx + 1) * (boxsize + gap),
				    yorigin + boxy * (boxsize + gap)
					    + boxsize / 2 - 5,
				    GLUT_BITMAP_8_BY_13, stringy11);
			}
		    }
		}
	    }
	}
	// for Heat Map work
    }

    if (displaymode == RASTER || displaymode == LINES
	    || displaymode == EEGSTYLE) {
	nowtime = timestamp();			// get time now in us

	if (freezedisplay == 1) {
	    nowtime = freezetime;
	}

	float x_scaling_factor = 1;
	float y_scaling_factor = (((float) windowHeight
		- (2.0 * (float) windowBorder))
		/ (highwatermark - lowwatermark)); // value rise per pixel of display

	int updateline = ((nowtime - starttimez)
		/ (int64_t)(timeperindex * 1000000)) % (HISTORYSIZE); // which index is being plotted now (on the right hand side)

	if (updateline < 0 || updateline > HISTORYSIZE) {
	    printf(
		    "Error line 2093: Updateline out of bounds: %d. Times - Now:%lld  Start:%lld \n",
		    updateline, (long long int) nowtime,
		    (long long int) starttimez);        // CPDEBUG
	} else {
	    int linestoclear = updateline - lasthistorylineupdated; // work out how many lines have gone past without activity.
	    if (linestoclear < 0
		    && updateline + 500 > lasthistorylineupdated) {
		linestoclear = 0; // to cover any underflow when resizing plotting window smaller (wrapping difference will be <500)
	    }
	    // if has wrapped then work out the true value
	    if (linestoclear < 0) {
		linestoclear = updateline + HISTORYSIZE
			- lasthistorylineupdated;
	    }
	    int numberofdatapoints = xdim * ydim;
	    for (int i = 0 ; i < linestoclear ; i++) {
		for (int j = 0 ; j < numberofdatapoints ; j++) {
		    history_data[(1 + i + lasthistorylineupdated)
			    % HISTORYSIZE][j] =
			    INITZERO ? 0.0 : NOTDEFINEDFLOAT; // nullify data in the quiet period
		}
		if (win2) {
		    numberofdatapoints = MAXRASTERISEDNEURONS; // bespoke for Discovery demo
		    for (int j = 0 ; j < numberofdatapoints ; j++) {
			// nullify data in the quiet period
			history_data_set2[(1 + i + lasthistorylineupdated)
				% HISTORYSIZE][j] =
				INITZERO ? 0.0 : NOTDEFINEDFLOAT;
		    }
		}
	    }
	    // Upon Plot screen. All between lastrowupdated and currenttimerow will be nothing - clear between last and to now.  If lastrowupdated = currenttimerow, nothing to nullify.
	}

	int itop1 = updateline - plotWidth; // final entry to print (needs a max)
	int itop2 = HISTORYSIZE; // begin with assumption no need for any wraparound
	if (itop1 < 0) {            // if final entry has wrapped around array
	    itop2 = HISTORYSIZE + itop1; // 2nd bite adds on extra wraparound data
	    if (itop2 < 0) {
		itop2 = 0; // can we go to x scaling here?  This is a bit coarse.
	    }
	    itop1 = 0;                   // 1st bite floors at bottom of array
	}

	glColor4f(0.0, 0.0, 1.0, 1.0);                    // Will plot in blue

	glPointSize(
		clamp(1.0F,
			(float) ((windowHeight - 2.0 * windowBorder)
				/ maxneuridrx), 4.0F));

	float workingwithdata;         // data value being manipulated/studied
	int numberofrasterplots = xdim * ydim;
	if (windowToUpdate == win2) {
	    numberofrasterplots = maxneuridrx;   // bespoke for Discovery demo
	}

	if (displaymode == RASTER) {
	    uint *spikesperxcoord = new uint[plotWidth];
	    for (int j = 0 ; j < plotWidth ; j++) {
		spikesperxcoord[j] = 0;
	    }
	    uint maxspikerate = 200;
	    glBegin (GL_POINTS); // TODO if targetdotsize>=4 - draw lines?
	    for (int j = 0 ; j < numberofrasterplots ; j++) {
		int jj = coordinate_manipulate(j); // if any manipulation of how the data is to be plotted is required, do it
		for (int i = updateline ; i >= itop1 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = history_data[i][jj];
		    if (windowToUpdate == win2) {
			workingwithdata = history_data_set2[i][jj]; // bespoke for Discovery demo
		    }
		    if (workingwithdata > (NOTDEFINEDFLOAT + 1)) {
			y_scaling_factor = (float) (windowHeight
				- (2 * windowBorder))
				/ (float) (numberofrasterplots); // how many pixels per neuron ID
			int y = (int) ((float) (j + 0.5)
				* (float) y_scaling_factor) + windowBorder;
			int x = (windowWidth - windowBorder - keyWidth)
				- ((updateline - i) * x_scaling_factor);
			glVertex2f(x, y); // TODO change to lines for low counts? 1 of 2 (targetdotsize).
			// start at y-(targetdotsize/2) end at y+(targetdotsize/2)
			spikesperxcoord[x - windowBorder]++;
		    }
		}
		for (int i = (HISTORYSIZE - 1) ; i > itop2 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = history_data[i][jj];
		    if (windowToUpdate == win2) {
			workingwithdata = history_data_set2[i][jj]; // bespoke for Discovery demo
		    }
		    if (workingwithdata > (NOTDEFINEDFLOAT + 1)) {
			y_scaling_factor = (float) (windowHeight
				- (2 * windowBorder))
				/ (float) (numberofrasterplots); // how many pixels per neuron ID
			int y = (int) ((float) (j + 0.5)
				* (float) y_scaling_factor) + windowBorder;
			int x = (windowWidth - windowBorder - keyWidth)
				- ((updateline + ((HISTORYSIZE) - i))
					* x_scaling_factor);
			glVertex2f(x, y); // TODO change to lines for low counts? 2 of 2. (targetdotsize)
			// start at y-(targetdotsize/2) end at y+(targetdotsize/2)
			spikesperxcoord[x - windowBorder]++;
		    }
		}

	    }
	    glEnd();
	    delete spikesperxcoord; // free stack memory back up again explicitly

	} else if (displaymode == LINES) {
	    glLineWidth(2.0);
	    for (int j = 0 ; j < numberofrasterplots ; j++) {
		int jj = coordinate_manipulate(j); // if any manipulation of how the data is to be plotted is required, do it
		float magnitude = colour_calculator(immediate_data[jj],
			highwatermark, lowwatermark);
		glBegin (GL_LINE_STRIP);
		for (int i = updateline ; i >= itop1 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = INITZERO ? 0.0 : NOTDEFINEDFLOAT; // default to invalid
		    if (history_data[i][jj] > (NOTDEFINEDFLOAT + 1))
			workingwithdata = history_data[i][jj];
		    if (workingwithdata > (NOTDEFINEDFLOAT + 1)) {
			int y = (int) (windowBorder
				+ y_scaling_factor
					* (workingwithdata - lowwatermark));
			glVertex2f(
				windowWidth - windowBorder - keyWidth
					- (updateline - i) * x_scaling_factor,
				clamp(windowBorder, y,
					windowHeight - windowBorder));
		    }
		}
		for (int i = (HISTORYSIZE - 1) ; i > itop2 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = INITZERO ? 0.0 : NOTDEFINEDFLOAT; // default to invalid
		    if (history_data[i][jj] > (NOTDEFINEDFLOAT + 1))
			workingwithdata = history_data[i][jj];
		    if (workingwithdata > (NOTDEFINEDFLOAT + 1)) {
			int y = (int) (((float) windowBorder
				+ ((workingwithdata - lowwatermark)
					* y_scaling_factor)));
			glVertex2f(
				windowWidth - windowBorder - keyWidth
					- (updateline + HISTORYSIZE - i)
						* x_scaling_factor,
				clamp(windowBorder, y,
					windowHeight - windowBorder));
		    }
		}
		glEnd();
	    }
	    glLineWidth(1.0);
	} else if (displaymode == EEGSTYLE) {
	    float eegrowheight = (windowHeight - 2 * windowBorder)
		    / (float) numberofrasterplots; // space given over for each eeg-style plot EEGSTYLE difference to LINES:
	    y_scaling_factor = eegrowheight / (highwatermark - lowwatermark); // value rise per pixel of eeg-style row strip space EEGSTYLE difference to LINES:

	    glLineWidth(2.0);
	    for (int j = 0 ; j < numberofrasterplots ; j++) {
		int jj = coordinate_manipulate(j); // if any manipulation of how the data is to be plotted is required, do it
		glColor4f(0.7, 0.7, 0.7, 1.0);
		glLineWidth(1.0);
		glBegin (GL_LINE_STRIP);
		glVertex2f(windowBorder - 10,
			windowBorder + ((int) (eegrowheight * (float) j)));
		glVertex2f(windowBorder + plotWidth + 10,
			windowBorder + ((int) (eegrowheight * (float) j)));
		glEnd();
		glLineWidth(2.0);
		float magnitude = colour_calculator(immediate_data[jj],
			highwatermark, lowwatermark);
		glBegin(GL_LINE_STRIP);
		for (int i = updateline ; i >= itop1 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = INITZERO ? 0.0 : NOTDEFINEDFLOAT; // default to invalid
		    if (history_data[i][jj] > (NOTDEFINEDFLOAT + 1)) {
			workingwithdata = history_data[i][jj];
		    }
		    if (workingwithdata >= lowwatermark) {
			int y = (int) (windowBorder
				+ (workingwithdata - lowwatermark)
					* y_scaling_factor);
			y += (int) (eegrowheight * (float) j); // EEGSTYLE difference to LINES: add on row offset up screen
			glVertex2f(
				windowWidth - windowBorder - keyWidth
					- (updateline - i) * x_scaling_factor,
				clamp(windowBorder, y,
					windowHeight - windowBorder));
		    }
		}
		for (int i = HISTORYSIZE - 1 ; i > itop2 ; i--) { // For each column of elements to the right / newer than the current line
		    workingwithdata = INITZERO ? 0.0 : NOTDEFINEDFLOAT; // default to invalid
		    if (history_data[i][jj] > NOTDEFINEDFLOAT + 1) {
			workingwithdata = history_data[i][jj];
		    }
		    if (workingwithdata >= lowwatermark) {
			int y = (int) (windowBorder
				+ (workingwithdata - lowwatermark)
					* y_scaling_factor);
			y += (int) (eegrowheight * (float) j); // EEGSTYLE difference to LINES: add on row offset up screen
			glVertex2f(
				windowWidth - windowBorder - keyWidth
					- (updateline + HISTORYSIZE - i)
						* x_scaling_factor,
				clamp(windowBorder, y,
					windowHeight - windowBorder));
		    }
		}
		glEnd();
	    }
	    glColor4f(0.7, 0.7, 0.7, 1.0);
	    glLineWidth(1.0);
	    glBegin (GL_LINE_STRIP);
	    glVertex2f(windowBorder - 10,
		    windowBorder
			    + (int) (eegrowheight
				    * (float) numberofrasterplots));
	    glVertex2f(windowBorder + plotWidth + 10,
		    windowBorder
			    + (int) (eegrowheight
				    * (float) numberofrasterplots));
	    glEnd();
	}
    }

    if (DECAYPROPORTION > 0.0 && !freezedisplay) { // CP - if used!
	for (int i = 0 ; i < (xdim * ydim) ; i++) {
	    if (immediate_data[i] > NOTDEFINEDFLOAT + 1) {
		immediate_data[i] *= DECAYPROPORTION; // puts a decay on the data per frame plotted
	    }
	}
    }

    glutSwapBuffers();             // no flickery gfx
    somethingtoplot = 0;            // indicate we have finished plotting
} // display

// called whenever the display window is resized
void reshape(int width, int height)
{
    if (glutGetWindow() == 1) {
	windowWidth = width;
	plotWidth = windowWidth - (2 * windowBorder) - keyWidth;
	if (fullscreen == 1) {
	    windowWidth += keyWidth;
	    plotWidth = windowWidth - keyWidth;
	}
	if (windowWidth < (2 * windowBorder) + keyWidth) {
	    windowWidth = (2 * windowBorder) + keyWidth; // stop the plotting area becoming -ve and crashing
	    plotWidth = 0;
	}
	windowHeight = height;
    }

    // turn off label printing if too small, and on if larger than this threshold.
    if (plotWidth <= 1 || height - 2 * windowBorder <= 1) {
	printlabels = 0; // turn off label printing (not much data to plot!)
    } else {
	printlabels = 1;
    }

    glViewport(0, 0, (GLsizei) width, (GLsizei) height); // viewport dimensions
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    // an orthographic projection. Should probably look into OpenGL perspective projections for 3D if that's your thing
    glOrtho(0.0, width, 0.0, height, -50.0, 50.0);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
    somethingtoplot = 1;        // indicate we will need to refresh the screen

} // reshape

// Called when arrow keys (and some others) are pressed
void specialDown(int key, int x, int y)
{
    if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
	    && INTERACTION) {
	int xc, yc;
	convert_index_to_coord(livebox, &xc, &yc);
	switch (key) {
	case GLUT_KEY_UP:
	    if (displaymode == HISTOGRAM) {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(0, 0);
		} else {
		    livebox = -1;
		}
	    } else {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(0, 0);
		} else {
		    convert_index_to_coord(livebox, &xc, &yc);
		    if (yc + 1 >= ydim) {
			livebox = -1;
		    } else {
			livebox = convert_coord_to_index(xc, yc + 1);
		    }
		}
	    }
	    break;
	case GLUT_KEY_DOWN:
	    if (displaymode == HISTOGRAM) {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(xdim - 1, ydim - 1);
		} else {
		    livebox = -1;
		}
	    } else {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(0, ydim - 1);
		} else if (yc - 1 < 0) {
		    livebox = -1;
		} else {
		    livebox = convert_coord_to_index(xc, yc - 1);
		}
	    }
	    break;
	case GLUT_KEY_RIGHT:
	    if (displaymode == HISTOGRAM) {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(0, 0);
		} else if (livebox + 1 < xdim * ydim) {
		    livebox++;
		} else {
		    livebox = -1;
		}
	    } else {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(0, 0);
		} else if (xc < xdim - 1) {
		    livebox = convert_coord_to_index(++xc, yc);
		} else {
		    livebox = -1;
		}
	    }
	    break;
	case GLUT_KEY_LEFT:
	    if (displaymode == HISTOGRAM) {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(xdim - 1, ydim - 1);
		} else if (livebox > 0) {
		    livebox--;
		} else {
		    livebox = -1;
		}
	    } else {
		if (livebox == -1) {
		    livebox = convert_coord_to_index(xdim - 1, 0);
		} else if (xc > 0) {
		    livebox = convert_coord_to_index(--xc, yc);
		} else {
		    livebox = -1;
		}
	    }
	    break;
	}
	if (livebox < 0 || livebox >= (xdim * ydim)) {
	    livebox = -1;
	}
	if (livebox != -1) {
	    biasediting = biascurrent[coordinate_manipulate(livebox)]; // **swopxy**
	}
	somethingtoplot = 1; // indicate we will need to refresh the screen
    }
}

// Called when arrow keys (and some others) are released
void specialUp(int key, int x, int y)
{
    if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
	    && INTERACTION) {
	switch (key) {
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

// Called when keys are pressed
void keyDown(unsigned char key, int x, int y)
{
    switch (tolower(key)) {
    case 'f':
	if (fullscreen) {
	    windowBorder = oldwindowBorder;	// restore old bordersize
	    windowWidth -= keyWidth;		// recover the key area
	    plotWidth = windowWidth - (2 * windowBorder) - keyWidth;
	} else {
	    oldwindowBorder = windowBorder;	// used as border disappears when going full-screen
	    windowBorder = 0;			// no borders around the plots
	    windowWidth += keyWidth;		// take over the area used for the key too
	    plotWidth = windowWidth - keyWidth;
	}
	fullscreen = !fullscreen;
	break;

    case 'b':
	gridlines = !gridlines
	break;

	// number keys used to select colour maps
    case '1':
	colourused = 1;
	break;
    case '2':
	colourused = 2;
	break;
    case '3':
	colourused = 3;
	break;
    case '4':
	colourused = 4;
	break;
    case '5':
	colourused = 5;
	break;
    case '6':
	colourused = 6;
	break;
    case '7':
	colourused = 7;
	break;
    case '8':
	colourused = 8;
	break;

    case 'c':
	cleardown();			// clears the output when 'c' key is pressed
	break;
    case 'i':
	displaymode = INTERPOLATED;	// switch display mode to interpolated mode when the 'i' key is pressed
	needtorebuildmenu = 1;
	break;
    case 'h':
	displaymode = HISTOGRAM;	// switch display mode to histogram mode when the 'h' key is pressed
	needtorebuildmenu = 1;
	break;
    case 't':
	displaymode = TILED;		// switch display mode to tiled mode when the 't' key is pressed
	needtorebuildmenu = 1;
	break;
    case 'l':
	displaymode = LINES;		// switch display mode to meandering worm-type lines
	needtorebuildmenu = 1;
	break;
    case 'r':
	displaymode = RASTER;		// switch display mode to raster plot mode
	needtorebuildmenu = 1;
	break;
    case 'm':
	displaymode = EEGSTYLE;		// switch display mode to raster plot mode
	needtorebuildmenu = 1;
	break;
    case 'q':
	safelyshut();
	break;
    case '"': {
	int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below. PAUSE
	//for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	// send pause packet out
	freezedisplay = 1;
	freezetime = timestamp();	// get time now in us
	needtorebuildmenu = 1;
	break;
    }
    case 'p': {
	int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below. RESUME
	//for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	// send resume/restart packet out
	freezedisplay = 0;
	needtorebuildmenu = 1;
	break;
    }
    case '#':
	// toggles the plotting of values when the hash '#' key is pressed
	plotvaluesinblocks = !plotvaluesinblocks;
	break;

    case 'd':                            // 90 degree rotate
	rotateflip = !rotateflip;
	break;
    case 'v':                            // vector swap
	vectorflip = !vectorflip;
	break;
    case 'x':                            // x flip
	xflip = !xflip;
	break;
    case 'y':                            // y flip
	yflip = !yflip;
	break;

	// only if a scrolling mode in operation on the main plot!  (or (surrounded by ifdefs) its a 2ndary rasterised plot)
    case '>':
	if (displaymode == RASTER || displaymode == LINES
		|| ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
			&& rasterpopulation != -1)) {
	    displayWindow += 0.1;
	    if (displayWindow > 100) {
		displayWindow = 100;
	    }
	}
	break;
    case '<':
	if (displaymode == RASTER || displaymode == LINES
		|| ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
			&& rasterpopulation != -1)) {
	    displayWindow -= 0.1;
	    if (displayWindow < 0.1) {
		displayWindow = 0.1;
	    }
	}
	break;

    case '+':
	if (INTERACTION) {
	    if (SIMULATION == CHIPTEMP || SIMULATION == CPUUTIL) {
		for (int i = 0 ;
			i
				< (XDIMENSIONS / EACHCHIPX)
					* (YDIMENSIONS / EACHCHIPY) ; i++) {
		    send_to_chip(i, 17 + (3 << 5), 2, 0, 0, 0, 0); // send message to increase (2) CPU utilisation
		    printf("%d=(%d,%d), ", i, i / (XDIMENSIONS / EACHCHIPX),
			    i % (YDIMENSIONS / EACHCHIPY));
		}
	    } else if (SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY) {
		if (livebox != -1) {
		    short subcorepopid = 0;
		    biasediting += ALTERSTEPSIZE;
		    if (biasediting > 100.0) {
			biasediting = 10.0;
		    }
		    int popid = (coordinate_manipulate(livebox)
			    % (EACHCHIPY * EACHCHIPX)) + 1;
		    short coreid = popid;
		    int chipx = ((coordinate_manipulate(livebox)
			    / (EACHCHIPY * EACHCHIPX))
			    / (XDIMENSIONS / EACHCHIPX)) * 256;
		    int chipy = (coordinate_manipulate(livebox)
			    / (EACHCHIPY * EACHCHIPX))
			    % (XDIMENSIONS / EACHCHIPX);
		    if (BITSOFPOPID > 0) {
			subcorepopid = (popid - 1)
				& (int) (pow(2, BITSOFPOPID) - 1); // work out the proto population.
			coreid = 1 + ((popid - 1) >> BITSOFPOPID); // work out the coreID that this population is on.
		    }
		    sdp_sender(chipx + chipy, 0x80 + coreid, 259,
			    subcorepopid, 0, (int) (biasediting * 256.0), 0); // send message to alter bias current (arg3) (+0x80 = port 4)
		    somethingtoplot = 1;                     // refresh screen
		}
	    } else if (SIMULATION == HEATMAP) {
		if (livebox == NORTH) {
		    alternorth += ALTERSTEPSIZE;
		}
		if (livebox == EAST) {
		    altereast += ALTERSTEPSIZE;
		}
		if (livebox == SOUTH) {
		    altersouth += ALTERSTEPSIZE;
		}
		if (livebox == WEST) {
		    alterwest += ALTERSTEPSIZE;
		}
	    }
	}
	break;
    case '-':
	if (INTERACTION) {
	    if (SIMULATION == CHIPTEMP || SIMULATION == CPUUTIL) {
		for (int i = 0 ;
			i
				< (XDIMENSIONS / EACHCHIPX)
					* (YDIMENSIONS / EACHCHIPY) ; i++) {
		    send_to_chip(i, 17 + (3 << 5), 1, 0, 0, 0, 0); // send message to decrease (1) CPU utilisation
		}
	    } else if (SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY) {
		if (livebox != -1) {
		    short subcorepopid = 0;
		    biasediting -= ALTERSTEPSIZE;
		    if (biasediting < -10.0) {
			biasediting = -10.0;
		    }
		    int popid = (coordinate_manipulate(livebox)
			    % (EACHCHIPY * EACHCHIPX)) + 1;
		    short coreid = popid;
		    int chipx = ((coordinate_manipulate(livebox)
			    / (EACHCHIPY * EACHCHIPX))
			    / (XDIMENSIONS / EACHCHIPX)) * 256;
		    int chipy = (coordinate_manipulate(livebox)
			    / (EACHCHIPY * EACHCHIPX))
			    % (XDIMENSIONS / EACHCHIPX);
		    if (BITSOFPOPID > 0) {
			subcorepopid = (popid - 1)
				& (int) (pow(2, BITSOFPOPID) - 1); // work out the proto population.
			coreid = 1 + ((popid - 1) >> BITSOFPOPID); // work out the coreID that this population is on.
		    }
		    sdp_sender(chipx + chipy, 0x80 + coreid, 259,
			    subcorepopid, 0, (int) (biasediting * 256.0), 0); // send message to alter bias current (arg3) (+0x80 = port 4)
		    somethingtoplot = 1;                     // refresh screen
		}
	    } else if (SIMULATION == HEATMAP) {
		// if scroll down, and livebox!=0, decrement variable
		if (livebox == NORTH) {
		    alternorth -= ALTERSTEPSIZE;
		}
		if (livebox == EAST) {
		    altereast -= ALTERSTEPSIZE;
		}
		if (livebox == SOUTH) {
		    altersouth -= ALTERSTEPSIZE;
		}
		if (livebox == WEST) {
		    alterwest -= ALTERSTEPSIZE;
		}
	    }
	}
	break;

    case 'z':
	if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
		&& INTERACTION) {
	    // Call for Raster Data
	    if (livebox != -1
		    && (rasterpopulation == -1 || livebox != rasterpopulation)) {
		printf("Just called up for a raster from population: %d\n",
			livebox + 1);
		for (int i = 0 ; i < (xdim * ydim) ; i++) {
		    sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
		    sleeplet();
		    printf("%d: ", i);
		}
		if (win2 == 0) {
		    create_new_window(); // if window doesn't already exist then create it on demand
		}
		sdp_sender(0, 0x80 + livebox + 1, 258, 1, 4, 0, 0); // send message to turn on bit 4 (raster on)
		rasterpopulation = livebox;
		somethingtoplot = 1;
		needtorebuildmenu = 1;
	    } else {
		printf(
			"Just sent messages to cancel all rasters, while on box: %d\n",
			livebox);
		rasterpopulation = -1;
		for (int i = 0 ; i < (xdim * ydim) ; i++) {
		    sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
		    sleeplet();
		    printf("%d, ", i);
		}
		printf("\n");
		if (win2 != 0) {
		    destroy_new_window(); // if spawned window does exist then close it
		}
		somethingtoplot = 1;
		needtorebuildmenu = 1;
	    }
	    needtorebuildmenu = 1;
	}
	break;

	// for Heat Map work
    case 'n':
	if (SIMULATION == HEATMAP && INTERACTION && editmode) {
	    if (livebox == NORTH) {
		livebox = -1;
	    } else {
		livebox = NORTH;
	    }
	}
	break;
    case 'e':
	if (SIMULATION == HEATMAP && INTERACTION && editmode) {
	    if (livebox == EAST) {
		livebox = -1;
	    } else {
		livebox = EAST;
	    }
	}
	break;
    case 's':
	if (SIMULATION == HEATMAP && INTERACTION && editmode) {
	    if (livebox == SOUTH) {
		livebox = -1;
	    } else {
		livebox = SOUTH;
	    }
	}
	break;
    case 'w':
	if (SIMULATION == HEATMAP && INTERACTION && editmode) {
	    if (livebox == WEST) {
		livebox = -1;
	    } else {
		livebox = WEST;
	    }
	}
	break;
    case 'a':
	if (SIMULATION == HEATMAP && INTERACTION && !editmode) {
	    editmode = 1;
	    livebox = -1;
	}
	break;
    case 'g':
	if (SIMULATION == HEATMAP && INTERACTION && editmode) {
#ifdef LOCKINGSEND
	    editmode=0; // set to 0 for locking sends
#endif
	    livebox = -1;
	    int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below.
	    //for (i=0;i<(XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY);i++)
	    send_to_chip(i, 0x21, 1, 0, 0, 0, 4, (int) (alternorth * 65536),
		    (int) (altereast * 65536), (int) (altersouth * 65536),
		    (int) (alterwest * 65536)); // send temperature packet out
	}
	break;
    case '9':
	if (SIMULATION == HEATMAP && INTERACTION) {
	    // special case to randomise the heatmap
	    int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below.
	    //for (i=0;i<(XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY);i++)
	    alternorth = (rand() % (int) (highwatermark - lowwatermark))
		    + lowwatermark;
	    altereast = (rand() % (int) (highwatermark - lowwatermark))
		    + lowwatermark;
	    altersouth = (rand() % (int) (highwatermark - lowwatermark))
		    + lowwatermark;
	    alterwest = (rand() % (int) (highwatermark - lowwatermark))
		    + lowwatermark;
	    send_to_chip(i, 0x21, 1, 0, 0, 0, 4, (int) (alternorth * 65536),
		    (int) (altereast * 65536), (int) (altersouth * 65536),
		    (int) (alterwest * 65536)); // send temperature packet out (reset to zero).
	}
	break;
    case '0':
	if (SIMULATION == HEATMAP && INTERACTION) {
	    // special case to zero the heatmap
	    livebox = -1;
	    int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below.
	    //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	    if (alternorth < 1.0 && altereast < 1.0 && altersouth < 1.0
		    && alterwest < 1.0) {     // if very low -reinitialise
		alternorth = 40.0;
		altereast = 10.0;
		altersouth = 10.0;
		alterwest = 40.0;
		send_to_chip(i, 0x21, 1, 0, 0, 0, 4,
			(int) (alternorth * 65536), (int) (altereast * 65536),
			(int) (altersouth * 65536),
			(int) (alterwest * 65536)); // send temperature packet out (reset to zero).
	    } else {                                 // else reset to zero
		alternorth = 0.0;
		altereast = 0.0;
		altersouth = 0.0;
		alterwest = 0.0;
		send_to_chip(i, 0x21, 1, 0, 0, 0, 4, (int) (0), (int) (0),
			(int) (0), (int) (0)); // send temperature packet out (reset to zero).
	    }
	}
	break;

	// for Heat Map work
    }
    somethingtoplot = 1;        // indicate we will need to refresh the screen
}

// Called when keys are released
void keyUp(unsigned char key, int x, int y)
{
    // Does nothing
}

// called when something happs with the moosie
void mousehandler(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON) {
	for (int boxer = 0 ; boxer < 3 ; boxer++) {
	    int boxsize = 40, gap = 10;
	    int xorigin = windowWidth - 3 * (boxsize + gap), yorigin =
		    windowHeight - gap - boxsize;
	    // local to this scope

	    if ((freezedisplay == 0 && boxer == 0)
		    || (freezedisplay != 0 && boxer == 1) || boxer == 2) {
		int match = 0;
		// look to see which box we are in,

		if (x >= xorigin + boxer * (boxsize + gap)
			&& x < xorigin + boxer * (boxsize + gap) + boxsize
			&& windowHeight - y < yorigin + boxsize
			&& windowHeight - y >= yorigin) {
		    match = 1; // in this box, mark it so
		    if (boxer == 2) {
			safelyshut();
		    } else if (boxer == 0) {
			int i = 0; // only sending to 0,0  - if all chips, uncomment line below.
			//for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
			send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
			// send pause packet out
			freezedisplay = 1;
			freezetime = timestamp(); // get time now in us
			somethingtoplot = 1; // indicate we will need to refresh the screen
			needtorebuildmenu = 1;
		    } else if (boxer == 1) {
			int i = 0; // only sending to 0,0  - if all chips, uncomment line below.
			//for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
			send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
			// send resume/restart packet out
			freezedisplay = 0;
			somethingtoplot = 1; // indicate we will need to refresh the screen
			needtorebuildmenu = 1;
		    }
		}
	    }
	}
    }

    if ((SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY)
	    && INTERACTION) {
	float xsize = ((float) (plotWidth) / xdim); // size of the tiles in the x dimension
	float ysize = ((float) (windowHeight - (2 * windowBorder)) / ydim); // size of the tiles in the y dimension
	// section for mini display selection detection (on all display options)
	if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON
		&& fullscreen == 0) { // print mini version unless fullscreen
	    if (xsize < 1.0)
		xsize = 1.0; // small pixels can't be less than a pixel wide or they're invisible
	    float tileratio = xsize / ysize; // the aspect ratio of the mini plot changes with the aspect ratio of the main screen

	    for (int i = 0 ; i < (xdim * ydim) ; i++) {
		int xcord, ycord;
		convert_index_to_coord(i, &xcord, &ycord); // (where xcoordinate and ycoordinate are ints)

		float ysize = max((float) 1.0,
			(float) (windowBorder - (6 * gap)) / (float) ydim); // minplot is the height of the border - 6*gapsizes. Plit between the # of y coords
		float xsize = max((float) 1.0, ysize * tileratio); // draw little tiled version in btm left - pixel size (can't be smaller than zero).
		int xmin = (int) (2 * gap + (float) xcord * xsize);
		int xmax = (int) (2 * gap + ((float) xcord + 1) * xsize);
		int ymax = windowHeight
			- (int) (2 * gap + (float) ycord * ysize); // y coords for screen and mouse are inverted
		int ymin = windowHeight
			- (int) (2 * gap + ((float) ycord + 1) * ysize);
		if (x >= xmin && x < xmax && y >= ymin && y < ymax) {
		    livebox = ((livebox == i) ? -1 : i); // toggle on off same box
		    biasediting = biascurrent[coordinate_manipulate(i)]; // **swopxy** the value we are going to be altering
		    somethingtoplot = 1;     // need to refresh the screen
		    needtorebuildmenu = 1; // context has changed so the menu probably will change and needs rebuilding
		}
	    }
	}

	xsize = ((float) (plotWidth) / xdim); // reset size of the tiles in the x dimension
	ysize = ((float) (windowHeight - (2 * windowBorder)) / ydim); // reset size of the tiles in the y dimension

	if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON
		&& (displaymode == TILED || displaymode == HISTOGRAM)) { // if we are in a tiled mode we can make a selection
	    if (displaymode == HISTOGRAM) {
		xsize = ((float) (plotWidth) / (xdim * ydim)); // Histogram means all tiles are across X axis
		ysize = ((float) (windowHeight - (2 * windowBorder))); // Histogram means height of block adjusts is height of the plottable area
	    }
	    for (int i = 0 ; i < (xdim * ydim) ; i++) {
		int match = 0;
		int xcord, ycord;
		convert_index_to_coord(i, &xcord, &ycord); // (where xcoordinate and ycoordinate are ints)

		if (displaymode == HISTOGRAM) {
		    xcord = i; // for histogram is just the data index, reorder into btmleft, btmright, ... topleft, topright coords
		    ycord = 0;           // for histogram always at origin
		}
		if (x >= windowBorder + xcord * xsize
			&& windowHeight - y >= windowBorder + ycord * ysize
			&& x < windowBorder + (xcord + 1) * xsize
			&& windowHeight - y
				< windowBorder + (ycord + 1) * ysize) {
		    match = 1; // in this box, mark it so
		}

		if (match == 1) {
		    livebox = ((livebox == i) ? -1 : i);
		    biasediting = biascurrent[coordinate_manipulate(i)]; // **swopxy**
		    somethingtoplot = 1;                 // refresh screen
		    needtorebuildmenu = 1;
		}
	    }
	} else if (state == GLUT_DOWN && button == 3 && livebox != -1) {
	    // if scroll up, and box is selected, increment variable
	    if (livebox != -1) {
		short subcorepopid = 0;
		biasediting += ALTERSTEPSIZE;
		if (biasediting > 100.0) {
		    biasediting = 10.0;
		}
		int popid = (coordinate_manipulate(livebox)
			% (EACHCHIPY * EACHCHIPX)) + 1;
		short coreid = popid;
		int chipx =
			((coordinate_manipulate(livebox)
				/ (EACHCHIPY * EACHCHIPX))
				/ (XDIMENSIONS / EACHCHIPX)) * 256;
		int chipy = (coordinate_manipulate(livebox)
			/ (EACHCHIPY * EACHCHIPX))
			% (XDIMENSIONS / EACHCHIPX);
		if (BITSOFPOPID > 0) {
		    subcorepopid = (popid - 1)
			    & (int) (pow(2, BITSOFPOPID) - 1); // work out the proto population.
		    coreid = 1 + ((popid - 1) >> BITSOFPOPID); // work out the coreID that this population is on.
		}
		sdp_sender(chipx + chipy, 0x80 + coreid, 259, subcorepopid, 0,
			(int) (biasediting * 256.0), 0); // send message to alter bias current (arg3) (+0x80 = port 4)
		somethingtoplot = 1;                     // refresh screen
	    }
	} else if (state == GLUT_DOWN && button == 4 && livebox != -1) {
	    // if scroll down, and box is selected, decrement variable
	    if (livebox != -1) {
		short subcorepopid = 0;
		biasediting -= ALTERSTEPSIZE;
		if (biasediting < -10.0) {
		    biasediting = -10.0;
		}
		int popid = (coordinate_manipulate(livebox)
			% (EACHCHIPY * EACHCHIPX)) + 1;
		short coreid = popid;
		int chipx =
			((coordinate_manipulate(livebox)
				/ (EACHCHIPY * EACHCHIPX))
				/ (XDIMENSIONS / EACHCHIPX)) * 256;
		int chipy = (coordinate_manipulate(livebox)
			/ (EACHCHIPY * EACHCHIPX))
			% (XDIMENSIONS / EACHCHIPX);
		if (BITSOFPOPID > 0) {
		    subcorepopid = (popid - 1)
			    & (int) (pow(2, BITSOFPOPID) - 1); // work out the proto population.
		    coreid = 1 + ((popid - 1) >> BITSOFPOPID); // work out the coreID that this population is on.
		}
		sdp_sender(chipx + chipy, 0x80 + coreid, 259, subcorepopid, 0,
			(int) (biasediting * 256.0), 0); // send message to alter bias current (arg3) (+0x80 = port 4)
		somethingtoplot = 1;
	    }
	}
    }

    if (SIMULATION == HEATMAP && INTERACTION) {
	if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON) {
	    for (int boxer = 0 ; boxer < (controlboxes * controlboxes) ;
		    boxer++) {
		int boxx = boxer % controlboxes;
		int boxy = boxer / controlboxes;
		if (x >= windowWidth - (boxx + 1) * (boxsize + gap)
			&& windowHeight - y
				>= yorigin + boxy * (boxsize + gap)
			&& x
				< windowWidth - (boxx + 1) * (boxsize + gap)
					+ boxsize
			&& windowHeight - y
				< yorigin + boxsize
					+ boxy * (boxsize + gap)) {
		    int selectedbox = (boxx * controlboxes) + boxy;
		    if (editmode == 0) {
			if (selectedbox == CENTRE) {
			    // if editmode==0 then if box ==4 editmode=1, livebox =0, calculate side values to edit;
			    editmode = 1;
			    livebox = -1;
			    somethingtoplot = 1; // indicate we will need to refresh the screen
			}
		    } else if (selectedbox == CENTRE) {
			// if editmode==1 then if box ==4 editmode=0, send command;
#ifdef LOCKINGSEND
			editmode = 0;
#endif
			livebox = -1;
			int i = 0; // only sending to 0,0  - if all chips, uncomment line below.
			//for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
			send_to_chip(i, 0x21, 1, 0, 0, 0, 4,
				(int) (alternorth * 65536),
				(int) (altereast * 65536),
				(int) (altersouth * 65536),
				(int) (alterwest * 65536)); // send temp pack
			somethingtoplot = 1; // indicate we will need to refresh the screen
		    } else if (selectedbox == WEST || selectedbox == SOUTH
			    || selectedbox == NORTH || selectedbox == EAST) { //NESW
			if (selectedbox == livebox) {
			    // if editmode==1 and box==livebox livebox=0
			    livebox = -1;
			    somethingtoplot = 1; // indicate we will need to refresh the screen

			} else {
			    // if editmode==1 and box!=livebox livebox=box
			    livebox = selectedbox;
			    somethingtoplot = 1; // indicate we will need to refresh the screen
			}
		    }
		}
	    }

	    // now check for play, pause and exit buttons.
	    // if freezedisplay==0 then display pause button, else display play button.

	} else if (state == GLUT_DOWN && button == 3 && livebox != -1) {
	    // if scroll up, and livebox!=0, increment variable
	    if (livebox == NORTH) {
		alternorth += ALTERSTEPSIZE;
	    }
	    if (livebox == EAST) {
		altereast += ALTERSTEPSIZE;
	    }
	    if (livebox == SOUTH) {
		altersouth += ALTERSTEPSIZE;
	    }
	    if (livebox == WEST) {
		alterwest += ALTERSTEPSIZE;
	    }
	    somethingtoplot = 1; // indicate we will need to refresh the screen

	} else if (state == GLUT_DOWN && button == 4 && livebox != -1) {
	    // if scroll down, and livebox!=0, decrement variable
	    if (livebox == NORTH) {
		alternorth -= ALTERSTEPSIZE;
	    }
	    if (livebox == EAST) {
		altereast -= ALTERSTEPSIZE;
	    }
	    if (livebox == SOUTH) {
		altersouth -= ALTERSTEPSIZE;
	    }
	    if (livebox == WEST) {
		alterwest -= ALTERSTEPSIZE;
	    }
	    somethingtoplot = 1; // indicate we will need to refresh the screen

	} else {
	    //anything else you fancy
	}
    }

    if (state == GLUT_DOWN && button == GLUT_LEFT_BUTTON
	    && somethingtoplot == 0) {
	livebox = -1; // if you didn't manage to do something useful, then likely greyspace around the figure was clicked (should now deselect any selection)
	somethingtoplot = 1;
	rebuildmenu();
    }

}

// Called repeatedly, once per OpenGL loop
void idleFunction()
{
    if (needtorebuildmenu == 1 && menuopen == 0) {
	filemenu();
	rebuildmenu();    // if menu is not open we can make changes
	needtorebuildmenu = 0;
    }

    int usecperframe = (1000000 / MAXFRAMERATE);        // us target per frame
    struct timeval stopwatchus, deltateeus;        // declare timing variables
    struct timespec ts; // used for calculating how long to wait for next frame
    int64_t nowtime, howlongrunning, howlongtowait;            // for timings

    if (plotWidth != windowWidth - (2 * windowBorder) - keyWidth) {
	printf(
		"NOT SAME: windowWidth-(2*windowBorder)-keyWidth=%d, plotWidth=%d.\n",
		windowWidth - (2 * windowBorder) - keyWidth, plotWidth);
    }

    gettimeofday(&stopwatchus, NULL);                // grab current time
    howlongtowait = ((int64_t) starttimez
	    + ((int64_t) counter * (int64_t) usecperframe))
	    - (((int64_t) stopwatchus.tv_sec * (int64_t) 1000000)
		    + (int64_t) stopwatchus.tv_usec); // how long in us until we need to draw the next frame

    if (howlongtowait > 0) {
	ts.tv_sec = howlongtowait / 1000000; // # seconds (very unlikely to be in the seconds!)
	ts.tv_nsec = (howlongtowait % 1000000) * 1000; // us * 1000 = nano secs
	nanosleep(&ts, NULL);   // if we are ahead of schedule sleep for a bit
    }

    // log lastrowupdated.
    // Upon Receive packet. All between lastrowupdated and currenttimerow will be nothing - clear between lastrowupdated+1 and to now.
    // If lastrowupdated = currenttimerow, nothing to nullify, just add on.
    // Upon Plot screen. All between lastrowupdated and currenttimerow will be nothing - clear between last and to now.
    // If lastrowupdated = currenttimerow, nothing to nullify.

    gettimeofday(&stopwatchus, NULL);                // grab current time
    nowtime = (((int64_t) stopwatchus.tv_sec * (int64_t) 1000000)
	    + (int64_t) stopwatchus.tv_usec);    // get time now in us
    howlongrunning = nowtime - starttimez; // how long in us since visualation started running.
    // idle until the next frame/s interval is due

    if (printpktgone != 0 && nowtime > (printpktgone + 1000000)) {
	printpktgone = 0; // if packet send message has been displayed for more than 1s, stop its display
	somethingtoplot = 1;                    // force refresh screen
    }

    if (PLOTONLYONDEMAND == 0)
	somethingtoplot = 1; // force the refresh for this frame timing (even if nothing has changed!)

    if (somethingtoplot != 0) {
	windowToUpdate = win1;                // update the main master window
	display(); // update the display - will be timered inside this function to get desired FPS
	if (win2) {
	    int mainwindowmode = displaymode;
	    displaymode = RASTER;
	    windowToUpdate = win2;                // do the 2nd window too
	    display(); // update the display - will be timered inside this function to get desired FPS
	    displaymode = mainwindowmode;
	}
    } else {
	// do we actually want to update a bare min number of times - e.g. once per sec?
    }
}

void myinit(void)
{
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glColor3f(1.0, 1.0, 1.0);
    glShadeModel (GL_SMOOTH); // permits nice shading between plot points for interpolation if required
    filemenu();
    transformmenu();
    modemenu();
    colmenu();
    rebuildmenu();
}

void myfilemenu(int value)
{
    int menuitem = 1;
    if (outputfileformat == 0) {                    // no savefile open
	if (value == menuitem++) {
	    outputfileformat = 1;
	    open_or_close_output_file();    // start saving data in spinn
	}
	if (value == menuitem++) {
	    outputfileformat = 2;
	    open_or_close_output_file();    //  or neurotools format
	}
    } else {                                    // savefile open
	if (writingtofile == 2) {
	    if (value == menuitem++) {
		writingtofile = 0;                             //   and paused
		printf("Recording resumed...  ");
	    }
	} else {
	    if (value == menuitem++) {
		writingtofile = 2;                             //   or running
		printf("Recording paused...  ");
	    }
	}
	if (value == menuitem++)
	    open_or_close_output_file();                // closefile out
    }
    needtorebuildmenu = 1;
    //rebuildmenu(); // rebuild menu with state modified by this work
}

void filemenu(void)
{
    int menuitem = 1;
    glutDestroyMenu(filesubmenu);
    filesubmenu = glutCreateMenu(myfilemenu);
    if (outputfileformat == 0) {                    // no savefile open
	glutAddMenuEntry("Save Input Data in .spinn format (replayable)",
		menuitem++);        // start saving data in spinn
	glutAddMenuEntry(
		"Save Input Spike Data as write-only .neuro Neurotools format",
		menuitem++);        //  or neurotools format
    } else {                                        // savefile open
	if (writingtofile == 2) {
	    glutAddMenuEntry("Resume Saving Data to file", menuitem++); //   and paused
	} else {
	    glutAddMenuEntry("Pause Saving Data to file", menuitem++); //   or running
	}
	glutAddMenuEntry("End saving Data to file", menuitem++); // closefile out
    }
}

void mytransformmenu(int value)
{
    switch (value) {
    case 1:
	xflip = !xflip;
	break;
    case 2:
	yflip = !yflip;
	break;
    case 3:
	vectorflip = !vectorflip;
	break;
    case 4:
	rotateflip = !rotateflip;
	break;
    case 5:
	cleardown();
	break;
    }
    needtorebuildmenu = 1;
    //rebuildmenu(); // rebuild menu with state modified by this work
}

void transformmenu(void)
{
    int menuitem = 1;
    glutDestroyMenu(transformsubmenu);
    transformsubmenu = glutCreateMenu(mytransformmenu);
    glutAddMenuEntry("(X) Mirror (left to right swop)", menuitem++);
    glutAddMenuEntry("(Y) Reflect (top to bottom swop)", menuitem++);
    glutAddMenuEntry("(V) Vector Swop (Full X+Y Reversal)", menuitem++);
    glutAddMenuEntry("90 (D)egree Rotate Toggle", menuitem++);
    glutAddMenuEntry("(C) Revert changes back to default", menuitem++);
}

void mycolmenu(int value)
{
    switch (value) {
    case 1:
	colourused = MULTI;
	break;
    case 2:
	colourused = GREYS;
	break;
    case 3:
	colourused = REDS;
	break;
    case 4:
	colourused = GREENS;
	break;
    case 5:
	colourused = BLUES;
	break;
    case 6:
	colourused = THERMAL;
	break;
    case 7:
	colourused = RED;
	break;
    case 8:
	colourused = BLUE;
	break;
    }
    needtorebuildmenu = 1;
    //rebuildmenu(); // rebuild menu with state modified by this work
}

void colmenu(void)
{
    int menuitem = 1;
    glutDestroyMenu(coloursubmenu);
    coloursubmenu = glutCreateMenu(mycolmenu);
    glutAddMenuEntry("(1) MultiColoured", menuitem++);
    glutAddMenuEntry("(2) Greys", menuitem++);
    glutAddMenuEntry("(3) Reds", menuitem++);
    glutAddMenuEntry("(4) Greens", menuitem++);
    glutAddMenuEntry("(5) Blues", menuitem++);
    glutAddMenuEntry("(6) Thermal Imaging", menuitem++);
    glutAddMenuEntry("(7) ON/OFF Red Only", menuitem++);
    glutAddMenuEntry("(8) ON/OFF Blue Only", menuitem++);

}

void mymodemenu(int value)
{
    switch (value) {
    case 1:
	displaymode = TILED;
	break;
    case 2:
	displaymode = HISTOGRAM;
	break;
    case 3:
	displaymode = INTERPOLATED;
	break;
    case 4:
	displaymode = LINES;
	break;
    case 5:
	displaymode = RASTER;
	break;
    case 6:
	displaymode = EEGSTYLE;
	break;
    }
    needtorebuildmenu = 1;
    //rebuildmenu(); // rebuild menu with state modified by this work
}

void modemenu(void)
{
    int menuitem = 1;
    glutDestroyMenu(modesubmenu);
    modesubmenu = glutCreateMenu(mymodemenu);
    glutAddMenuEntry("(T)iled", menuitem++);
    glutAddMenuEntry("(H)istogram", menuitem++);
    glutAddMenuEntry("(I)nterpolated", menuitem++);
    glutAddMenuEntry("(L)ines", menuitem++);
    glutAddMenuEntry("(R)aster", menuitem++);
    glutAddMenuEntry("(M)ultichannel EEG Style", menuitem++);
}

void rebuildmenu(void)
{
    int menuitem = 1;
    glutDestroyMenu(RHMouseMenu);
    RHMouseMenu = glutCreateMenu(mymenu);

    if (SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY) {
	if (livebox >= 0) {
	    if (rasterpopulation == livebox) {
		glutAddMenuEntry("(Z) Turn OFF this population's Raster Plot",
			menuitem++); // if box not selected offer to turn raster plots off all
		// if off then turn on
	    } else {
		glutAddMenuEntry(
			"(Z) Turn Raster Plot ON for this population",
			menuitem++); // if box not selected offer to turn raster plots off all
	    }
	}
	glutAddMenuEntry("Turn OFF ALL population Raster Plots", menuitem++); // if mode unavailable to deselect box then offer to turn it all off!
	glutAddMenuEntry("-----", menuitem++);  // segmenter
    }
    glutAddSubMenu("Plot Mode", modesubmenu);
    glutAddSubMenu("Transform Plot", transformsubmenu);
    glutAddSubMenu("Colours", coloursubmenu);
    glutAddSubMenu("Save Data Operations", filesubmenu);

    glutAddMenuEntry("-----", menuitem++); // segmenter
    if (displaymode == HISTOGRAM || displaymode == TILED) {
	if (gridlines == 1) {
	    glutAddMenuEntry("Grid (B)orders off", menuitem++);
	} else {
	    glutAddMenuEntry("Grid (B)orders on", menuitem++);
	}
	if (plotvaluesinblocks == 1) {
	    glutAddMenuEntry("Numbers (#) off", menuitem++);
	} else {
	    glutAddMenuEntry("Numbers (#) on", menuitem++);
	}
    }
    if (fullscreen == 1) {
	glutAddMenuEntry("(F)ull Screen off", menuitem++);
    } else {
	glutAddMenuEntry("(F)ull Screen on", menuitem++);
    }

    glutAddMenuEntry("-----", menuitem++); // segmenter

    if (freezedisplay == 0) {
	glutAddMenuEntry("(\") Pause Plot", menuitem++);
    } else {
	glutAddMenuEntry("(P)lay / Restart Plot", menuitem++);
    }
    glutAddMenuEntry("(Q)uit", menuitem++);
    glutAttachMenu (GLUT_RIGHT_BUTTON);
}

void mymenu(int value)
{
    int menuitem = 1;

    if (SIMULATION == RATEPLOT || SIMULATION == RATEPLOTLEGACY) {
	if (livebox >= 0) {    // we have a box selected
	    if (rasterpopulation == livebox) {
		if (value == menuitem++) {
		    for (int i = 0 ; i < (xdim * ydim) ; i++) {
			sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
			sleeplet();
			printf("%d: ", i);
		    }
		    printf("\n");
		    if (win2 != 0) {
			destroy_new_window(); // if spawned window does exist then close it
		    }
		    rasterpopulation = -1;
		    somethingtoplot = 1;
		}
	    } else {
		if (value == menuitem++) {
		    for (int i = 0 ; i < (xdim * ydim) ; i++) {
			sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster for all populations
			sleeplet();
			printf("%d, ", i);
		    }
		    printf("\n");
		    if (win2 == 0) {
			create_new_window(); // if window doesn't already exist then create it on demand
		    }
		    sdp_sender(0, 0x80 + livebox + 1, 258, 1, 4, 0, 0); // send message to turn on bit 4 (raster on)
		    rasterpopulation = livebox;
		    somethingtoplot = 1;
		}
	    }
	}
	if (value == menuitem++) {
	    rasterpopulation = -1;
	    for (int i = 0 ; i < (xdim * ydim) ; i++) {
		sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
		sleeplet();
		printf("%d, ", i);
	    }
	    printf("\n");
	    if (win2 != 0)
		destroy_new_window(); // if spawned window does exist then close it
	    livebox = -1;
	    somethingtoplot = 1;
	}
	if (value == menuitem++) {
	    //do nothing - segmenter
	}
    }

    if (value == menuitem++) {
	//do nothing - segmenter
    }

    if (displaymode == HISTOGRAM || displaymode == TILED) {
	if (value == menuitem++) {
	    gridlines = !gridlines;
	}
	if (value == menuitem++) {
	    plotvaluesinblocks = !plotvaluesinblocks;
	}
    }
    if (value == menuitem++) {
	if (fullscreen == 1) {
	    windowBorder = oldwindowBorder;	// restore old bordersize
	    windowWidth -= keyWidth;		// recover the key area
	    plotWidth = windowWidth - (2 * windowBorder) - keyWidth;
	} else {
	    oldwindowBorder = windowBorder;	// used as border disappears when going full-screen
	    windowBorder = 0;			// no borders around the plots
	    windowWidth += keyWidth;		// take over the area used for the key too
	    plotWidth = windowWidth - keyWidth;
	}
	fullscreen = !fullscreen;
    }

    if (value == menuitem++) {
	//do nothing - segmenter;
    }

    if (value == menuitem++) {
	if (!freezedisplay) {
	    int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below. PAUSE
	    //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	    send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	    // send pause packet out
	    freezedisplay = 1;
	    freezetime = timestamp();		// get time now in us
	} else {
	    int i = 0; // only sending to 0,0  - if needs to go to all chips, uncomment line below. RESUME
	    //for (i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	    // send resume/restart packet out
	    freezedisplay = 0;
	}
    }
    if (value == menuitem++) {
	safelyshut();
    }

    needtorebuildmenu = 1;
}

void logifmenuopen(int status, int x, int y)
{
    if (status == GLUT_MENU_IN_USE) {
	menuopen = 1;            // no changes while it's open
    } else {
	if (needtorebuildmenu) {
	    filemenu();
	    rebuildmenu();
	}
	menuopen = 0;            // if menu is not open we can make changes
	needtorebuildmenu = 0;
    }
}

void safelyshut(void)
{
    // in some circumstances this gets run twice, therefore we check for this (partic the frees!)
    if (safelyshutcalls == 0) {
	int ii;         // used for freeing loops
	int i = 0; // only sending to 0,0  - if all chips, uncomment line below.
	//for (int i=0;i<((XDIMENSIONS*YDIMENSIONS)/(EACHCHIPX*EACHCHIPY));i++)
	if (spinnakerboardport != 0) {
	    // send exit packet out if we are interacting
	    send_to_chip(i, 0x21, 0, 0, 0, 0, 4, 0, 0, 0, 0);
	}

	open_or_close_output_file();	// Close down any open output file
	if (fileinput != NULL) {
	    fclose(fileinput);		// deal with input file
	}

	// free up mallocs made for dynamic arrays
	if (SIMULATION == RATEPLOT || RATEPLOTLEGACY) {
	    free(biascurrent);
	}

	for (ii = 0; ii < HISTORYSIZE ; ii++) {
	    free(history_data[ii]);
	}
	free(history_data);
	for (ii = 0; ii < HISTORYSIZE ; ii++) {
	    free(history_data_set2[ii]);
	}
	free(history_data_set2);

	free(immediate_data);

	for (ii = 0; ii < XDIMENSIONS * YDIMENSIONS ; ii++) {
	    free(maplocaltoglobal[ii]);
	}
	free(maplocaltoglobal);
	for (ii = 0; ii < XDIMENSIONS * YDIMENSIONS ; ii++) {
	    free(mapglobaltolocal[ii]);
	}
	free(mapglobaltolocal);

	safelyshutcalls++;		// note that this routine has been run before
    }
    exit(0);				// kill program dead
}

void display_win2(void)
{
    glutSetWindow(win2);

    glClearColor(0.2, 0.2, 0.7, 1.0);   // background colour - random surround
    glClear (GL_COLOR_BUFFER_BIT);

    glColor4f(0.0, 0.0, 0.0, 1.0);                    // Black Text for Labels

    char stringy[] = "This is a Test Mssg\n";
    printgl(250, 500, GLUT_BITMAP_HELVETICA_12, stringy);
    printglstroke(25 + (rand() % 50), 20, 0.12, 0, stringy);
    printgl(250, 300, GLUT_BITMAP_TIMES_ROMAN_24, stringy);
    glColor4f(1.0, 0.0, 0.0, 1.0);
    glBegin (GL_QUADS);
    glVertex2f(100, 100);
    glVertex2f(200, 100);
    glVertex2f(200, 800);
    glVertex2f(100, 800);
    glEnd();    // draw centre window
    glFlush();
    glutSwapBuffers();
}

void destroy_new_window(void)
{
    printf("Destroying new Window: %d, - after destruction is:", win2);
    glutDestroyWindow(win2);
    win2 = 0;
    printf("%d.\n", win2);
}

void create_new_window(void)
{
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB); /* Set the display mode */
    glutInitWindowSize(windowWidth, windowHeight); /* Set the window size */
    glutInitWindowPosition(800, 100); /* Set the window position */
    win2 = glutCreateWindow("Spawned window"); /* Create the window */
    myinit();
    glutDisplayFunc(display); /* Register the "display" function */
    glutReshapeFunc(reshape); /* Register the "reshape" function */
    glutIdleFunc(idleFunction); /* Register the idle function */
    glutSpecialFunc(specialDown); /* Register the special key press function  */
    glutSpecialUpFunc(specialUp); /* Register the special key release function */
    glutKeyboardFunc(keyDown); /* Register the key press function */
    glutKeyboardUpFunc(keyUp); /* Register the key release function */
    glutMouseFunc(mousehandler); /* Register the mouse handling function */
}

void open_or_close_output_file(void)
{
    time_t rawtime;
    struct tm * timeinfo;
    char filenamebuffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (fileoutput == NULL) { // If file isn't already open, so this is a request to open it
	if (outputfileformat == 2) {               //SAVE AS NEUROTOOLS FORMAT
	    strftime(filenamebuffer, 80, "packets-20%y%b%d_%H%M.neuro",
		    timeinfo);
	    printf("Saving spike packets in this file:\n       %s\n",
		    filenamebuffer);
	    fileoutput = fopen(filenamebuffer, "w");
	    fprintf(fileoutput, "# first_id =          \n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# n =          \n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# dt = 1.0\n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# dimensions = [          ]\n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# last_id =          \n"); // ! writing header for neurotools format file
	} else if (outputfileformat == 1) { //SAVE AS SPINN (UDP Payload) FORMAT
	    strftime(filenamebuffer, 80, "packets-20%y%b%d_%H%M.spinn",
		    timeinfo);
	    printf("Saving all input data in this file:\n       %s\n",
		    filenamebuffer);
	    fileoutput = fopen(filenamebuffer, "wb");
	}
    } else {                    // File is open already, so we need to close
	if (outputfileformat == 2) {          // File was in neurotools format
	    do {
	    } while (writingtofile == 1); // busy wait for file to finish being updated if in-flight
	    writingtofile = 2; // stop anybody else writing the file, pause further updating
	    fseek(fileoutput, 13, SEEK_SET);     // pos 13 First ID
	    fprintf(fileoutput, "%d", minneuridrx); // write lowest detected neurid
	    fseek(fileoutput, 29, SEEK_SET);   // pos 29 n number of neurons-1
	    fprintf(fileoutput, "%d", (maxneuridrx - minneuridrx)); // write number of neurons in range
	    fseek(fileoutput, 67, SEEK_SET); // pos 67 dimensions number of neurons-1
	    fprintf(fileoutput, "%d", (maxneuridrx - minneuridrx)); // write number of neurons in range
	    fseek(fileoutput, 90, SEEK_SET);     // pos 90 Last ID
	    fprintf(fileoutput, "%d", maxneuridrx); // write highest detected neurid
	}
	fflush(fileoutput);
	fclose(fileoutput);
	printf("File Save Completed\n");
	fileoutput = NULL;
	outputfileformat = 0;
	writingtofile = 0;
    }
}

#include "config.cpp"

int main(int argc, char **argv)
{
    // read and check the command line arguments
    int errfound = 0;
    char *configfn, *replayfn, *l2gfn, *g2lfn;
    float replayspeed = 1.0;

    parse_arguments(argc, argv, configfn, replayfn, l2gfn, g2lfn,
	    replayspeed);

    if (configfn == nullptr) {
	configfn = (char*) "visparam.ini"; // default filename if not supplied by the user
    }

    printf("\n\n"); // give some spacing for the output

    paramload(configfn); // recover the parameters from the file used to configure this visualisation

    if (l2gfn != nullptr && g2lfn != nullptr) { // if both translations are provided
	readmappings(l2gfn, g2lfn);       // read mappings file into array
    }

    for (int ii = 0 ; ii < XDIMENSIONS * YDIMENSIONS ; ii++) {
	int xcoordinate, ycoordinate, index;
	convert_index_to_coord(ii, &xcoordinate, &ycoordinate);
	index = convert_coord_to_index(xcoordinate, ycoordinate);
    }
    //call with: convert_index_to_coord(index, &xcoordinate, &ycoordinate);  // (where xcoordinate and ycoordinate are ints)
    //convert_coord_to_index(int x, int y)

    cleardown(); // reset the plot buffer to something sensible (i.e. 0 to start with)
    starttimez = timestamp();
    keepalivetime = starttimez;

    for (int j = 0 ; j < HISTORYSIZE ; j++) {
	for (int i = 0 ; i < xdim * ydim ; i++) {
	    history_data[j][i] = INITZERO ? 0.0 : NOTDEFINEDFLOAT;
	}
    }

    if (replayfn == nullptr) {
	fprintf(stderr,
		"No Input File provided.   Using Ethernet Frames Only\n");
    } else {
	playbackmultiplier = replayspeed;
	if (playbackmultiplier == 0.0) {
	    playbackmultiplier = 1;      // if mis-understood set to default 1
	}
	playbackmultiplier = clamp(0.01f, playbackmultiplier, 100.0f);
	printf("\nGot a request for a file called: %s.\n", replayfn);
	if (playbackmultiplier != 1) {
	    printf("    Requested Playback speed will be at %3.2f rate.\n",
		    playbackmultiplier);
	}
	pthread_t p1;
	fileinput = fopen(replayfn, "rb");
	if (fileinput == NULL) {
	    fprintf(stderr,
		    "I can't read the file you've specified you muppet:\n");
	    exit(2);
	}  // check if file is readable

	// setup with target 127.0.0.1 on right port if not already set
	if (spinnakerboardipset == 0) {
	    inet_aton("127.0.0.1", &spinnakerboardip);
	}
	spinnakerboardport = SDPPORT; // SDPPORT is used for outgoing cnnx
	spinnakerboardipset++;
	init_sdp_sender();
	printf("Set up to receive internally from %s on port: %d\n",
		inet_ntoa(spinnakerboardip), SDPPORT);
	pthread_create(&p1, NULL, load_stimulus_data_from_file, NULL); // away the file receiver goes
    }

    pthread_t p2; // this sets up the thread that can come back to here from type
    init_sdp_listening(); //initialization of the port for receiving SDP frames
    pthread_create(&p2, NULL, input_thread_SDP, NULL); // away the SDP network receiver goes

    glutInit(&argc, argv); /* Initialise OpenGL */

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB); /* Set the display mode */
    glutInitWindowSize(windowWidth + keyWidth, windowHeight); /* Set the window size */
    glutInitWindowPosition(0, 100); /* Set the window position */
    win1 = glutCreateWindow(
	    "VisRT - plotting your network data in real time"); /* Create the window */
    windowToUpdate = win1;

    myinit();

    glutDisplayFunc(display); /* Register the "display" function */
    glutReshapeFunc(reshape); /* Register the "reshape" function */
    glutIdleFunc(idleFunction); /* Register the idle function */
    glutSpecialFunc(specialDown); /* Register the special key press function  */
    glutSpecialUpFunc(specialUp); /* Register the special key release function */
    glutKeyboardFunc(keyDown); /* Register the key press function */
    glutKeyboardUpFunc(keyUp); /* Register the key release function */
    glutMouseFunc(mousehandler); /* Register the mouse handling function */
#ifdef TESTING
    glutMotionFunc (MouseMotion); /* Register the mouse handling function when a button depressed */
#endif

    glutCloseFunc(safelyshut); // register what to do when the use kills the window via the frame object
    glutMenuStatusFunc(logifmenuopen); // this keeps an eye on whether a window is open (as can't alter when open!)

    glutMainLoop(); /* Enter the main OpenGL loop */
    printf("goodbye");

    return 0;
}
