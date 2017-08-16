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
    MENU_BORDER_TOGGLE,
    MENU_NUMBER_TOGGLE,
    MENU_FULLSCREEN_TOGGLE,
    MENU_PAUSE,
    MENU_RESUME,
    MENU_QUIT
};

static void menu_callback(int value)
{
    switch (value) {
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

static void rebuildmenu(void)
{
    glutDestroyMenu(RHMouseMenu);
    RHMouseMenu = glutCreateMenu(menu_callback);

    glutAddMenuEntry("(X) Mirror (left to right swap)", XFORM_XFLIP);
    glutAddMenuEntry("(Y) Reflect (top to bottom swap)", XFORM_YFLIP);
    glutAddMenuEntry("(V) Vector Swap (Full X+Y Reversal)", XFORM_VECTORFLIP);
    glutAddMenuEntry("90 (D)egree Rotate Toggle", XFORM_ROTATEFLIP);
    glutAddMenuEntry("(C) Revert changes back to default", XFORM_REVERT);
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

static void logifmenuopen(int status, int x, int y)
{
    use(x);
    use(y);

    menuopen = int(status == GLUT_MENU_IN_USE);
    if (!menuopen && needtorebuildmenu) {
	// if menu is not open we can make changes
	rebuildmenu();
	needtorebuildmenu = 0;
    }
}
