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

// --------------------------------------------------------------------------------------------------
// This section checks to see whether the compilation is on a 32- or a 64-bit architecture.

#if defined(__i386__)
#define MACHINEBITS 32
#elif defined(__x86_64__)
#define MACHINEBITS 64
#else
#define MACHINEBITS 0
#endif

// -------------------------------------------------------------------------
// Key enum types

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

enum directionbox_t {		// which box in the grid is used for each edge + middle, Ordered:  BtmR->TopR .. BtmL->TopR
    NORTH = 5,
    EAST = 1,
    SOUTH = 3,
    WEST = 7,
    CENTRE = 4
};

// --------------------------------------------------------------------------------------------------
// These defines used to represent max/min and invalid data values for checking out of range etc.
//    as data is typically all floating point, these INT versions are rarely (if ever) used.

#define MAXDATAINT	(65535)
#define MAXDATAFLOAT	(65535.0)

#define MINDATAINT	(-65535)
#define MINDATAFLOAT	(-65535.0)

#define NOTDEFINEDINT	(-66666)
#define NOTDEFINEDFLOAT	(-66666.0)

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

static inline bool is_defined(float f) {
    return f > NOTDEFINEDFLOAT + 1;
}

static inline void trigger_display_refresh(void) {
    somethingtoplot = 1;
}

//-------------------------------------------------------------------

void readmappings(char* filenamea, char* filenameb)
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
	    numberofpackets - 1, float(endtimer - startimer) / 1000000.0);

    int buffsize = 100000;          // max number of packets to load each time
    if (numberofpackets < buffsize) {
	buffsize = numberofpackets;  // size for the number of packets we have
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
    for (int i = 0 ; i < xdim * ydim ; i++) {
	immediate_data[i] = NOTDEFINEDFLOAT;
    }
    highwatermark = HIWATER; // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
    lowwatermark = LOWATER; // reset for auto-scaling of plot colours, can dynamically alter this value (255.0 = top of the shop)
    xflip = 0;
    yflip = 0;
    vectorflip = 0;
    rotateflip = 0;
}

//-------------------------------------------------------------------------
//  Draws a string at the specified coordinates.
//-------------------------------------------------------------------------
void printgl(float x, float y, void *font_style, const char* format, ...)
{
    va_list arg_list;
    char str[256];
    int i;

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
	const char* format,
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
    glEnable (GL_BLEND);	// antialias the font
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_LINE_SMOOTH);
    glLineWidth(1.5);		// end setup for antialiasing
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

static inline void convert_index_to_coord(int index, int &x, int &y)
{
    int elementid = index % (EACHCHIPX * EACHCHIPY);
    int elementx = elementid / EACHCHIPY;
    int elementy = elementid % EACHCHIPY;
    int tileid = index / (EACHCHIPX * EACHCHIPY);
    int tilex = tileid / (YDIMENSIONS / EACHCHIPY);
    int tiley = tileid % (YDIMENSIONS / EACHCHIPY);

    int xcord = tilex * EACHCHIPX + elementx;
    int ycord = tiley * EACHCHIPY + elementy;

    x = xcord;
    y = ycord;
}

static inline int convert_coord_to_index(int x, int y)
{
    int elementx = x % EACHCHIPX;	// x position in tile
    int elementy = y % EACHCHIPY;	// y position in tile
    int tilex = x / EACHCHIPX;		// x tile
    int tiley = y / EACHCHIPY;		// y tile

    int elementid = elementx * EACHCHIPY + elementy;
    int index = (EACHCHIPX * EACHCHIPY) * (tilex * YCHIPS + tiley)
	    + elementid; // CP 16thJan to support #chips rather than calculated

    return index;
}

int coordinate_manipulate(int ii)
{
    int i = ii;     // begin with the assumption of no flipping of coordinates
    int xcoordinate, ycoordinate;

    if (yflip || xflip || vectorflip || rotateflip) {
	const int chips_x = XDIMENSIONS / EACHCHIPX;
	const int chips_y = YDIMENSIONS / EACHCHIPY;
	int elementid = i % (EACHCHIPX * EACHCHIPY);
	int elementx = elementid / EACHCHIPY;
	int elementy = elementid % EACHCHIPY;
	const int tileid = i / (EACHCHIPX * EACHCHIPY);
	int tilex = tileid / chips_y;
	int tiley = tileid % chips_y;

	// flip ycords
	if (yflip) {
	    elementy = EACHCHIPY - 1 - elementy;
	    tiley = YDIMENSIONS / EACHCHIPY - 1 - tiley;
	}
	// flip xcoords
	if (xflip) {
	    elementx = EACHCHIPX - 1 - elementx;
	    tilex = chips_x - 1 - tilex;
	}

	elementid = elementx * EACHCHIPY + elementy;
	i = (EACHCHIPX * EACHCHIPY) * (tilex * chips_x + tiley) + elementid;

	// go back to front (cumulative)
	if (vectorflip) {
	    i = YDIMENSIONS * XDIMENSIONS - 1 - i;
	}
	// rotate
	if (rotateflip) {
	    convert_index_to_coord(i, xcoordinate, ycoordinate);
	    i = convert_coord_to_index(ycoordinate,
		    XDIMENSIONS - 1 - xcoordinate);
	}
    }
    return i;                            // return cumulative reorientation
}

//Split into n sections. specify colours for each section. how far away from
//top of section is it. Multiply R,G,B difference by this proportion
static inline float interpolate(
	const int gamut[][3],
	int gamutsize,
	int rgbindex,
	float fillcolour)
{
    fillcolour = clamp(0.0F, fillcolour, 1.0F);
    int colourindex = int(fillcolour * (gamutsize - 1));
    // how far away from higher index (range between 0 and 1).
    float colouroffset = (colourindex + 1) - fillcolour * (gamutsize - 1);
    return (1 - colouroffset) * gamut[colourindex + 1][rgbindex]
	    + colouroffset * gamut[colourindex][rgbindex];
}

float colour_calculator(float inputty, float hiwater, float lowater)
{
    // 6 different RGB colours, Black, Blue, Cyan, Green, Yellow, Red
#define COLOURSTEPS 6
    static const int gamut[COLOURSTEPS][3] = { { 0, 0, 0 }, { 0, 0, 1 }, { 0,
	    1, 1 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 } };

    float scalingfactor = 0;
    float fillcolour = 1.0;
    float diff = hiwater - lowater;

    inputty = clamp(lowater, inputty, hiwater);
    // for truncated plots or if data supplied is out of range.

    if (diff <= 0.0001) {
	fillcolour = 1.0; // if in error, or close to a divide by zero (no intensity plotted)
    } else {
	scalingfactor = 1 / diff; // work out how to scale the input data depending on low and highwater values
	fillcolour = scalingfactor * (inputty - lowater); // calculate the colour to plot
    }
    // must always range between 0 and 1 floating point

    float R = interpolate(gamut, COLOURSTEPS, 0, fillcolour);
    float G = interpolate(gamut, COLOURSTEPS, 1, fillcolour);
    float B = interpolate(gamut, COLOURSTEPS, 2, fillcolour);
    glColor4f(R, G, B, 1.0);

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

static inline void display_titles_labels(void)
{
    printgl(windowWidth / 2 - 200, windowHeight - 50,
	    GLUT_BITMAP_TIMES_ROMAN_24, (char*) TITLE);
    printgl(windowWidth / 2 - 250, windowHeight - 80,
	    GLUT_BITMAP_HELVETICA_12, "Menu: right click.");

    int xlabels = xdim;
    float xspacing = plotWidth / float(xdim);
    int xplotted, spacing = 24, lastxplotted = -100;

    printglstroke(windowWidth / 2 - 25, 20, 0.12, 0, "X Coord");
    for (int i = 0 ; i < xlabels ; i++) {                // X-Axis
	if (i > 100) {
	    spacing = 32;
	}
	xplotted = i * xspacing + windowBorder + (xspacing - 8) / 2 - 3; // what will be the next x coordinate
	if (xplotted > lastxplotted + spacing) { // plot if enough space to not overlap labels.
	    printgl(xplotted, 60, GLUT_BITMAP_HELVETICA_18, "%d", i); // Print X Axis Labels at required intervals
	    lastxplotted = xplotted; // record last x coordinate plotted to
	}
    }

    int ylabels = ydim;
    float yspacing = float(windowHeight - 2 * windowBorder) / ydim;
    int yplotted, lastyplotted = -100;

    printglstroke(25, windowHeight / 2 - 50, 0.12, 90, "Y Coord");
    for (int i = 0 ; i < ylabels ; i++) {                // Y-Axis
	yplotted = i * yspacing + windowBorder + (yspacing - 18) / 2 + 2; // what will be the next y coordinate
	if (yplotted > lastyplotted + 16) { // plot only if enough space to not overlap labels.
	    printgl(60, i * yspacing + windowBorder + (yspacing - 18) / 2 + 2,
		    GLUT_BITMAP_HELVETICA_18, "%d", i); // Print Y Axis Label
	    lastyplotted = yplotted; // record where last label plotted on the Y axis
	}
    }
}

static inline void display_key(void)
{
    glColor4f(0.0, 0.0, 0.0, 1.0);		// Black Text for Labels
    int keybase = windowBorder + 0.20 * (windowHeight - windowBorder); // bottom of the key
    printgl(windowWidth - 55, windowHeight - windowBorder - 5,
	    GLUT_BITMAP_HELVETICA_12, "%.2f", highwatermark); // Print HighWaterMark Value
    printgl(windowWidth - 55, keybase - 5, GLUT_BITMAP_HELVETICA_12, "%.2f",
	    lowwatermark); // Print LowWaterMark Value
    float interval = 1, difference = highwatermark - lowwatermark;
    for (float i = 10000 ; i >= 0.1 ; i /= 10.0) {
	if (difference < i) {
	    interval = i / (difference < i / 2 ? 20.0 : 10.0);
	}
    }
    int multipleprinted = 1;
    float linechunkiness = (windowHeight - windowBorder - keybase)
	    / float(highwatermark - lowwatermark);
    if (windowHeight - windowBorder - keybase > 0) { // too small to print
	for (uint i = 0 ; i < windowHeight - windowBorder - keybase ; i++) {
	    float temperaturehere = 1.0;
	    if (linechunkiness > 0.0) {
		temperaturehere = i / linechunkiness + lowwatermark;
	    }
	    float magnitude = colour_calculator(temperaturehere,
		    highwatermark, lowwatermark);

	    glBegin (GL_LINES);
	    glVertex2f(windowWidth - 65, i + keybase); // rhs
	    glVertex2f(windowWidth - 65 - keyWidth, i + keybase); // lhs
	    glEnd();      //draw_line;

	    float positiveoffset = temperaturehere - lowwatermark;
	    if (positiveoffset >= interval * multipleprinted) {
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
			GLUT_BITMAP_HELVETICA_12, "%.2f",
			lowwatermark + multipleprinted * interval);
		multipleprinted++;
	    }
	    // if need to print a tag - do it
	}

	glColor4f(0.0, 0.0, 0.0, 1.0);
	glLineWidth(2.0);

	glBegin (GL_LINE_LOOP);
	glOpenBoxVertices(windowWidth - 65 - keyWidth, keybase,
		windowWidth - 65, windowHeight - windowBorder);
	glEnd();      //draw_line loop around the key;

	glLineWidth(1.0);
    } // key is only printed if big enough to print
}

static inline void display_controls()
{
    const int boxsize = 40, gap = 10;

    for (int boxer = 0 ; boxer < 3 ; boxer++) {
	int xorigin = windowWidth - 3 * (boxsize + gap), yorigin =
		windowHeight - gap - boxsize;
	// local to this scope

	if ((!freezedisplay && boxer == 0) || (freezedisplay && boxer == 1)
		|| boxer == 2) {
	    glColor4f(0.0, 0.0, 0.0, 1.0);   // black is the new black

	    glBegin (GL_QUADS);
	    glRectVertices(xorigin + boxer * (boxsize + gap),
		    yorigin + boxsize,
		    xorigin + boxer * (boxsize + gap) + boxsize, yorigin);
	    glEnd();

	    glColor4f(1.0, 0.0, 0.0, 1.0);
	    glLineWidth(15.0);

	    // now draw shapes on boxes
	    if (boxer == 0) {
		glBegin(GL_QUADS);
		glRectVertices(xorigin + gap, yorigin + boxsize - gap,
			xorigin + (boxsize + gap) / 2 - gap, yorigin + gap);
		glRectVertices(xorigin + (boxsize - gap) / 2 + gap,
			yorigin + boxsize - gap, xorigin + boxsize - gap,
			yorigin + gap);
		glEnd();
	    } else if (boxer == 1) {
		glBegin (GL_TRIANGLES);
		glVertex2f(xorigin + boxsize + 2 * gap,
			yorigin + boxsize - gap); // topleft
		glVertex2f(xorigin + 2 * boxsize, yorigin + boxsize / 2); // centreright
		glVertex2f(xorigin + boxsize + gap * 2, yorigin + gap); // bottomleft
		glEnd();
	    } else if (boxer == 2) {
		glBegin (GL_LINES);
		glVertex2f(xorigin + boxer * (boxsize + gap) + gap,
			yorigin + boxsize - gap); // topleft
		glVertex2f(xorigin + boxer * (boxsize + gap) + boxsize - gap,
			yorigin + gap); // bottomright
		glVertex2f(xorigin + boxer * (boxsize + gap) + boxsize - gap,
			yorigin + boxsize - gap); // topright
		glVertex2f(xorigin + boxer * (boxsize + gap) + gap,
			yorigin + gap); // bottomleft
		glEnd();
	    }

	    glLineWidth(1.0);
	}
    }
}

static inline void display_gridlines(void)
{
    uint xsteps = xdim, ysteps = ydim;
    glColor4f(0.8, 0.8, 0.8, 1.0);	// Grey Colour for Gridlines

    // if not going to completely obscure the data
    if (xsize > 3.0) {
	// vertical grid lines
	for (int xcord = 0 ; xcord <= xsteps ; xcord++) {
	    glBegin (GL_LINES);
	    glVertex2f(windowBorder + xcord * xsize, windowBorder);
	    glVertex2f(windowBorder + xcord * xsize,
		    windowHeight - windowBorder);
	    glEnd();
	}
    }

    // if not going to completely obscure the data
    if (ysize > 3.0) {
	// horizontal grid lines
	for (int ycord = 0 ; ycord <= ysteps ; ycord++) {
	    glBegin (GL_LINES);
	    glVertex2f(windowBorder, windowBorder + ycord * ysize);
	    glVertex2f(windowWidth - windowBorder - keyWidth,
		    windowBorder + ycord * ysize);
	    glEnd();
	}
    }
}

static inline void display_boxes(void)
{
    for (int boxer = 0 ; boxer < controlboxes * controlboxes ; boxer++) {
	int boxx = boxer / controlboxes, boxy = boxer % controlboxes;
	if (boxx != 1 && boxy != 1) {
	    continue;
	}
	//only plot NESW+centre
	glColor4f(0.0, 0.0, 0.0, 1.0);
	if (boxer == livebox) {
	    glColor4f(0.0, 1.0, 1.0, 1.0);
	}
	if (editmode || boxer == CENTRE) {
	    if (boxer == CENTRE && editmode) {
		glColor4f(0.0, 0.6, 0.0, 1.0); // go button is green!
	    }

	    glBegin (GL_QUADS);
	    glRectVertices(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize,
		    windowWidth - (boxx + 1) * (boxsize + gap) + boxsize,
		    yorigin + boxy * (boxsize + gap));
	    glEnd();  // alter button
	}

	if (boxer == CENTRE) {
	    glColor4f(1.0, 1.0, 1.0, 1.0);
	    printgl(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize / 2 - 5,
		    GLUT_BITMAP_8_BY_13, editmode ? " Go!" : "Alter");
	} else {
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    if (editmode && boxer != livebox) {
		glColor4f(1.0, 1.0, 1.0, 1.0);
	    }
	    float currentvalue = 0.0;
	    if (boxer == NORTH) {
		currentvalue = alternorth;
	    } else if (boxer == EAST) {
		currentvalue = altereast;
	    } else if (boxer == SOUTH) {
		currentvalue = altersouth;
	    } else if (boxer == WEST) {
		currentvalue = alterwest;
	    }
	    printgl(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize / 2 - 5,
		    GLUT_BITMAP_8_BY_13, "%3.1f", currentvalue);
	}
    }
}

static inline void display_mini_pixel(
	float xsize,
	float ysize,
	float tileratio,
	int i,
	int ii,
	int xcord,
	int ycord)
{
    const int gap = 10;

    float ysize = max(1.0F, float(windowBorder - 6 * gap) / ydim);
    float xsize = max(1.0F, float(ysize * tileratio)); // draw little / mini tiled version in btm left - pixel size
    if (is_defined(immediate_data[ii])) { // only plot if data is valid
	glBegin (GL_QUADS); // draw little tiled version in btm left
	glRectVertices(2 * gap + xcord * xsize, 2 * gap + ycord * ysize,
		2 * gap + (xcord + 1) * xsize, 2 * gap + (ycord + 1) * ysize);
	glEnd(); // this plots the basic quad box filled as per colour above
    }

    if (livebox == i) { // draw outlines for selected box in little / mini version
	glLineWidth(1.0);
	glColor4f(0.0, 0.0, 0.0, 1.0);

	glBegin (GL_LINE_LOOP);
	glOpenBoxVertices(2 * gap + xcord * xsize, 2 * gap + ycord * ysize,
		2 * gap + (xcord + 1) * xsize, 2 * gap + (ycord + 1) * ysize);
	glEnd(); // this plots the external black outline of the selected tile

	glColor4f(1.0, 1.0, 1.0, 1.0);

	glBegin(GL_LINE_LOOP);
	glOpenBoxVertices(1 + 2 * gap + xcord * xsize,
		1 + 2 * gap + ycord * ysize,
		2 * gap + (xcord + 1) * xsize - 1,
		2 * gap + (ycord + 1) * ysize - 1);
	glEnd(); // this plots the internal white outline of the selected tile

	glLineWidth(1.0);
    }
}

static inline void display_pixel(
	float xsize,
	float ysize,
	int ii,
	int xcord,
	int ycord)
{
    // basic plot
    if (is_defined(immediate_data[ii])) {
	glBegin (GL_QUADS);
	glRectVertices(windowBorder + xcord * xsize,
		windowBorder + ycord * ysize,
		windowBorder + (xcord + 1) * xsize,
		windowBorder + (ycord + 1) * ysize);
	glEnd(); // this plots the basic quad box filled as per colour above
    }

    // if we want to plot values in blocks (and blocks big enough)
    if (plotvaluesinblocks && xsize > 8 && is_defined(immediate_data[ii])) {
	// choose if light or dark labels
	auto intensity = float(magnitude <= 0.6);
	glColor4f(intensity, intensity, intensity, 1.0);
	printglstroke(windowBorder - 20 + (xcord + 0.5) * xsize,
		windowBorder - 6 + (ycord + 0.5) * ysize, 0.12, 0, "%3.2f",
		immediate_data[ii]); // normal
    }
}

// display function, called whenever the display window needs redrawing
void display(void)
{
    int64_t nowtime;
    float timeperindex = displayWindow / float(plotWidth); // time in seconds per history index in use

    glPointSize(1.0);

    counter++;				// how many frames have we plotted in our history

    glLoadIdentity();
    glutSetWindow(windowToUpdate);	// specifically look at our plotting window
    glLoadIdentity();

    //glutPostRedisplay();

    glClearColor(0.8, 0.8, 0.8, 1.0);	// background colour - grey surround
    glClear (GL_COLOR_BUFFER_BIT);

    glColor4f(0.0, 0.0, 0.0, 1.0);	// Black Text for Labels

    // titles and labels are only printed if border is big enough
    if (printlabels && !fullscreen) {
	display_titles_labels();
    }

    // scale all the values to plottable range
    for (int i = 0 ; i < xdim * ydim ; i++) {
	if (is_defined(immediate_data[i])) {    // is valid
	    immediate_data[i] = clamp(MINDATAFLOAT, immediate_data[i],
		    MAXDATAFLOAT);
	    if (spinnakerboardipset) {
		if (immediate_data[i] > highwatermark) {
		    highwatermark = immediate_data[i];
		}
		if (immediate_data[i] < lowwatermark) {
		    lowwatermark = immediate_data[i];
		}
	    }
	}
    }

    float xsize = plotWidth / float(xdim); // changed for dynamic reshaping
    if (xsize < 1.0) {
	xsize = 1.0;
    }
    float ysize = float(windowHeight - 2 * windowBorder) / ydim; // changed for dynamic reshaping
    float tileratio = xsize / ysize;

    for (int i = 0 ; i < xdim * ydim ; i++) {
	int ii = coordinate_manipulate(i); // if any manipulation of how the data is to be plotted is required, do it
	int xcord, ycord;
	convert_index_to_coord(i, xcord, ycord); // find out the (x,y) coordinates of where to plot this data

	float magnitude = colour_calculator(immediate_data[ii], highwatermark,
		lowwatermark); // work out what colour we should plot - sets 'ink' plotting colour

	// if required, plot tiled mini version in bottom left
	if (!fullscreen) {
	    display_mini_pixel(xsize, ysize, tileratio, i, ii, xcord, ycord);
	}

	xsize = plotWidth / float(xdim);
	if (xsize < 1.0) {
	    xsize = 1.0;
	}
	ysize = float(windowHeight - 2 * windowBorder) / ydim; // changed for dynamic reshaping

	// Set 'ink' plotting colour
	colour_calculator(immediate_data[ii], highwatermark, lowwatermark);

	// basic plot
	display_pixel(xsize, ysize, ii, xcord, ycord);

    }
    glColor4f(0.0, 0.0, 0.0, 1.0);

    // scrolling modes x scale and labels and gridlines
    if (gridlines) {
	display_gridlines();
    }

    // only print if not in fullscreen mode
    if (!fullscreen) {
	display_key();

	// for display of visualisation screen controls
	display_controls();

	if (printpktgone) {
	    glColor4f(0.0, 0.0, 0.0, 1.0);
	    if (spinnakerboardipset == 0) {
		printgl(windowWidth - 3 * (boxsize + gap) - 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, "Target Unknown");
	    } else {
		printgl(windowWidth - 3 * (boxsize + gap) + 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, "Packet Sent");
	    }
	}

	display_boxes();
    }

    glutSwapBuffers();			// no flickery graphics
    somethingtoplot = 0;		// indicate we have finished plotting
}

// called whenever the display window is resized
void reshape(int width, int height)
{
    if (glutGetWindow() == 1) {
	windowWidth = width;
	plotWidth = windowWidth - 2 * windowBorder - keyWidth;
	if (fullscreen) {
	    windowWidth += keyWidth;
	    plotWidth = windowWidth - keyWidth;
	}
	if (windowWidth < 2 * windowBorder + keyWidth) {
	    windowWidth = 2 * windowBorder + keyWidth; // stop the plotting area becoming -ve and crashing
	    plotWidth = 0;
	}
	windowHeight = height;
    }

    // turn off label printing if too small, and on if larger than this threshold.
    printlabels = !(plotWidth <= 1 || height - 2 * windowBorder <= 1);

    glViewport(0, 0, (GLsizei) width, (GLsizei) height); // viewport dimensions
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity();
    // an orthographic projection. Should probably look into OpenGL perspective projections for 3D if that's your thing
    glOrtho(0.0, width, 0.0, height, -50.0, 50.0);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
    // indicate we will need to refresh the screen
    trigger_display_refresh();
}

static inline void set_heatmap_cell(int id, float north, float east, float south float west)
{
    send_to_chip(i, 0x21, 1, 0, 0, 0, 4, int(north * 65536),
	    int(east * 65536), int(south * 65536), int(west * 65536));
}

// Called when keys are pressed
void keyDown(unsigned char key, int x, int y)
{
    switch (tolower(key)) {
    case 'f':
	if (fullscreen) {
	    windowBorder = oldwindowBorder;	// restore old bordersize
	    windowWidth -= keyWidth;		// recover the key area
	    plotWidth = windowWidth - 2 * windowBorder - keyWidth;
	} else {
	    oldwindowBorder = windowBorder;	// used as border disappears when going full-screen
	    windowBorder = 0;			// no borders around the plots
	    windowWidth += keyWidth;		// take over the area used for the key too
	    plotWidth = windowWidth - keyWidth;
	}
	fullscreen = !fullscreen;
	break;
    case 'c':
	cleardown();			// clears the output when 'c' key is pressed
	break;
    case 'q':
	safelyshut();
	break;
    case '"': {
	// send pause packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 1;
	freezetime = timestamp();	// get time now in us
	needtorebuildmenu = 1;
	break;
    }
    case 'p': {
	// send resume/restart packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 0;
	needtorebuildmenu = 1;
	break;
    }

    case 'b':
	gridlines = !gridlines
	break;
    case '#':
	// toggles the plotting of values
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

    case '+':
	if (livebox == NORTH) {
	    alternorth += ALTERSTEPSIZE;
	} else if (livebox == EAST) {
	    altereast += ALTERSTEPSIZE;
	} else if (livebox == SOUTH) {
	    altersouth += ALTERSTEPSIZE;
	} else if (livebox == WEST) {
	    alterwest += ALTERSTEPSIZE;
	}
	break;
    case '-':
	if (livebox == NORTH) {
	    alternorth -= ALTERSTEPSIZE;
	} else if (livebox == EAST) {
	    altereast -= ALTERSTEPSIZE;
	} else if (livebox == SOUTH) {
	    altersouth -= ALTERSTEPSIZE;
	} else if (livebox == WEST) {
	    alterwest -= ALTERSTEPSIZE;
	}
	break;

    case 'n':
	if (editmode) {
	    livebox = (livebox == NORTH ? -1 : NORTH);
	}
	break;
    case 'e':
	if (editmode) {
	    livebox = (livebox == EAST ? -1 : EAST);
	}
	break;
    case 's':
	if (editmode) {
	    livebox = (livebox == SOUTH ? -1 : SOUTH);
	}
	break;
    case 'w':
	if (editmode) {
	    livebox = (livebox == WEST ? -1 : WEST);
	}
	break;
    case 'a':
	if (!editmode) {
	    editmode = 1;
	    livebox = -1;
	}
	break;
    case 'g':
	if (editmode) {
	    livebox = -1;
	    // send temperature packet out
	    for (int i = 0 ; i < all_desired_chips() ; i++) {
		set_heatmap_cell(i, alternorth, altereast, altersouth,
			alterwest);
	    }
	}
	break;
    case '9':
	// special case to randomise the heatmap
	// send temperature packet out (reset to zero).
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    alternorth = rand(lowwatermark, highwatermark);
	    altereast = rand(lowwatermark, highwatermark);
	    altersouth = rand(lowwatermark, highwatermark);
	    alterwest = rand(lowwatermark, highwatermark);
	    set_heatmap_cell(i, alternorth, altereast, altersouth, alterwest);
	}
	break;
    case '0':
	// special case to zero the heatmap
	livebox = -1;
	if (alternorth < 1.0 && altereast < 1.0 && altersouth < 1.0
		&& alterwest < 1.0) {
	    // if very low -reinitialise
	    alternorth = 40.0;
	    altereast = 10.0;
	    altersouth = 10.0;
	    alterwest = 40.0;
	} else {
	    // else reset to zero
	    alternorth = 0.0;
	    altereast = 0.0;
	    altersouth = 0.0;
	    alterwest = 0.0;
	}
	// send temperature packet out (reset to zero).
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    set_heatmap_cell(i, alternorth, altereast, altersouth, alterwest);
	}
	break;
    }
    // indicate we will need to refresh the screen
    trigger_display_refresh();
}

// These two constants ought to be defined by GLUT, but aren't
#define SCROLL_UP	3
#define SCROLL_DOWN	4

static inline bool in_box(int box, int x, int y, int boxsize=40, int gap=10)
{
    int xorigin = windowWidth - 3 * (boxsize + gap);
    int yorigin = windowHeight - gap - boxsize;

    return x >= xorigin + box * (boxsize + gap)
	    && x < xorigin + box * (boxsize + gap) + boxsize
	    && windowHeight - y < yorigin + boxsize
	    && windowHeight - y >= yorigin;
}

static inline bool in_box2(int boxx, int boxy, int x, int y, int boxsize=40, int gap=10)
{
    int xorigin = windowWidth - (boxx + 1) * (boxsize + gap);
    int yorigin = windowHeight - gap - boxsize;

    return x >= xorigin
	    && windowHeight - y >= yorigin + boxy * (boxsize + gap)
	    && x < xorigin + boxsize
	    && windowHeight - y < yorigin + boxsize + boxy * (boxsize + gap);
}

static inline void handle_control_box_click(int x, int y)
{
    if (in_box(0, x, y) && !freezedisplay) {
	// send pause packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 1;
	freezetime = timestamp(); // get time now in us
	// indicate we will need to refresh the screen
	trigger_display_refresh();
	needtorebuildmenu = 1;
    }
    if (in_box(1, x, y) && freezedisplay) {
	// send resume/restart packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 0;
	// indicate we will need to refresh the screen
	trigger_display_refresh();
	needtorebuildmenu = 1;
    }
    if (in_box(2, x, y)) {
	safelyshut();
    }
}

static inline int get_box_id(int x, int y) {
    for (int boxx = 0 ; boxx < controlboxes ; boxx++) {
	for (int boxy = 0 ; boxy < controlboxes ; boxy++) {
	    if (in_box2(boxx, boxy, x, y)) {
		return boxx * controlboxes + boxy;
	    }
	}
    }
    return -1;
}

static inline void handle_main_box_click(int x, int y)
{
    int selectedbox = get_box_id(x, y);
    switch (selectedbox) {
    case CENTRE:
	livebox = -1;
	if (!editmode) {
	    // if !editmode then if box ==4 editmode=1, livebox =0, calculate side values to edit;
	    editmode = 1;
	    break;
	}
	// if editmode then if box ==4 editmode=0, send command;
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    set_heatmap_cell(i, alternorth, altereast, altersouth, alterwest); // send temp pack
	}
	break;
    case NORTH:
    case EAST:
    case SOUTH:
    case WEST:
	if (editmode) {
	    if (selectedbox == livebox) {
		// if editmode and box==livebox livebox=0
		livebox = -1;
	    } else {
		// if editmode and box!=livebox livebox=box
		livebox = selectedbox;
	    }
	    break;
	}
    default:
	return;
    }
    // indicate we will need to refresh the screen
    trigger_display_refresh();
}

// called when something happens with the mouse
void mousehandler(int button, int state, int x, int y)
{
    if (state != GLUT_DOWN) {
	return;
    }

    if (button == GLUT_LEFT_BUTTON) {
	handle_control_box_click(x, y);
	hanlde_main_box_click(x, y);

	// if you didn't manage to do something useful, then likely greyspace
	// around the figure was clicked (should now deselect any selection)
	if (!somethingtoplot) {
	    livebox = -1;
	    // indicate we will need to refresh the screen
	    trigger_display_refresh();
	    rebuildmenu();
	}
    } else if (button == SCROLL_UP) {
	switch (livebox) {
	case NORTH:
	    alternorth += ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case EAST:
	    altereast += ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case SOUTH:
	    altersouth += ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case WEST:
	    alterwest += ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	}
    } else if (button == SCROLL_DOWN) {
	// if scroll down, decrement variable
	switch (livebox) {
	case NORTH:
	    alternorth -= ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case EAST:
	    altereast -= ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case SOUTH:
	    altersouth -= ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	case WEST:
	    alterwest -= ALTERSTEPSIZE;
	    trigger_display_refresh();
	    break;
	}
    }
}

// Called repeatedly, once per OpenGL loop
void idleFunction()
{
    if (needtorebuildmenu && !menuopen) {
	filemenu();
	rebuildmenu();    // if menu is not open we can make changes
	needtorebuildmenu = 0;
    }

    int usecperframe = 1000000 / MAXFRAMERATE;		// us target per frame
    struct timespec ts; // used for calculating how long to wait for next frame
    int64_t nowtime, howlongrunning, howlongtowait;	// for timings

    if (plotWidth != windowWidth - 2 * windowBorder - keyWidth) {
	printf(
		"NOT SAME: windowWidth-(2*windowBorder)-keyWidth=%d, plotWidth=%d.\n",
		windowWidth - 2 * windowBorder - keyWidth, plotWidth);
    }

    howlongtowait = starttimez + counter * (int64_t) usecperframe
	    - timestamp(); // how long in us until we need to draw the next frame

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

    nowtime = timestamp();    // get time now in us
    howlongrunning = nowtime - starttimez; // how long in us since visualation started running.
    // idle until the next frame/s interval is due

    if (printpktgone && nowtime > printpktgone + 1000000) {
	printpktgone = 0; // if packet send message has been displayed for more than 1s, stop its display
	trigger_display_refresh();	// force refresh screen
    }

    // force the refresh for this frame timing (even if nothing has changed!)
    trigger_display_refresh();

    if (somethingtoplot) {
	display(); // update the display - will be timered inside this function to get desired FPS
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

#include "menu.cpp"

void safelyshut(void)
{
    // in some circumstances this gets run twice, therefore we check for
    // this (particularly the frees!)
    if (!safelyshutcalls) {
	if (spinnakerboardport != 0) {
	    for (int i = 0 ; i < all_desired_chips() ; i++) {
		// send exit packet out if we are interacting
		send_to_chip(i, 0x21, 0, 0, 0, 0, 4, 0, 0, 0, 0);
	    }
	}

	open_or_close_output_file();	// Close down any open output file
	if (fileinput != NULL) {
	    fclose(fileinput);		// deal with input file
	}

	finalise_memory();

	safelyshutcalls++;	// note that this routine has been run before
    }
    exit(0);			// kill program dead
}

void open_or_close_output_file(void)
{
    time_t rawtime;
    struct tm * timeinfo;
    char filenamebuffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (fileoutput == NULL) {		// If file isn't already open, so this is a request to open it
	if (outputfileformat == 2) {	//SAVE AS NEUROTOOLS FORMAT
	    strftime(filenamebuffer, 80, "packets-20%y%b%d_%H%M.neuro",
		    timeinfo);
	    printf("Saving spike packets in this file:\n       %s\n",
		    filenamebuffer);
	    fileoutput = fopen(filenamebuffer, "w");
	    fprintf(fileoutput, "# first_id =          \n");	// ! writing header for neurotools format file
	    fprintf(fileoutput, "# n =          \n");		// ! writing header for neurotools format file
	    fprintf(fileoutput, "# dt = 1.0\n");		// ! writing header for neurotools format file
	    fprintf(fileoutput, "# dimensions = [          ]\n"); // ! writing header for neurotools format file
	    fprintf(fileoutput, "# last_id =          \n");	// ! writing header for neurotools format file
	} else if (outputfileformat == 1) {	//SAVE AS SPINN (UDP Payload) FORMAT
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

	    fseek(fileoutput, 13, SEEK_SET);		// pos 13 First ID
	    fprintf(fileoutput, "%d", minneuridrx);	// write lowest detected neurid
	    fseek(fileoutput, 29, SEEK_SET);		// pos 29 n number of neurons-1
	    fprintf(fileoutput, "%d", maxneuridrx - minneuridrx); // write number of neurons in range
	    fseek(fileoutput, 67, SEEK_SET);		// pos 67 dimensions number of neurons-1
	    fprintf(fileoutput, "%d", maxneuridrx - minneuridrx); // write number of neurons in range
	    fseek(fileoutput, 90, SEEK_SET);		// pos 90 Last ID
	    fprintf(fileoutput, "%d", maxneuridrx);	// write highest detected neurid
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
	// default filename if not supplied by the user
	configfn = (char*) "visparam.ini";
    }

    // recover the parameters from the file used to configure this visualisation
    paramload(configfn);

    if (l2gfn != nullptr && g2lfn != nullptr) {	// if both translations are provided
	readmappings(l2gfn, g2lfn);		// read mappings file into array
    }

    for (int ii = 0 ; ii < XDIMENSIONS * YDIMENSIONS ; ii++) {
	int xcoordinate, ycoordinate, index;
	convert_index_to_coord(ii, xcoordinate, ycoordinate);
	index = convert_coord_to_index(xcoordinate, ycoordinate);
    }

    cleardown(); // reset the plot buffer to something sensible (i.e. 0 to start with)
    starttimez = timestamp();
    keepalivetime = starttimez;

    for (int j = 0 ; j < HISTORYSIZE ; j++) {
	for (int i = 0 ; i < xdim * ydim ; i++) {
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
	pthread_t p1;
	fileinput = fopen(replayfn, "rb");
	if (fileinput == NULL) {
	    fprintf(stderr, "cannot read replay file \"%s\"\n", replayfn);
	    exit(2);
	}  // check if file is readable

	// setup with target 127.0.0.1 on right port if not already set
	if (!spinnakerboardipset) {
	    inet_aton("127.0.0.1", &spinnakerboardip);
	}
	spinnakerboardport = SDPPORT; // SDPPORT is used for outgoing cnnx
	spinnakerboardipset++;
	init_sdp_sender();
	printf("Set up to receive internally from %s on port: %d\n",
		inet_ntoa(spinnakerboardip), SDPPORT);
	// Launch the file receiver
	pthread_create(&p1, NULL, load_stimulus_data_from_file, NULL);
    }

    // this sets up the thread that can come back to here from type
    pthread_t p2;
    init_sdp_listening();	//initialization of the port for receiving SDP frames
    pthread_create(&p2, NULL, input_thread_SDP, NULL); // away the SDP network receiver goes

    glutInit(&argc, argv);	// Initialise OpenGL

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

    glutMainLoop();		// Enter the main OpenGL loop
    printf("goodbye");

    return 0;
}
