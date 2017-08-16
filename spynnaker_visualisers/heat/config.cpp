#include <libconfig.h>
#include "state.h"

template<typename INT>
static inline void get_setting(
	config_setting_t *setting,
	const char *name,
	INT &var)
{
    long long VALUE = 0;

    if (config_setting_lookup_int64(setting, name, &VALUE)) {
	var = INT(VALUE);
    }
}

static inline void get_setting(
	config_setting_t *setting,
	const char *name,
	float &var)
{
    double VALUE = 0;

    if (config_setting_lookup_float(setting, name, &VALUE)) {
	var = VALUE;
    }
}

static inline void get_setting(
	config_setting_t *setting,
	const char *name,
	double &var)
{
    double VALUE = 0;

    if (config_setting_lookup_float(setting, name, &VALUE)) {
	var = VALUE;
    }
}

#define GET_SETTING(var) get_setting(setting, #var, var)

static inline int streq(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}

// --------------------------------------------------------------------

template<typename T>
static bool parse_config_section(T setting, const char *&titletemp)
{
    if (setting == nullptr) {
	return true;
    }
    if (!config_setting_lookup_string(setting, "TITLE", &titletemp)) {
	titletemp = "NO SIMULATION TITLE SUPPLIED";
    }

    GET_SETTING(XDIMENSIONS);
    GET_SETTING(YDIMENSIONS);
    GET_SETTING(EACHCHIPX);
    GET_SETTING(EACHCHIPY);
    GET_SETTING(XCHIPS);
    GET_SETTING(YCHIPS);
    GET_SETTING(HISTORYSIZE);
    GET_SETTING(MAXFRAMERATE);
    GET_SETTING(SDPPORT);
    GET_SETTING(FIXEDPOINT);
    GET_SETTING(ALTERSTEPSIZE);
    return false;
}

static void parse_config(const char *config_file_name)
{
    config_t cfg; //Returns all parameters in this structure
    const char *paramblock;
    const char *titletemp;

    //Initialization
    config_init(&cfg);

    if (!config_read_file(&cfg, config_file_name)) {
	printf("No readable %s in the local directory; "
		"configuration defaulted to 48-chip HEATMAP.\n",
		config_file_name);
	goto use_defaults;
    }

    // Get the simulation parameters to use.
    if (config_lookup_string(&cfg, "simparams", &paramblock)) {
	printf("Sim params specified: %s\n", paramblock);
    } else {
	printf("No 'simparams' settings in configuration file.\n");
    }

    if (parse_config_section(config_lookup(&cfg, paramblock), titletemp)) {
	use_defaults:
	printf("Sim Name not found, so using defaults\n");
	titletemp = "SIM PARAMETER LIST NOT FOUND";
	XCHIPS = XDIMENSIONS / EACHCHIPX;
	YCHIPS = YDIMENSIONS / EACHCHIPY;
    }
    strcpy(TITLE, titletemp);

    config_destroy(&cfg);
}

static void paramload(const char *config_file_name)
{
    parse_config(config_file_name);

    // this section sets the derived variables

    windowBorder = WINBORDER;
    windowHeight = WINHEIGHT;
    windowWidth = WINWIDTH + keyWidth; // startup for window sizing

    plotWidth = windowWidth - 2 * windowBorder - keyWidth; // how wide is the actual plot area
    printlabels = (windowBorder >= 100); // only print labels if the border is big enough

    xdim = XDIMENSIONS;        // number of items to plot in the x dimension
    ydim = YDIMENSIONS;        // number of items to plot in the y dimension

    xorigin = (windowWidth + keyWidth) - controlboxes * (boxsize + gap); // for the control box

    FixedPointFactor = 1.0F / float(pow(2.0, FIXEDPOINT));

    // malloc appropriate memory

    const unsigned len = XDIMENSIONS * YDIMENSIONS;
    history_data = new float*[HISTORYSIZE];
    for (unsigned ii = 0 ; ii < HISTORYSIZE ; ii++) {
	history_data[ii] = new float[len];
    }
    // ====================================================
    history_data_set2 = new float*[HISTORYSIZE];
    for (unsigned ii = 0 ; ii < HISTORYSIZE ; ii++) {
	history_data_set2[ii] = new float[1024];
    }
    // ====================================================
    immediate_data = new float[len];
}

// free up mallocs made for dynamic arrays
static void finalise_memory(void) {
    for (unsigned i = 0 ; i < HISTORYSIZE ; i++) {
	delete[] history_data[i];
    }
    delete[] history_data;
    // ====================================================
    for (unsigned i = 0 ; i < HISTORYSIZE ; i++) {
	delete[] history_data_set2[i];
    }
    delete[] history_data_set2;
    // ====================================================
    delete[] immediate_data;
}

static void parse_arguments(int argc, char **argv, const char *&configfn)
{
    // read and check the command line arguments
    int errfound = 0;

    configfn = nullptr;

    // go through all the arguments
    for (int i = 1; i < argc ; i++) {
	if (streq(argv[i], "-c") || streq(argv[i], "-config")) {
	    if (i + 1 >= argc) {
		errfound++;
		fprintf(stderr, "***** No local config filename given.\n");
		break;
	    }
	    configfn = argv[++i];
	    printf("Attempting to load configuration file: %s.\n", configfn);
	} else if (streq(argv[i], "-ip")) {
	    // spinnakerboardip is set
	    if (i + 1 >= argc) { // check to see if a 2nd argument provided
		errfound++;
		fprintf(stderr, "***** IP address missing.\n");
		break;
	    }
	    char *sourceipaddr = argv[++i]; // here's our hostname or ip address to check
	    hostent *validipfound = gethostbyname(sourceipaddr);
	    if (!validipfound) {
		errfound++;                  // if doesn't exist then fail
		fprintf(stderr, "***** Can't parse IP address/hostname.\n");
		continue;
	    }
	    set_board_ip_address((in_addr *) validipfound->h_addr);
	    printf("Waiting for packets only from: %s.\n",
		    inet_ntoa(spinnakerboardip));
	} else {
	    errfound++;
	    break;
	}
    }

    if (errfound) {
	fprintf(stderr, "usage: %s [-c configfile] "
		"[-ip boardhostname|ipaddr]\n", argv[0]);
	exit(1);
    }
}
