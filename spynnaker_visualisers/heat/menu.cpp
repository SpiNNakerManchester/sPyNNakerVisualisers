enum menuentries_t {
    MENU_SEPARATOR,
    // File Submenu Items
    FILE_SAVE_SPINN,
    FILE_SAVE_NEURO,
    FILE_RESUME,
    FILE_PAUSE,
    FILE_END,
    // Transform Submenu Items
    XFORM_XFLIP,
    XFORM_YFLIP,
    XFORM_VECTORFLIP,
    XFORM_ROTATEFLIP,
    XFORM_REVERT,
    // Main Menu Items
    MENU_RASTER_OFF,
    MENU_RASTER_ON,
    MENU_RASTER_OFF_ALL,
    MENU_BORDER_TOGGLE,
    MENU_NUMBER_TOGGLE,
    MENU_FULLSCREEN_TOGGLE,
    MENU_PAUSE,
    MENU_RESUME,
    MENU_QUIT
};

static void menu_callback(int value);

void filemenu(void)
{
    glutDestroyMenu(filesubmenu);
    filesubmenu = glutCreateMenu(menu_callback);
    if (outputfileformat == 0) {		// no savefile open
	glutAddMenuEntry("Save Input Data in .spinn format (replayable)",
		FILE_SAVE_SPINN);		// start saving data in spinn
	glutAddMenuEntry(
		"Save Input Spike Data as write-only .neuro Neurotools format",
		FILE_SAVE_NEURO);		// or neurotools format
    } else {					// savefile open
	if (writingtofile == 2) {
	    glutAddMenuEntry("Resume Saving Data to file", FILE_RESUME); // and paused
	} else {
	    glutAddMenuEntry("Pause Saving Data to file", FILE_PAUSE); // or running
	}
	glutAddMenuEntry("End saving Data to file", FILE_END); // closefile out
    }
}

void transformmenu(void)
{
    glutDestroyMenu(transformsubmenu);
    transformsubmenu = glutCreateMenu(menu_callback);
    glutAddMenuEntry("(X) Mirror (left to right swop)", XFORM_XFLIP);
    glutAddMenuEntry("(Y) Reflect (top to bottom swop)", XFORM_YFLIP);
    glutAddMenuEntry("(V) Vector Swop (Full X+Y Reversal)", XFORM_VECTORFLIP);
    glutAddMenuEntry("90 (D)egree Rotate Toggle", XFORM_ROTATEFLIP);
    glutAddMenuEntry("(C) Revert changes back to default", XFORM_REVERT);
}

void rebuildmenu(void)
{
    glutDestroyMenu(RHMouseMenu);
    RHMouseMenu = glutCreateMenu(menu_callback);

    glutAddSubMenu("Transform Plot", transformsubmenu);
    glutAddSubMenu("Save Data Operations", filesubmenu);

    glutAddMenuEntry("-----", MENU_SEPARATOR);
    if (gridlines) {
	glutAddMenuEntry("Grid (B)orders off", MENU_BORDER_TOGGLE);
    } else {
	glutAddMenuEntry("Grid (B)orders on", MENU_BORDER_TOGGLE);
    }
    if (plotvaluesinblocks) {
	glutAddMenuEntry("Numbers (#) off", MENU_NUMBER_TOGGLE);
    } else {
	glutAddMenuEntry("Numbers (#) on", MENU_NUMBER_TOGGLE);
    }
    if (fullscreen) {
	glutAddMenuEntry("(F)ull Screen off", MENU_FULLSCREEN_TOGGLE);
    } else {
	glutAddMenuEntry("(F)ull Screen on", MENU_FULLSCREEN_TOGGLE);
    }

    glutAddMenuEntry("-----", MENU_SEPARATOR);

    if (!freezedisplay) {
	glutAddMenuEntry("(\") Pause Plot", MENU_PAUSE);
    } else {
	glutAddMenuEntry("(P)lay / Restart Plot", MENU_RESUME);
    }
    glutAddMenuEntry("(Q)uit", MENU_QUIT);
    glutAttachMenu (GLUT_RIGHT_BUTTON);
}

static void menu_callback(int value)
{
    switch (value) {
    case FILE_SAVE_SPINN:
	outputfileformat = 1;
	open_or_close_output_file();	// start saving data in spinn
	break;
    case FILE_SAVE_NEURO:
	outputfileformat = 2;
	open_or_close_output_file();	// start saving data in neurotools format
	break;
    case FILE_RESUME:
	writingtofile = 0;		// savefile open and paused
	printf("Recording resumed...  ");
	break;
    case FILE_PAUSE:
	writingtofile = 2;		// savefile open and running
	printf("Recording paused...  ");
	break;
    case FILE_END:
	open_or_close_output_file();	// closefile out
	break;
    case XFORM_XFLIP:
	xflip = !xflip;
	break;
    case XFORM_YFLIP:
	yflip = !yflip;
	break;
    case XFORM_VECTORFLIP:
	vectorflip = !vectorflip;
	break;
    case XFORM_ROTATEFLIP:
	rotateflip = !rotateflip;
	break;
    case XFORM_REVERT:
	cleardown();
	break;
    case MENU_RASTER_OFF:
	for (int i = 0 ; i < xdim * ydim ; i++) {
	    sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
	    sleeplet();
	    printf("%d: ", i);
	}
	printf("\n");
	if (win2 != 0) {
	    destroy_new_window();		// if spawned window does exist then close it
	}
	rasterpopulation = -1;
	somethingtoplot = 1;
	break;
    case MENU_RASTER_ON:
	for (int i = 0 ; i < xdim * ydim ; i++) {
	    sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster for all populations
	    sleeplet();
	    printf("%d, ", i);
	}
	printf("\n");
	if (win2 == 0) {
	    create_new_window();		// if window doesn't already exist then create it on demand
	}
	sdp_sender(0, 0x80 + livebox + 1, 258, 1, 4, 0, 0); // send message to turn on bit 4 (raster on)
	rasterpopulation = livebox;
	somethingtoplot = 1;
	break;
    case MENU_RASTER_OFF_ALL:
	rasterpopulation = -1;
	for (int i = 0 ; i < xdim * ydim ; i++) {
	    sdp_sender(0, 0x80 + i + 1, 258, 0, 4, 0, 0); // turn off raster on all populations
	    sleeplet();
	    printf("%d, ", i);
	}
	printf("\n");
	if (win2 != 0) {
	    destroy_new_window();		// if spawned window does exist then close it
	}
	livebox = -1;
	somethingtoplot = 1;
	break;
    case MENU_BORDER_TOGGLE:
	gridlines = !gridlines;
	break;
    case MENU_NUMBER_TOGGLE:
	plotvaluesinblocks = !plotvaluesinblocks;
	break;
    case MENU_FULLSCREEN_TOGGLE:
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
    case MENU_PAUSE:
	// send pause packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 2, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 1;
	freezetime = timestamp();		// get time now in us
	break;
    case MENU_RESUME:
	// send resume/restart packet out
	for (int i = 0 ; i < all_desired_chips() ; i++) {
	    send_to_chip(i, 0x21, 3, 0, 0, 0, 4, 0, 0, 0, 0);
	}
	freezedisplay = 0;
	break;
    case MENU_QUIT:
	safelyshut();
	break;
    }

    needtorebuildmenu = 1;
}

void logifmenuopen(int status, int x, int y)
{
    menuopen = int(status == GLUT_MENU_IN_USE);
    if (!menuopen && needtorebuildmenu) {
	// if menu is not open we can make changes
	filemenu();
	rebuildmenu();
	needtorebuildmenu = 0;
    }
}
