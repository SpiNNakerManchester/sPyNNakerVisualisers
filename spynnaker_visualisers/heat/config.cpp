#include "state.h"

static inline void get_setting(
	config_setting_t *setting,
	const char *name,
	int &var)
{
    long long VALUE = 0;

    if (config_setting_lookup_int64(setting, name, &VALUE)) {
	var = (int) VALUE;
    }
}

static inline void get_setting(
	config_setting_t *setting,
	const char *name,
	float &var)
{
    float VALUE = 0;

    if (config_setting_lookup_float(setting, name, &VALUE)) {
	var = VALUE;
    }
}

#define GET_SETTING(var) get_setting(setting, #var, var)

int paramload(char* config_file_name)
{
    // check if visparam exists
    // if not then use in-built defaults
    // if it does then deal with it

    config_t cfg; /*Returns all parameters in this structure */
    config_setting_t *setting;
    const char *paramblock;
    const char *titletemp;
    int tmp;
    //const char *config_file_name = "visparam.ini";

    config_init(&cfg); /*Initialization */

    if (!config_read_file(&cfg, config_file_name)) {
	printf(
		"No readable %s in the local directory - configuration defaulted to 48-chip HEATMAP.\n",
		config_file_name);
	setting != NULL;
	//config_destroy(&cfg);
	//return -1;
    } else {
	/* Get the simulation parameters to use. */
	if (config_lookup_string(&cfg, "simparams", &paramblock)) {
	    printf("Sim params specified: %s\n", paramblock);
	} else {
	    printf("No 'simparams' settings in configuration file.\n");
	}
	setting = config_lookup(&cfg, paramblock); /*Read the simulation parameters group*/
    }

    if (setting != NULL) {
	if (!(config_setting_lookup_string(setting, "TITLE", &titletemp))) {
	    titletemp = "NO SIMULATION TITLE SUPPLIED";
	}

	GET_SETTING(SIMULATION);
	GET_SETTING(WINBORDER);
	GET_SETTING(WINHEIGHT);
	GET_SETTING(WINWIDTH);
	GET_SETTING(TIMEWINDOW);
	GET_SETTING(KEYWIDTH);
	GET_SETTING(DISPLAYKEY);
	GET_SETTING(DISPLAYXLABELS);
	GET_SETTING(DISPLAYYLABELS);
	GET_SETTING(BLACKBACKGROUND);
	GET_SETTING(INITZERO);
	GET_SETTING(XDIMENSIONS);
	GET_SETTING(YDIMENSIONS);
	GET_SETTING(EACHCHIPX);
	GET_SETTING(EACHCHIPY);
	GET_SETTING(N_PER_PROC);
	GET_SETTING(ID_OFFSET);
	GET_SETTING(XCHIPS);
	GET_SETTING(YCHIPS);
	// if not explicitly defined, we assume display will be chipwise

	GET_SETTING(HISTORYSIZE);
	GET_SETTING(MAXRASTERISEDNEURONS);
	GET_SETTING(XFLIP);
	GET_SETTING(YFLIP);
	GET_SETTING(VECTORFLIP);
	GET_SETTING(ROTATEFLIP);
	GET_SETTING(HIWATER);
	GET_SETTING(LOWATER);
	GET_SETTING(DYNAMICSCALE);
	GET_SETTING(PERCENTAGESCALE);
	GET_SETTING(STARTCOLOUR);
	GET_SETTING(STARTMODE);
	GET_SETTING(LABELBYCHIP);
	GET_SETTING(PLAYPAUSEXIT);
	GET_SETTING(DISPLAYMINIPLOT);
	GET_SETTING(INTERACTION);
	GET_SETTING(MAXFRAMERATE);
	GET_SETTING(PLOTONLYONDEMAND);
	GET_SETTING(SDPPORT);
	GET_SETTING(FIXEDPOINT);
	GET_SETTING(BITSOFPOPID);
	GET_SETTING(ALTERSTEPSIZE);
	GET_SETTING(DECAYPROPORTION);
    } else {
	printf("Sim Name not found, so using defaults\n");
	titletemp = "SIM PARAMETER LIST NOT FOUND";
	XCHIPS = XDIMENSIONS / EACHCHIPX;
	YCHIPS = YDIMENSIONS / EACHCHIPY;
    }

    // this section sets the variables based on the input (or defaults)

    strcpy(TITLE, titletemp);

    windowBorder = WINBORDER;
    windowHeight = WINHEIGHT;
    keyWidth = KEYWIDTH;
    if (DISPLAYKEY == 0) {
	keyWidth = 0;
    }
    windowWidth = WINWIDTH + keyWidth; // startup for window sizing
    displayWindow = TIMEWINDOW;

    plotWidth = windowWidth - 2 * windowBorder - keyWidth; // how wide is the actual plot area
    printlabels = (windowBorder >= 100); // only print labels if the border is big enough

    // set global variables based on input file data
    xdim = XDIMENSIONS;        // number of items to plot in the x dimension
    ydim = YDIMENSIONS;        // number of items to plot in the y dimension

    colourused = STARTCOLOUR;    // start with requested colour scheme
    displaymode = STARTMODE;    // initialise mode variable for the start

    if (SIMULATION == HEATMAP) {
	xorigin = (windowWidth + keyWidth) - controlboxes * (boxsize + gap); // for the control box
    }

    // malloc appropriate memory

    history_data = (float**) malloc(HISTORYSIZE * sizeof(float*));
    for (int ii = 0; ii < HISTORYSIZE ; ii++) {
	history_data[ii] = (float*) malloc(
		XDIMENSIONS * YDIMENSIONS * sizeof float);
    }
    history_data_set2 = (float**) malloc(HISTORYSIZE * sizeof(float*));
    for (int ii = 0; ii < HISTORYSIZE ; ii++) {
	history_data_set2[ii] = (float*) malloc(
		MAXRASTERISEDNEURONS * sizeof float);
    }

    const int len = XDIMENSIONS * YDIMENSIONS;
    immediate_data = (float*) malloc(len * sizeof float);

    maplocaltoglobal = (int**) malloc(len * sizeof(int*));
    for (int ii = 0; ii < len ; ii++) {
	maplocaltoglobal[ii] = (int*) malloc(2 * sizeof int);
    }
    mapglobaltolocal = (int**) malloc(len * sizeof(int*));
    for (int ii = 0; ii < len ; ii++) {
	mapglobaltolocal[ii] = (int*) malloc(2 * sizeof int);
    }

    // Per visualiser option
    if (SIMULATION == RATEPLOT || RATEPLOTLEGACY) {
	biascurrent = (float*) malloc(len * sizeof float);
    }

    config_destroy(&cfg);
}


void parse_arguments(
	int argc,
	char **argv,
	char *&configfn,
	char *&replayfn,
	char *&l2gfn,
	char *&g2lfn,
	float &replayspeed)
{
    // read and check the command line arguments
    int errfound = 0;
    int gotl2gfn = 0, gotg2lfn = 0;

    configfn = nullptr;
    replayfn = nullptr;
    l2gfn = nullptr;
    g2lfn = nullptr;
    replayspeed = 1.0;

    // go through all the arguments
    for (int i = 1; i < argc ; i++) {
	if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-config") == 0) {
	    if (i + 1 >= argc) {
		errfound++;
		printf("*No local config filename provided. Error.\n");
		break;
	    }
	    // TODO: check if filename begins with - and give error if this is the case?
	    configfn = argv[++i];
	    printf("Attempting to load configuration file: %s.\n", configfn);
	} else if (strcmp(argv[i], "-r") == 0
		|| strcmp(argv[i], "-replay") == 0) {
	    if (i + 1 >= argc) {
		errfound++;
		printf("** No replay filename provided. Error.\n");
		break;
	    }
	    replayfn = argv[++i];
	    printf("Attempting to load file for replay: %s.\n", replayfn);
	    if (i + 1 < argc && atof(argv[i + 1]) >= 0.1
		    && atof(argv[i + 1]) <= 100.0) {
		// if next argument is a number then this is the multiplier
		replayspeed = atof(argv[++i]);
		printf("** Replay multiplier: %f.\n", replayspeed);
	    } else {
		printf("** Note: no multiplier option supplied.\n");
	    }
	} else if (strcmp(argv[i], "-l2g") == 0) {
	    if (i + 1 >= argc) {
		errfound++;
		printf("*** No L to G filename provided. Error.\n");
		break;
	    }
	    gotl2gfn = 1;
	    l2gfn = argv[++i];
	    printf("Attempting to load Local to Global file: %s.\n", l2gfn);
	} else if (strcmp(argv[i], "-g2l") == 0) {
	    if (i + 1 >= argc) {
		errfound++;
		printf("**** No G to L filename provided. Error.\n");
		break;
	    }
	    gotg2lfn = 1;
	    g2lfn = argv[++i];
	    printf("Attempting to load Global to Local file: %s.\n", g2lfn);
	} else if (strcmp(argv[i], "-ip") == 0) {
	    // spinnakerboardip is set
	    if (i + 1 >= argc) { // check to see if a 2nd argument provided
		errfound++;
		printf(
			"***** You said you'd supply an IP "
			"address/hostname but didn't, I'm sad. Error.\n");
		break;
	    }
	    char *sourceipaddr = argv[++i]; // here's our hostname or ip address to check
	    hostent *validipfound = gethostbyname(sourceipaddr);
	    if (!validipfound) {
		errfound++;                  // if doesn't exist then fail
		printf("***** Can't figure out the IP "
			"address/hostname supplied, sorry. Error.\n");
		continue;
	    }
	    spinnakerboardip = *(in_addr *) validipfound->h_addr;
	    spinnakerboardipset++;
	    //spinnakerboardport=SDPPORT;
	    //init_sdp_sender();  // unsure of sending port # - so this is a guess...
	    printf("Waiting for packets only from: %s.\n",
		    inet_ntoa(spinnakerboardip));
	} else {
	    errfound++;
	    break;
	}
    }

    if (gotl2gfn == 1 && gotg2lfn == 0) {
	printf("L to G filename specified, but G to L is not. Error.\n");
	errfound = 1;
    }
    if (gotl2gfn == 0 && gotg2lfn == 1) {
	printf("G to L filename specified, but L to G is not. Error.\n");
	errfound = 1;
    }

    if (errfound > 0) {
	printf("\n Unsure of your command line options old chap.\n\n");
	fprintf(stderr,
		"usage: %s [-c configfile] [-r savedspinnfile [replaymultiplier(0.1->100)]] [-l2g localtoglobalmapfile] [-g2l globaltolocalmapfile] [-ip boardhostname|ipaddr]\n",
		argv[0]);
	exit(1);
    }
}
