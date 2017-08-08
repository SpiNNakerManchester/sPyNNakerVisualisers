#include "state.h"

// setup socket for SDP frame receiving on port SDPPORT defined about (usually 17894)
void init_sdp_listening()
{
    snprintf(portno_input, 6, "%d", SDPPORT);

    bzero(&hints_input, sizeof(hints_input));
    hints_input.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints_input.ai_socktype = SOCK_DGRAM; // type UDP (socket datagram)
    hints_input.ai_flags = AI_PASSIVE; // use my IP

    rv_input = getaddrinfo(NULL, portno_input, &hints_input, &servinfo_input);
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

    //printf ("SDP UDP listener setup complete!\n");      // here ends the UDP listener setup witchcraft
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
	return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static inline void process_retina_packet(void)
{
    if (freezedisplay) { // if we are paused, do nothing
	return;
    }
    if (htonl(scanptrspinn->cmd_rc) != STIM_IN_SPINN_PACKET) { // only if we got the proper command
	return;
    }

    for (int i = 0 ; i < (numbytes_input - 18) / 4 ; i++) { // for all extra data (assuming regular array of 4 byte words)
	uint spikerID = scanptrspinn->data[i] & 0xFF; // Get the firing neuron ID (mask off last 8 bits for neuronID ignoring chip/coreID)
	immediate_data[spikerID] += 1; // Set the bit to say it's arrived
	if (spikerID < minneuridrx) {
	    minneuridrx = spikerID;
	}
	if (spikerID > maxneuridrx) {
	    maxneuridrx = spikerID;
	}
	history_data[updateline][spikerID] = immediate_data[spikerID]; // add to count in this interval
	if (outputfileformat == 2) { // write to output file only if required and in NeuroTools format (2)
	    if (writingtofile == 0) {
		writingtofile = 1; // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
		fprintf(fileoutput, "%lld.0\t%d.0\n",
			(long long int) sincefirstpacket,
			spikerID); // neurotools format (ms and NeurID)
		writingtofile = 0;      // note write finished
	    }
	}
    } //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
    somethingtoplot = 1; // indicate we should refresh the screen as we likely have new data
}

static inline void process_seville_retina_packet(int numAdditionalBytes)
{
    uint commandcode = scanptr->cmd_rc;
    uint columnnum = scanptr->arg1;
    uint numofrows = scanptr->arg2;
    uint numofcols = scanptr->arg3;

    if (freezedisplay | commandcode != 0x4943) {
	return;
    }
    // we are not paused, going to I.C. the seville retina
    for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) {	// for all extra data (assuming regular array of signed shorts)
	short datain1 = (scanptr->data[i]) & 0xFFFF;		// 1st of the pair
	short datain2 = (scanptr->data[i] >> 16) & 0xFFFF;	// 2nd of the pair
	uint pixelid = columnnum * numofrows + (i * 2);		// 1st pixel ID

	immediate_data[pixelid] = datain1;	// store 1st pixel ID
	history_data[updateline][pixelid] =
		immediate_data[pixelid];	// replace any data here already
	immediate_data[pixelid + 1] = datain2;	// store 2nd pixel ID
	history_data[updateline][pixelid + 1] =
		immediate_data[pixelid + 1];	// replace any data here already
    }
}

static inline void process_seville_retina_2_packet(int numAdditionalBytes)
{
    // FG for Seville Retina 19th Apr 2013

    if (freezedisplay) {
	return;
    }

    // so long as the display is still active then listen to new input
    for (uint e = 0 ; e < numAdditionalBytes / 4 ; e++) {
	uint bottom_rkey = scanptr->data[e] & 0xFFFF;
	uint neuron_id = ((bottom_rkey + ID_OFFSET) % 0x800
		+ ((bottom_rkey + ID_OFFSET) / 0x800 * N_PER_PROC))
		& (XDIMENSIONS * YDIMENSIONS - 1); // trying hack for non 2048 populations

	ushort x_coord_neuron = neuron_id % XDIMENSIONS; // X coordinate is lower 4 bits [0:3]
	ushort y_coord_neuron = neuron_id / YDIMENSIONS; // Y coordinate is bits [11:4] ??

	uint pixelid = neuron_id;                   // indexID

	immediate_data[pixelid] = x_coord_neuron; // store 1st pixel ID
	history_data[updateline][pixelid] = x_coord_neuron; // replace any data here already
	immediate_data[pixelid + 1] = y_coord_neuron; // store 2nd pixel ID
	history_data[updateline][pixelid + 1] = y_coord_neuron; // replace any data here already
    }
}

static inline void process_cochlea_packet(void)
{
    // QL for silicon cochlea 27th Aug 2013, CP incorporated 4th Sept 2013.

    if (freezedisplay) {
	return;
    }

    // so long as the display is still active then listen to new input
    short neuronID = scanptr->data[0] % 0x0800;
    short coreID = (scanptr->data[0] >> 11) % 0x20;
    short x_chip = (scanptr->data[0] >> 24);
    short y_chip = (scanptr->data[0] >> 16) % 0x0100;
    short NUM_Cell = 4;
    short NUM_Channel = 64;
    short x_coord = (coreID - 1) * NUM_Cell + neuronID % NUM_Cell;
    short y_coord = neuronID / NUM_Cell;

    uint pixelid = (x_coord * NUM_Channel) + y_coord;
    immediate_data[pixelid] += 1;
    history_data[updateline][pixelid] = immediate_data[pixelid];
}

static inline void process_legacy_rate_plot(int numAdditionalBytes)
{
    if (freezedisplay) {
	return;
    }

    uint chippopulationid = (scanptr->srce_port & 0x1F) - 1; // Francesco maps Virtual CPU ID to population 1:1 (at present)
    unsigned char xsrc = scanptr->srce_addr / 256; // takes the chip ID and works out the chip X coord
    unsigned char ysrc = scanptr->srce_addr % 256; // and the chip Y coord
    xcoord = (xsrc * EACHCHIPX) + (chippopulationid / EACHCHIPY); // each chip has 16 population values, 0=bottom left
    ycoord = ((ysrc * EACHCHIPY) + (chippopulationid % EACHCHIPY)); // 3 = top left, 12=BtmR, 15=TopR
    uint populationid = (EACHCHIPX * EACHCHIPY)
	    * ((ysrc * (XDIMENSIONS / EACHCHIPX)) + xsrc) + chippopulationid;
    uint commandcode = scanptr->cmd_rc;
    uint intervalinms = scanptr->arg2 + 1; // how often we are getting population firing counts for this population
    uint neuronsperpopulation = scanptr->arg3; // how many neurons are in this population ID

    if (commandcode == 257) { // populate rate data
	biascurrent[populationid] = (float) scanptr->arg1 / 256.0; // 8.8 fixed format data for the bias current used for this population
	for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) { // for all extra data (assuming regular array of 4 byte words)
	    uint spikesperinterval = scanptr->data[i]; // Spikes per interval for this population
	    immediate_data[populationid] = (spikesperinterval * 1000.0)
		    / (float) (intervalinms * neuronsperpopulation); // for this population stores average spike rate - in spikes per neuron/second
	    history_data[updateline][populationid] =
		    immediate_data[populationid]; // replace any data here already
	} //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)

	somethingtoplot = 1; // indicate we should refresh the screen as we likely have new data
	return;
    }

    if (commandcode == 256) { // populate raster spike data
	for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) { // for all extra data (assuming regular array of 4 byte words)
	    uint neuronID = scanptr->data[i] & 0xFF; // Which neuron has fired in this population (last 8 bits are significant)
	    if (neuronID < MAXRASTERISEDNEURONS) {
		if (neuronID < minneuridrx) {
		    minneuridrx = neuronID;
		}
		if (neuronID > maxneuridrx) {
		    maxneuridrx = neuronID;
		}
		history_data_set2[updateline][neuronID]++; // increment the spike count for this neuron at this time
		if (history_data_set2[updateline][neuronID] == 0) {
		    history_data_set2[updateline][neuronID]++; // increment the spike count for this neuron at this time TODO - this is a comparison of a float and an integer
		}
		if (outputfileformat == 2) { // write to output file only if required and in NeuroTools format (2)
		    if (writingtofile == 0) {
			writingtofile = 1; // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
			fprintf(fileoutput, "%lld.0\t%d.0\n",
				(long long int) sincefirstpacket, neuronID); // neurotools format (ms and NeurID)
			writingtofile = 0;  // note write finished
		    }
		}
	    }
	} //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)

	somethingtoplot = 1; // indicate we should refresh the screen as we likely have new data
	return;
    }
}

static inline void process_ma12_raster_packet(int numAdditionalBytes)
{
    uint commandcode = scanptr->cmd_rc;
    if (freezedisplay || commandcode != 80) {
	return;
    }

    for (int i = 0 ; i < numAdditionalBytes / 4 ; i += 2) { // for all extra data (assuming regular array of paired words, word1=key, word2=data)
	unsigned char xsrc = scanptr->data[i] >> 24;
	unsigned char ysrc = (scanptr->data[i] >> 16) & 0xFF;
	uint chippopulationid = (scanptr->data[i] >> 11) & 0xF; // Francesco maps Virtual CPU ID to population 1:1 (at present)

	printf("CoreID: %d. CorePopID: ", chippopulationid);
	if (BITSOFPOPID > 0) {
	    chippopulationid = chippopulationid << BITSOFPOPID; // add space for any per core population IDs (proto pops per core)
	    chippopulationid += (scanptr->data[i] >> 4) & 0x3; // add in proto popid
	}
	uint populationid = (EACHCHIPX * EACHCHIPY) * ((xsrc * YCHIPS) + ysrc)
		+ chippopulationid;

	immediate_data[populationid] = (float) scanptr->data[i + 1]; // for this population stores average spike rate - in spikes per neuron/second
	history_data[updateline][populationid] = immediate_data[populationid]; // replace any data here already
    }
}

static inline void process_spike_receive_packet(int numAdditionalBytes)
{
    uint commandcode = scanptr->cmd_rc;
    for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) { // for all extra data (assuming regular array of paired words, word1=key, word2=data)
	uint32_t data = scanptr->data[i];
	unsigned char xsrc = data >> 24;      // chip x coordinate
	unsigned char ysrc = (data >> 16) & 0xFF; // chip y coordinate
	uint chipcore = (data >> 11) & 0xF; // core of chip (note: 4 bits)
	uint neurid = data & 0x8FF; // neuron ID within this core

	printf("CoreID:%d, neurid:%d", chipcore, neurid); // do some printing for debug
	// note the neurid in this example is the only relevant index - there's no relevance of chip ID or core
	// if this is relevant then make the array indexes dependant on this data
	immediate_data[neurid] = 1; // make the data valid to say (at least) one spike received in the immediate data
	history_data[updateline][neurid] = immediate_data[neurid]; // make the data valid to say (at least) one spike received in this historical index data
    }
}

static inline void process_rate_plot_packet(int numAdditionalBytes)
{
    if (freezedisplay) {
	return;
    }

    uint commandcode = scanptr->cmd_rc;
    if (commandcode == 64 || commandcode == 65 || commandcode == 66) { // if we are not paused, going to populate rate data
	for (int i = 0 ; i < numAdditionalBytes / 4 ; i += 2) { // for all extra data (assuming regular array of paired words, word1=key, word2=data)
	    // read header info, x,y,core,pop.
	    unsigned char xsrc = scanptr->data[i] >> 24;
	    unsigned char ysrc = (scanptr->data[i] >> 16) & 0xFF;
	    uint chippopulationid = (scanptr->data[i] >> 11) & 0xF; // Francesco maps Virtual CPU ID to population 1:1 (at present)

	    if (BITSOFPOPID > 0) {
		chippopulationid = chippopulationid << BITSOFPOPID; // add space for any per core population IDs (proto pops per core)
		chippopulationid += (scanptr->data[i] >> 4) & 0x3; // add in proto popid
	    }
	    uint populationid = (EACHCHIPX * EACHCHIPY)
		    * ((xsrc * YCHIPS) + ysrc) + chippopulationid;

	    if (populationid > (YDIMENSIONS * XDIMENSIONS)) {
		commandcode = 11; // ignore anything that will go offscreen
	    }

	    if (commandcode == 64) {
		immediate_data[populationid] = (float) scanptr->data[i + 1]; // for this population stores average spike rate - in spikes per neuron/second
		history_data[updateline][populationid] =
			immediate_data[populationid]; // replace any data here already
	    } else if (commandcode == 65) {
		biascurrent[populationid] = (float) scanptr->data[i + 1]
			/ 256.0;
		// 8.8 fixed format data for the bias current used for this population
	    } else if (commandcode == 66) { // means we are plotting voltage
		float tempstore = (short) scanptr->data[i + 1];
		tempstore /= 256.0;
		immediate_data[0] = tempstore; // only 1 value to plot - the potential!
		history_data[updateline][0] = immediate_data[0]; // replace any data here already
	    }
	} //recombine to single vector - if display paused don't update what's there, sends to log file for plotting (overwriting what's already here)
	somethingtoplot = 1; // indicate we should refresh the screen as we likely have new data
    }
}

static inline void process_heatmap_packet(int numAdditionalBytes)
{
    // takes the chip ID and works out the chip X,Y coords
    unsigned char xsrc = scanptr->srce_addr / 256;
    unsigned char ysrc = scanptr->srce_addr % 256;

    // for all extra data (assuming regular array of 4 byte words)
    for (int i = 0 ; i < numAdditionalBytes / 4 ; i++) {
	uint arrayindex = (EACHCHIPX * EACHCHIPY)
		* ((xsrc * (XDIMENSIONS / EACHCHIPX)) + ysrc) + i;
	if (freezedisplay == 0) {
	    if (arrayindex < 0 || arrayindex > XDIMENSIONS * YDIMENSIONS) {
		printf(
			"Error line 772: Array index out of bounds: %u. (x=%u, y=%u)\n",
			arrayindex, xsrc, ysrc);        // CPDEBUG
	    } else {
		immediate_data[arrayindex] = (float) scanptr->data[i]
			/ (float) pow(2.0, FIXEDPOINT);
		if (immediate_data[arrayindex] > highwatermark)
		    printf("new hwm [%d, %d, %d] = %f\n", xsrc, ysrc, i,
			    immediate_data[arrayindex]);
		if (updateline < 0 || updateline > HISTORYSIZE) {
		    printf(
			    "Error line 776: Updateline is out of bounds: %d.\n",
			    updateline);        // CPDEBUG
		} else {
		    history_data[updateline][arrayindex] =
			    immediate_data[arrayindex]; // replace any data already here
		}
	    }
	    somethingtoplot = 1; // indicate we will need to refresh the screen

	}
	// recombine to single vector - if display paused don't update what's there
	// send to log file for plotting (overwriting what's already here)
    }
}

static inline void process_link_check_packet(void)
{
    // takes the chip ID and works out the chip X,Y coords
    unsigned char xsrc = scanptr->srce_addr / 256;
    unsigned char ysrc = scanptr->srce_addr % 256;

    int indexer = (ysrc * (EACHCHIPX * EACHCHIPY)); // give the y offset
    indexer += (xsrc * (EACHCHIPX * EACHCHIPY) * (YDIMENSIONS / EACHCHIPY));

    for (int i = 0 ; i < (EACHCHIPX * EACHCHIPY) ; i++) {
	immediate_data[indexer + i] = 100;
	if (i == 5) {
	    immediate_data[indexer + i] = 20;   // set centre blue
	}
	if (i == 2 || i == 8 || i == 3 || i == 7 || i > 10) {
	    immediate_data[indexer + i] = 0; // set top left and btm right black
	}
    }

    if (xsrc > 7) {
	printf("X out of bounds. Src: 0x%x, %d\n", scanptr->srce_addr, xsrc);
    }
    if (ysrc > 7) {
	printf("Y out of bounds. Src: 0x%x, %d\n", scanptr->srce_addr, ysrc);
    }
    if (!freezedisplay) {
	for (int i = 0 ; i < 6 ; i++) {
	    //int arrayindex=(EACHCHIPX*EACHCHIPY)*((xsrc*(XDIMENSIONS/EACHCHIPX))+ysrc);    // base index
	    if (scanptr->arg1 & (0x1 << i)) { // if array entry is set (have received on this port)
		int arrayindex = indexer;
		if (i == 0)
		    arrayindex += 1;         // RX from west
		//if (i==1) arrayindex += 1; // RX from sw (zero position)
		if (i == 2)
		    arrayindex += 4;         // RX from south
		if (i == 3)
		    arrayindex += 9;         // RX from east
		if (i == 4)
		    arrayindex += 10;         // RX from ne
		if (i == 5)
		    arrayindex += 6;         // RX from north
		immediate_data[arrayindex] = 60; // update immediate data
		history_data[updateline][arrayindex] =
			immediate_data[arrayindex]; // update historical data
		somethingtoplot = 1; // indicate we will need to refresh the screen
	    }
	}
    }
}

static inline void process_cpu_util_packet(int numAdditionalBytes)
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
		* ((xsrc * (XDIMENSIONS / EACHCHIPX)) + ysrc) + i;
	immediate_data[arrayindex] = (float) scanptr->data[i]; // utilisation data
	history_data[updateline][arrayindex] = immediate_data[arrayindex]; // replace any data already here
	somethingtoplot = 1; // indicate we will need to refresh the screen
	// recombine to single vector - if display paused don't update what's there
	// send to log file for plotting (overwriting what's already here)
    }
}

static inline void process_chip_temperature_packet(void)
{
    if (freezedisplay) {
	return;
    }
    // takes the chip ID and works out the chip X,Y coords
    unsigned char xsrc = scanptr->srce_addr / 256;
    unsigned char ysrc = scanptr->srce_addr % 256;
    uint arrayindex = (EACHCHIPX * EACHCHIPY)
	    * ((xsrc * (XDIMENSIONS / EACHCHIPX)) + ysrc); // no per core element so no +i
    // immediate_data[arrayindex]=(float)scanptr->cmd_rc/(float)pow(2.0,FIXEDPOINT);     // temperature data using algorithm in report.c (not great)
    // immediate_data[arrayindex]=((float)scanptr->arg1-6300) /15.0;     // temperature 1 sensor data
    // immediate_data[arrayindex]=((float)scanptr->arg2-9300) / 18.0;     // temperature 2 sensor data
    // immediate_data[arrayindex]=(55000-(float)scanptr->arg3)/450.0;     // temperature 3 sensor data
    immediate_data[arrayindex] = ((((float) scanptr->arg1 - 6300) / 15.0)
	    + (((float) scanptr->arg2 - 9300) / 18.0)
	    + ((55000 - (float) scanptr->arg3) / 450.0) - 80.0) / 1.5;
    // scale to something approximating 0->100  for the extremities spotted (so far - may need to tinker!)
    history_data[updateline][arrayindex] = immediate_data[arrayindex]; // replace any data already here
    somethingtoplot = 1; // indicate we will need to refresh the screen

    // recombine to single vector - if display paused don't update what's there
    // send to log file for plotting (overwriting what's already here)
}

static inline void process_integrator_fg_packet(int numAdditionalBytes)
{
    if (freezedisplay) {
	return;
    }
    xsrc = scanptr->srce_addr / 256; // takes the chip ID and works out the chip X coord
    ysrc = scanptr->srce_addr % 256; // and the chip Y coord
    uint arrayindex = (EACHCHIPX * EACHCHIPY)
	    * ((xsrc * (XDIMENSIONS / EACHCHIPX)) + ysrc); // no per core element so no +i
    if (numAdditionalBytes >= 4) {
	float input = 1.0 + (float) ((int) scanptr->data[0]) / 256.0;
	input *= 0.001 / 0.03;

	int previousindex = updateline - 1;
	if (previousindex < 0) {
	    previousindex = HISTORYSIZE;
	}

	float pastdata = 0.0;
	if (history_data[previousindex][arrayindex] > NOTDEFINEDFLOAT + 1) {
	    pastdata = history_data[previousindex][arrayindex];
	}
	input += pastdata * exp(-0.001 / 0.03);

	immediate_data[arrayindex] = input; // wobbler data
	history_data[updateline][arrayindex] = immediate_data[arrayindex]; // replace any data already here
	somethingtoplot = 1; // indicate we will need to refresh the screen
    }
    // recombine to single vector - if display paused don't update what's there
    // send to log file for plotting (overwriting what's already here)
}

void* input_thread_SDP(void *ptr)
{
    struct sockaddr_in si_other; // for incoming frames
    socklen_t addr_len_input = sizeof(struct sockaddr_in);
    int64_t sincefirstpacket, nowtime;
    char sdp_header_len = 26;
    unsigned char xsrc, ysrc;
    unsigned int i, xcoord, ycoord;
    struct timeval stopwatchus;

    while (1) {                             // for ever ever, ever ever.
	int numAdditionalBytes = 0;

	numbytes_input = recvfrom(sockfd_input, buffer_input,
		sizeof buffer_input, 0, (sockaddr*) &si_other,
		(socklen_t*) &addr_len_input);
	if (numbytes_input == -1) {
	    printf("Error line 441: : %s\n", strerror(errno));
	    perror((char*) "error recvfrom");
	    exit(-1); // will only get here if there's an error getting the input frame off the Ethernet
	}

	scanptr = (sdp_msg*) buffer_input; // pointer to our packet in the buffer from the Ethernet
	scanptrspinn = (spinnpacket*) buffer_input; // pointer to our packet in the buffer from the Ethernet
	numAdditionalBytes = numbytes_input - sdp_header_len; // used for SDP only

	if (scanptrspinn->cmd_rc != htonl(SPINN_HELLO)) { // we process only spinnaker packets that are non-hellos
	    if (spinnakerboardipset == 0) {   // if no ip: set ip,port && init
		// if we don't already know the SpiNNaker board IP then we learn that this is our board to listen to
		spinnakerboardip = si_other.sin_addr;
		spinnakerboardport = htons(si_other.sin_port);
		spinnakerboardipset++;
		init_sdp_sender();
		printf("Pkt Received from %s on port: %d\n",
			inet_ntoa(si_other.sin_addr),
			htons(si_other.sin_port));
	    }        // record the IP address of our SpiNNaker board.

	    if (spinnakerboardport == 0) {     // if no port: set port && init
		// if we don't already know the SpiNNaker port, then we get this dynamically from an incoming message.
		spinnakerboardport = htons(si_other.sin_port);
		init_sdp_sender();
		printf("Pkt Received from %s on port: %d\n",
			inet_ntoa(si_other.sin_addr),
			htons(si_other.sin_port));
	    } // record the port number we are being spoken to upon, and open the SDP connection externally.

	    // ip && port are now set, so process this SpiNNaker packet

	    gettimeofday(&stopwatchus, NULL);             // grab current time
	    nowtime = (((int64_t) stopwatchus.tv_sec * (int64_t) 1000000)
		    + (int64_t) stopwatchus.tv_usec);    // get time now in us
	    if (firstreceivetimez == 0)
		firstreceivetimez = nowtime; // if 1st packet then note it's arrival
	    sincefirstpacket = (nowtime - firstreceivetimez) / 1000; // how long in ms since visualisation got 1st valid packet.

	    float timeperindex = displayWindow / (float) plotWidth; // time in seconds per history index in use (or pixel displayed)
	    int updateline = ((nowtime - starttimez)
		    / (int64_t)(timeperindex * 1000000)) % (HISTORYSIZE); // which index is being updated (on the right hand side)

	    if (updateline < 0 || updateline > HISTORYSIZE) {
		printf("Error line 500: Updateline out of bounds: %d. "
			"Time per Index: %f.\n"
			"  Times - Now:%lld  Start:%lld\n", updateline,
			timeperindex, (long long int) nowtime,
			(long long int) starttimez); // CPDEBUG
	    } else if (!freezedisplay) {
		int linestoclear = updateline - lasthistorylineupdated; // work out how many lines have gone past without activity.
		// when window is reduced updateline reduces. ths causes an underflow construed as a wraparound. TODO.
		if (linestoclear < 0
			&& (updateline + 500) > lasthistorylineupdated) {
		    // to cover any underflow when resizing plotting window smaller (wrapping difference will be <500)
		    linestoclear = 0;
		}
		if (linestoclear < 0) {
		    // if has wrapped then work out the true value
		    linestoclear = (updateline + HISTORYSIZE)
			    - lasthistorylineupdated;
		}
		int numberofdatapoints = xdim * ydim;
		for (int i = 0 ; i < linestoclear ; i++) {
		    for (int j = 0 ; j < numberofdatapoints ; j++) {
			// nullify data in the quiet period
			history_data[(1 + i + lasthistorylineupdated)
				% (HISTORYSIZE)][j] =
				INITZERO ? 0.0 : NOTDEFINEDFLOAT;
		    }
		    if (win2) {
			numberofdatapoints = MAXRASTERISEDNEURONS; // bespoke for Discovery demo
			for (int j = 0 ; j < numberofdatapoints ; j++) {
			    // nullify data in the quiet period
			    history_data_set2[(1 + i + lasthistorylineupdated)
				    % (HISTORYSIZE)][j] =
				    INITZERO ? 0.0 : NOTDEFINEDFLOAT;
			}
		    }
		}
		lasthistorylineupdated = updateline;
	    }

	    if (SIMULATION == RETINA) {
		process_retina_packet()
	    } else if (SIMULATION == SEVILLERETINA) {
		process_seville_retina_packet(numAdditionalBytes);
	    } else if (SIMULATION == RETINA2) {
		process_seville_retina_2_packet(numAdditionalBytes);
	    } else if (SIMULATION == COCHLEA) {
		process_cochlea_packet();
	    } else if (SIMULATION == RATEPLOTLEGACY) {
		process_legacy_rate_plot(numAdditionalBytes);
	    } else if (SIMULATION == MAR12RASTER) {
		process_ma12_raster_packet(numAdditionalBytes);
	    } else if (SIMULATION == SPIKERVC) {
		process_spike_receive_packet(numAdditionalBytes);
	    } else if (SIMULATION == RATEPLOT) {
		process_rate_plot_packet(numAdditionalBytes);
	    } else if (SIMULATION == HEATMAP) {
		process_heatmap_packet(numAdditionalBytes);
	    } else if (SIMULATION == LINKCHECK) {
		process_link_check_packet();
	    } else if (SIMULATION == CPUUTIL) {
		process_cpu_util_packet(numAdditionalBytes);
	    } else if (SIMULATION == CHIPTEMP) {
		process_chip_temperature_packet();
	    } else if (SIMULATION == INTEGRATORFG) {
		process_integrator_fg_packet(numAdditionalBytes);
	    }

	    if (outputfileformat == 1) { // write to output file only if required and in normal SPINNAKER packet format (1) - basically the UDP payload
		short test_length = numbytes_input;
		int64_t test_timeoffset = nowtime - firstreceivetimez;

		if (writingtofile == 0) { // can only write to the file if its not paused and can write
		    writingtofile = 1; // 3 states.  1=busy writing, 2=paused, 0=not paused, not busy can write.
		    fwrite(&test_length, sizeof(test_length), 1, fileoutput);
		    fwrite(&test_timeoffset, sizeof(test_timeoffset), 1,
			    fileoutput);
		    fwrite(&buffer_input, test_length, 1, fileoutput);
		    writingtofile = 0;        // note write finished
		}
	    }

	    if (outputfileformat != 0) {
		fflush(fileoutput); // will have written something in this section to the output file - flush to the file now
	    }
	    fflush(stdout); // flush IO buffers now - and why not? (immortal B. Norman esq)

	}    // discarding any hello packet by dropping out without processing
    }
}

void init_sdp_sender()
{
    char portno_input[7];
    snprintf(portno_input, 6, "%d", spinnakerboardport);

    int rv, numbytes, block_data_len, block_id, i, j, length, remaining_bytes;

    bzero(&hints_output, sizeof hints_output);
    hints_output.ai_family = AF_UNSPEC;		// set to AF_INET to force IPv4
    hints_output.ai_socktype = SOCK_DGRAM;	// type UDP (socket datagram)

    rv = getaddrinfo(inet_ntoa(spinnakerboardip), portno_input, &hints_output,
	    &servinfo);
    if (rv != 0) {
	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	exit(1);
    }
    // loop through all the results and make a socket
    for (p = servinfo; p != NULL ; p = p->ai_next) {
	sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
	if (sockfd == -1) {
	    perror("talker: socket");
	    continue;
	}
	break;
    }

    if (p == NULL) {
	fprintf(stderr, "failed to bind socket\n");
	exit(1);
    }

    // freeaddrinfo(servinfo);  // at the end only
}

void sdp_sender(
	unsigned short dest_add,
	unsigned char dest_port,
	unsigned int command,
	unsigned int arg1,
	unsigned int arg2,
	unsigned int arg3,
	unsigned char extrawords,
	...)
{
    va_list ExtraData;            // Initialise list of extra data
    va_start(ExtraData, extrawords); // Populate it - it's just after the extra words argument

    unsigned int numbytes, sdplength;
    struct sdp_msg output_packet; // create the SDP message we are going to send;
    struct sdp_msg *output_packet_ptr;     // create a pointer to it too
    output_packet_ptr = &output_packet;

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
	output_packet.data[i]=va_arg(ExtraData,unsigned int);
    }
    va_end(ExtraData);			// de-initialize the list

    sdplength = 26 + 4 * extrawords;	// only send extra data if it's supplied

    if (spinnakerboardipset != 0) { // if we don't know where to send don't send!
	numbytes = sendto(sockfd, output_packet_ptr, sdplength, 0,
		p->ai_addr, p->ai_addrlen);
	if (numbytes == -1) {
	    perror("oh dear - we didn't send our data!\n");
	    exit(1);
	}
    }

    int64_t nowtime;
    struct timeval stopwatchus;

    gettimeofday(&stopwatchus, NULL);	// grab current time
    nowtime = (((int64_t) stopwatchus.tv_sec * (int64_t) 1000000)
	    + (int64_t) stopwatchus.tv_usec);    // get time now in us
    printpktgone = nowtime;		// initialise the time the message started being displayed
    //printf("Pkt sent:\n");
}
