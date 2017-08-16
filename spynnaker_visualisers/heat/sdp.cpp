#include "state.h"

static in_addr spinnakerboardip;
static char spinnakerboardipset = 0;
static int spinnakerboardport = 0;
static sockaddr_in board_address;

static void set_board_ip_address(in_addr *address)
{
    spinnakerboardip = *address;
    spinnakerboardipset = 1;
}

static bool is_board_address_set(void)
{
    return spinnakerboardipset != 0;
}

static bool is_board_port_set(void)
{
    // 0 is not a valid port, as it is the ANY port.
    return spinnakerboardport != 0;
}

// setup socket for SDP frame receiving on port SDPPORT defined about
// (usually 17894)
static int init_sdp_listening()
{
    struct addrinfo hints_input, *servinfo_input, *p_input;
    char portno_input[8];

    snprintf(portno_input, 7, "%d", SDPPORT);
    bzero(&hints_input, sizeof hints_input);
    hints_input.ai_family = AF_INET;		// SpiNNaker only supports IPv4
    hints_input.ai_socktype = SOCK_DGRAM;	// type UDP (socket datagram)
    hints_input.ai_flags = AI_PASSIVE;		// use my IP

    int rv_input = getaddrinfo(NULL, portno_input, &hints_input,
	    &servinfo_input);
    if (rv_input != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv_input));
	exit(1);
    }

    // loop through all the results and bind to the first we can
    for (p_input = servinfo_input; p_input != NULL ;
	    p_input = p_input->ai_next) {
	sockfd_input = socket(p_input->ai_family, p_input->ai_socktype,
		p_input->ai_protocol);
	if (sockfd_input == -1) {
	    perror("SDP SpiNNaker listener: socket");
	    continue;
	}

	if (bind(sockfd_input, p_input->ai_addr, p_input->ai_addrlen) == -1) {
	    close(sockfd_input);
	    perror("SDP SpiNNaker listener: bind");
	    continue;
	}

	break;
    }

    if (p_input == NULL) {
	fprintf(stderr, "SDP listener: failed to bind socket\n");
	exit(-1);
    }

    freeaddrinfo(servinfo_input);
    return sockfd_input;
}

static inline void process_heatmap_packet(
	int numAdditionalBytes,
	unsigned updateline)
{
    if (freezedisplay) {
	return;
    }
    // takes the chip ID and works out the chip X,Y coords
    unsigned char xsrc = scanptr->srce_addr / 256;
    unsigned char ysrc = scanptr->srce_addr % 256;

    // for all extra data (assuming regular array of 4 byte words)
    for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) {
	uint arrayindex = (EACHCHIPX * EACHCHIPY)
		* (xsrc * (XDIMENSIONS / EACHCHIPX) + ysrc) + i;
	if (arrayindex > XDIMENSIONS * YDIMENSIONS) {
	    printf("Array index out of bounds: %u. (x=%u, y=%u)\n",
		    arrayindex, xsrc, ysrc);        // CPDEBUG
	} else {
	    immediate_data[arrayindex] = scanptr->data[i] * FixedPointFactor;
	    if (immediate_data[arrayindex] > highwatermark)
		printf("new hwm [%d, %d, %d] = %f\n", xsrc, ysrc, i,
			immediate_data[arrayindex]);
	    if (updateline > HISTORYSIZE) {
		printf("Updateline is out of bounds: %d\n", updateline);
	    } else {
		history_data[updateline][arrayindex] =
			immediate_data[arrayindex]; // replace any data already here
	    }
	}
	somethingtoplot = 1; // indicate we will need to refresh the screen
	// recombine to single vector - if display paused don't update what's there
	// send to log file for plotting (overwriting what's already here)
    }
}

static inline void update_history_data(unsigned updateline)
{
    // this is stored so that rows that have not been updated between then
    // and now can be cleared out; NOTE THAT IT IS STATIC!
    static unsigned lasthistorylineupdated = 0;

    // work out how many lines have gone past without activity.
    int linestoclear = updateline - lasthistorylineupdated;
    // TODO: when window is reduced updateline reduces. this causes an
    // underflow construed as a wraparound.
    if (linestoclear < 0 && updateline + 500 > lasthistorylineupdated) {
	// to cover any underflow when resizing plotting window smaller
	// (wrapping difference will be < 500)
	linestoclear = 0;
    }
    if (linestoclear < 0) {
	// if has wrapped then work out the true value
	linestoclear = updateline + HISTORYSIZE - lasthistorylineupdated;
    }

    unsigned numberofdatapoints = xdim * ydim;
    for (int i = 0 ; i < linestoclear ; i++) {
	for (unsigned j = 0 ; j < numberofdatapoints ; j++) {
	    // nullify data in the quiet period
	    history_data[(1 + i + lasthistorylineupdated) % HISTORYSIZE][j] =
		    NOTDEFINEDFLOAT;
	}
    }
    lasthistorylineupdated = updateline;
}

static void* input_thread_SDP(void *ptr)
{
    use(ptr);
    sockaddr_in si_other;		// for incoming frames
    const unsigned char sdp_header_len = 26;

    while (true) {
	int numAdditionalBytes = 0;
	socklen_t addr_len_input = sizeof(si_other);
	int numbytes_input = recvfrom(sockfd_input, buffer_input,
		sizeof buffer_input, 0, (sockaddr *) &si_other,
		&addr_len_input);
	if (numbytes_input == -1) {
	    if (errno == EBADF) {
		// Socket was closed on our end; we're out of here!
		return nullptr;
	    }
	    perror("error recvfrom");
	    exit(-1); // will only get here if there's an error getting the input frame off the Ethernet
	}

	scanptr = (sdp_msg *) buffer_input; // pointer to our packet in the buffer from the Ethernet
	scanptrspinn = (spinnpacket *) buffer_input; // pointer to our packet in the buffer from the Ethernet
	numAdditionalBytes = numbytes_input - sdp_header_len; // used for SDP only

	if (scanptrspinn->cmd_rc == htonl(SPINN_HELLO)) {
	    // discarding any hello packet by dropping out without processing
	    continue;
	}

	// record the port number we are being spoken to upon, and open the SDP connection externally.
	if (!is_board_address_set()) {   // if no ip: set ip,port && init
	    // if we don't already know the SpiNNaker board IP then we learn that this is our board to listen to
	    spinnakerboardport = htons(si_other.sin_port);
	    set_board_ip_address(&si_other.sin_addr);
	    init_sdp_sender();
	    printf("Pkt Received from %s on port: %d\n",
		    inet_ntoa(si_other.sin_addr), htons(si_other.sin_port));
	} else if (!is_board_port_set()) {     // if no port: set port && init
	    // if we don't already know the SpiNNaker port, then we get this dynamically from an incoming message.
	    spinnakerboardport = htons(si_other.sin_port);
	    init_sdp_sender();
	    printf("Pkt Received from %s on port: %d\n",
		    inet_ntoa(si_other.sin_addr), htons(si_other.sin_port));
	}

	// ip && port are now set, so process this SpiNNaker packet

	int64_t nowtime = timestamp();    // get time now in us
	if (firstreceivetimez == 0) {
	    firstreceivetimez = nowtime; // if 1st packet then note it's arrival
	}

	if (!freezedisplay) {
	    update_history_data(nowtime);
	}

	// time in seconds per history index in use (or pixel displayed)
	float timeperindex = TIMEWINDOW / float(plotWidth);
	// which index is being updated (on the right hand side)
	unsigned updateline = ((nowtime - starttimez)
		/ int64_t(timeperindex * 1000000)) % HISTORYSIZE;
	process_heatmap_packet(numAdditionalBytes, updateline);
    }
}

static int init_sdp_sender()
{
    struct addrinfo hints_output, *servinfo, *p;
    char portno_input[8];

    snprintf(portno_input, 8, "%d", spinnakerboardport);
    bzero(&hints_output, sizeof hints_output);
    hints_output.ai_family = AF_INET;
    hints_output.ai_socktype = SOCK_DGRAM;

    auto rv = getaddrinfo(inet_ntoa(spinnakerboardip), portno_input,
	    &hints_output, &servinfo);
    if (rv != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	exit(1);
    }
    // loop through all the results and make a socket
    for (p = servinfo; p != NULL ; p = p->ai_next) {
	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	if (sockfd != -1) {
	    board_address = *(sockaddr_in*) p->ai_addr;
	    freeaddrinfo(servinfo);
	    return sockfd;
	}
	perror("talker: socket");
    }

    fprintf(stderr, "failed to bind socket\n");
    exit(1);
}

static void sdp_sender(
	unsigned short dest_add,
	unsigned char dest_port,
	unsigned int command,
	unsigned int arg1,
	unsigned int arg2,
	unsigned int arg3,
	unsigned char extrawords,
	...)
{
    va_list ExtraData;			// Initialise list of extra data
    va_start(ExtraData, extrawords);	// Populate it - it's just after the extra words argument

    struct sdp_msg output_packet;	// create the SDP message we are going to send;

    output_packet.ip_time_out = 0;	// n/a
    output_packet.pad = 0;		// n/a
    output_packet.flags = 7;		// defaults
    output_packet.tag = 255;		// not used CP Changed 1st November 2011.
    output_packet.dest_port = dest_port;// dest port supplied externally  Was: 0x21; // core = 1,  port = 1
    output_packet.srce_port = 0xFF;	// Ethernet
    output_packet.dest_addr = htons(dest_add); // supplied externally
    output_packet.srce_addr = 0;	// from outside world not a SpiNNaker chip
    output_packet.cmd_rc = (unsigned short) command; // from outside world (host ordered)
    output_packet.seq = 0;		// seq code nullified - per ST email 27th Oct 2011
    output_packet.arg1 = arg1;		// argument1
    output_packet.arg2 = arg2;		// argument2
    output_packet.arg3 = arg3;		// argument3

    for (short i = 0 ; i < extrawords ; i++) {
	output_packet.data[i] = va_arg(ExtraData, unsigned int);
    }
    va_end(ExtraData);			// de-initialize the list

    if (is_board_address_set()) {
	auto sdplength = 26 + 4 * extrawords;
	struct sdp_msg *output_packet_ptr = &output_packet;

	if (sendto(sockfd, output_packet_ptr, sdplength, 0,
		(sockaddr*) &board_address, sizeof board_address) == -1) {
	    perror("oh dear - we didn't send our data!\n");
	    exit(1);
	}
    }

    printpktgone = timestamp();		// initialise the time the message started being displayed
}
