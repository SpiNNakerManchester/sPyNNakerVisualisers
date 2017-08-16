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

// Called when keys are pressed
void keyDown(unsigned char key, int x, int y)
{
    use(x);
    use(y);

    switch (tolower(key)) {
    case 'f':
	if (fullscreen) {
	    windowBorder = oldwindowBorder;	// restore old bordersize
	    windowWidth -= keyWidth;		// recover the key area
	    plotWidth = windowWidth - 2 * windowBorder - keyWidth;
	} else {
	    oldwindowBorder = windowBorder;// used as border disappears when going full-screen
	    windowBorder = 0;			// no borders around the plots
	    windowWidth += keyWidth;// take over the area used for the key too
	    plotWidth = windowWidth - keyWidth;
	}
	fullscreen = !fullscreen;
	break;
    case 'c':
	cleardown();		// clears the output when 'c' key is pressed
	break;
    case 'q':
	safelyshut();
	break;
    case '"':
	// send pause packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 1;
	freezetime = timestamp();	// get time now in us
	needtorebuildmenu = 1;
	break;
    case 'p':
	// send resume/restart packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 0;
	needtorebuildmenu = 1;
	break;

    case 'b':
	gridlines = !gridlines;
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

static inline bool in_control_box(unsigned box, int x, int y)
{
    const unsigned boxsize = 40, gap = 10;
    unsigned xorigin = windowWidth - 3 * (boxsize + gap);
    unsigned yorigin = windowHeight - gap - boxsize;

    return x >= int(xorigin + box * (boxsize + gap))
	    && x < int(xorigin + box * (boxsize + gap) + boxsize)
	    && int(windowHeight - y) < int(yorigin + boxsize)
	    && int(windowHeight - y) >= int(yorigin);
}

static inline void handle_control_box_click(int x, int y)
{
    if (in_control_box(0, x, y) && !freezedisplay) {
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
    if (in_control_box(1, x, y) && freezedisplay) {
	// send resume/restart packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 0;
	// indicate we will need to refresh the screen
	trigger_display_refresh();
	needtorebuildmenu = 1;
    }
    if (in_control_box(2, x, y)) {
	safelyshut();
    }
}

static inline bool in_box(unsigned boxx, unsigned boxy, int x, int y)
{
    unsigned xorigin = windowWidth - (boxx + 1) * (boxsize + gap);
    unsigned yorigin = windowHeight - gap - boxsize;

    return x >= int(xorigin)
	    && int(windowHeight - y) >= int(yorigin + boxy * (boxsize + gap))
	    && x < int(xorigin + boxsize)
	    && int(windowHeight - y)
		    < int(yorigin + boxsize + boxy * (boxsize + gap));
}

static inline int get_box_id(int x, int y)
{
    for (unsigned boxx = 0 ; boxx < controlboxes ; boxx++) {
	for (unsigned boxy = 0 ; boxy < controlboxes ; boxy++) {
	    if (in_box(boxx, boxy, x, y)) {
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
	handle_main_box_click(x, y);

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
	rebuildmenu();    // if menu is not open we can make changes
	needtorebuildmenu = 0;
    }

    int usecperframe = 1000000 / MAXFRAMERATE;	// us target per frame

    // if we are ahead of schedule sleep for a bit
    auto howlongtowait = starttimez + counter * (int64_t) usecperframe
	    - timestamp();
    if (howlongtowait > 0) {
	struct timespec ts;
	ts.tv_sec = howlongtowait / 1000000;
	ts.tv_nsec = (howlongtowait % 1000000) * 1000;
	nanosleep(&ts, NULL);
    }

    // if packet send message has been displayed for more than 1s, stop its display
    if (printpktgone && timestamp() > printpktgone + 1000000) {
	printpktgone = 0;
    }

    // force the refresh for this frame timing (even if nothing has changed!)
    trigger_display_refresh();

    // update the display - will be timed inside this function to get desired FPS
    if (somethingtoplot) {
	display();
    }
}
