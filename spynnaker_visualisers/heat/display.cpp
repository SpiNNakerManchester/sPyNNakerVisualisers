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
	glutStrokeCharacter(font_style, str[i]);
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
    color(R, G, B);

    return fillcolour;
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
    color(BLACK);
    const int keybase = windowBorder + 0.20 * (windowHeight - windowBorder); // bottom of the key
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
    // key is only printed if big enough to print
    if (windowHeight - windowBorder - keybase > 0) {
	for (uint i = 0 ; i < windowHeight - windowBorder - keybase ; i++) {
	    float temperaturehere = 1.0;
	    if (linechunkiness > 0.0) {
		temperaturehere = i / linechunkiness + lowwatermark;
	    }
	    colour_calculator(temperaturehere, highwatermark, lowwatermark);

	    glBegin (GL_LINES);
	    glVertex2f(windowWidth - 65, i + keybase); // rhs
	    glVertex2f(windowWidth - 65 - keyWidth, i + keybase); // lhs
	    glEnd();      //draw_line;

	    float positiveoffset = temperaturehere - lowwatermark;
	    if (positiveoffset >= interval * multipleprinted) {
		color(BLACK);
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

	color(BLACK);
	glLineWidth(2.0);

	glBegin (GL_LINE_LOOP);
	glOpenBoxVertices(windowWidth - 65 - keyWidth, keybase,
		windowWidth - 65, windowHeight - windowBorder);
	glEnd();      //draw_line loop around the key;

	glLineWidth(1.0);
    }
}

static inline void display_controls()
{
    const unsigned boxsize = 40, gap = 10;

    for (int box = 0 ; box < 3 ; box++) {
	int xorigin = windowWidth - 3 * (boxsize + gap), yorigin =
		windowHeight - gap - boxsize;
	// local to this scope

	if ((!freezedisplay && box == 0) || (freezedisplay && box == 1)
		|| box == 2) {
	    color(BLACK);

	    glBegin (GL_QUADS);
	    glRectVertices(xorigin + box * (boxsize + gap), yorigin + boxsize,
		    xorigin + box * (boxsize + gap) + boxsize, yorigin);
	    glEnd();

	    color(RED);
	    glLineWidth(15.0);

	    // now draw shapes on boxes
	    if (box == 0) {
		glBegin(GL_QUADS);
		glRectVertices(xorigin + gap, yorigin + boxsize - gap,
			xorigin + (boxsize + gap) / 2 - gap, yorigin + gap);
		glRectVertices(xorigin + (boxsize - gap) / 2 + gap,
			yorigin + boxsize - gap, xorigin + boxsize - gap,
			yorigin + gap);
		glEnd();
	    } else if (box == 1) {
		glBegin (GL_TRIANGLES);
		glVertex2f(xorigin + boxsize + 2 * gap,
			yorigin + boxsize - gap); // topleft
		glVertex2f(xorigin + 2 * boxsize, yorigin + boxsize / 2); // centreright
		glVertex2f(xorigin + boxsize + gap * 2, yorigin + gap); // bottomleft
		glEnd();
	    } else if (box == 2) {
		glBegin (GL_LINES);
		glVertex2f(xorigin + 2 * boxsize + 3 * gap,
			yorigin + boxsize - gap); // topleft
		glVertex2f(xorigin + 3 * boxsize + gap, yorigin + gap); // bottomright
		glVertex2f(xorigin + 3 * boxsize + gap,
			yorigin + boxsize - gap); // topright
		glVertex2f(xorigin + 2 * boxsize + 3 * gap, yorigin + gap); // bottomleft
		glEnd();
	    }

	    glLineWidth(1.0);
	}
    }
}

static inline void display_gridlines(float xsize, float ysize)
{
    color(GREY);

    // if not going to completely obscure the data
    if (xsize > 3.0) {
	// vertical grid lines
	for (unsigned xcord = 0 ; xcord <= xdim ; xcord++) {
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
	for (unsigned ycord = 0 ; ycord <= ydim ; ycord++) {
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
    for (unsigned box = 0 ; box < controlboxes * controlboxes ; box++) {
	int boxx = box / controlboxes, boxy = box % controlboxes;
	if (boxx != 1 && boxy != 1) {
	    continue;
	}
	//only plot NESW+centre
	color(BLACK);
	if (int(box) == livebox) {
	    color(CYAN);
	}
	if (editmode || box == CENTRE) {
	    if (box == CENTRE && editmode) {
		color(GREEN); // go button is green!
	    }

	    glBegin (GL_QUADS);
	    glRectVertices(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize,
		    windowWidth - (boxx + 1) * (boxsize + gap) + boxsize,
		    yorigin + boxy * (boxsize + gap));
	    glEnd();  // alter button
	}

	if (box == CENTRE) {
	    color(WHITE);
	    printgl(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize / 2 - 5,
		    GLUT_BITMAP_8_BY_13, editmode ? " Go!" : "Alter");
	} else {
	    float currentvalue = 0.0;
	    if (box == NORTH) {
		currentvalue = alternorth;
	    } else if (box == EAST) {
		currentvalue = altereast;
	    } else if (box == SOUTH) {
		currentvalue = altersouth;
	    } else if (box == WEST) {
		currentvalue = alterwest;
	    }
	    color(editmode && int(box) != livebox ? WHITE : BLACK);
	    printgl(windowWidth - (boxx + 1) * (boxsize + gap),
		    yorigin + boxy * (boxsize + gap) + boxsize / 2 - 5,
		    GLUT_BITMAP_8_BY_13, "%3.1f", currentvalue);
	}
    }
}

static inline void display_mini_pixel(
	float tileratio,
	int i,
	int ii,
	int xcord,
	int ycord)
{
    float ysize = max(1.0F, float(windowBorder - 6 * gap) / ydim);
    float xsize = max(1.0F, float(ysize * tileratio)); // draw little / mini tiled version in btm left - pixel size
    if (is_defined(immediate_data[ii])) { // only plot if data is valid
	// work out what colour we should plot - sets 'ink' plotting colour
	colour_calculator(immediate_data[ii], highwatermark, lowwatermark);

	glBegin (GL_QUADS); // draw little tiled version in btm left
	glRectVertices(2 * gap + xcord * xsize, 2 * gap + ycord * ysize,
		2 * gap + (xcord + 1) * xsize, 2 * gap + (ycord + 1) * ysize);
	glEnd(); // this plots the basic quad box filled as per colour above
    }

    if (livebox == i) { // draw outlines for selected box in little / mini version
	glLineWidth(1.0);
	color(BLACK);

	glBegin (GL_LINE_LOOP);
	glOpenBoxVertices(2 * gap + xcord * xsize, 2 * gap + ycord * ysize,
		2 * gap + (xcord + 1) * xsize, 2 * gap + (ycord + 1) * ysize);
	glEnd(); // this plots the external black outline of the selected tile

	color(WHITE);

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
    float magnitude = colour_calculator(immediate_data[ii], highwatermark,
	    lowwatermark);

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
	color(magnitude <= 0.6 ? WHITE : BLACK);
	printglstroke(windowBorder - 20 + (xcord + 0.5) * xsize,
		windowBorder - 6 + (ycord + 0.5) * ysize, 0.12, 0, "%3.2f",
		immediate_data[ii]); // normal
    }
}

// display function, called whenever the display window needs redrawing
void display(void)
{
    glPointSize(1.0);
    counter++;		// how many frames have we plotted in our history
    glLoadIdentity();
    //glutPostRedisplay();

    glClearColor(0.8, 0.8, 0.8, 1.0);	// background colour - grey surround
    glClear (GL_COLOR_BUFFER_BIT);

    color(BLACK);

    // titles and labels are only printed if border is big enough
    if (printlabels && !fullscreen) {
	display_titles_labels();
    }

    // scale all the values to plottable range
    for (unsigned i = 0 ; i < xdim * ydim ; i++) {
	if (is_defined(immediate_data[i])) {    // is valid
	    immediate_data[i] = clamp(MINDATAFLOAT, immediate_data[i],
		    MAXDATAFLOAT);
	    if (is_board_address_set()) {
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

    for (unsigned i = 0 ; i < xdim * ydim ; i++) {
	int ii = coordinate_manipulate(i); // if any manipulation of how the data is to be plotted is required, do it
	int xcord, ycord;
	convert_index_to_coord(i, xcord, ycord); // find out the (x,y) coordinates of where to plot this data

	// if required, plot tiled mini version in bottom left
	if (!fullscreen) {
	    display_mini_pixel(tileratio, i, ii, xcord, ycord);
	}

	xsize = plotWidth / float(xdim);
	if (xsize < 1.0) {
	    xsize = 1.0;
	}
	ysize = float(windowHeight - 2 * windowBorder) / ydim; // changed for dynamic reshaping

	// basic plot
	display_pixel(xsize, ysize, ii, xcord, ycord);

    }
    color(BLACK);

    // scrolling modes x scale and labels and gridlines
    if (gridlines) {
	display_gridlines(xsize, ysize);
    }

    // only print if not in fullscreen mode
    if (!fullscreen) {
	display_key();

	// for display of visualisation screen controls
	display_controls();

	if (printpktgone) {
	    color(BLACK);
	    if (is_board_address_set()) {
		printgl(windowWidth - 3 * (boxsize + gap) + 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, "Packet Sent");
	    } else {
		printgl(windowWidth - 3 * (boxsize + gap) - 5,
			windowHeight - gap - boxsize - 25,
			GLUT_BITMAP_8_BY_13, "Target Unknown");
	    }
	}

	display_boxes();
    }

    glutSwapBuffers();			// no flickery graphics
    somethingtoplot = 0;		// indicate we have finished plotting
}
