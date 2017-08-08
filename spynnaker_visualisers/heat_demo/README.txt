Hello,

This is the real-time visualiser.  This documentation will expand over time, but in the meantime here's some snippets...


Compilation
===========
Compile with the command at the top of the visualiser.cpp file (you'll need some development libraries as specified).


Operation
=========
Operate via the command line depending on the simulation you've loaded to the system.
usage: ./visualiser [-c configfile] [-r savedspinnfile [replaymultiplier(0.1->100)]] [-l2g localtoglobalmapfile] [-g2l globaltolocalmapfile]

* Specify the configuration file, which sets how the visualiser operates (on which simulation), using the -c option.
* You can also optionally load and replay (-r)  a saved set of data, and chose its playback speed as a multiple of its original timescale.
* It's also possible to specify the local to global and visa-versa mappings. You'll only need this if PACMAN has split your
neuron populations over multiple cores, and these maps help put them back together for visualisation, and for sending
interactive messages back into the machine from the visualiser.

If you do not specify any configuration file (or if the file you specify is not found) then the visualiser defaults to the HEATMAP48 settings.
If you do not specify a replay file, then your data will come in live over the network.


Adding New Simulations
======================
To be completed.
In the meantime feel free to take a gander at the source code and try and work it out.  
Contact me (Cameron Patterson - pattersc@cs.man.ac.uk) if you'd like more info.
You can change parameters on how you look at the data within the .ini files.


Examples/Regression tests
=========================
There are a good few tests that you can run with the system.   
From the directory where you have compiled the visualiser take a looksie at:

  ./visualiser -c heatmap48.ini    -r regressiontests/HEATMAP/HEATMAP48/replay48.spinn 1.0
  ./visualiser -c heatmap3x48.ini  -r regressiontests/HEATMAP/HEATMAP48/replay48.spinn 1.2
  ./visualiser -c heatmap4.ini     -r regressiontests/HEATMAP/HEATMAP4/replay4.spinn 0.5

  ./visualiser -c cpuutil48.ini    -r regressiontests/CPUUTIL/replay48.spinn 1.2
  ./visualiser -c cpuutil4.ini     -r regressiontests/CPUUTIL/replay4.spinn 1.0

  ./visualiser -c linkcheck48.ini  -r regressiontests/LINKCHECK/replay48.spinn
  ./visualiser -c linkcheck4.ini   -r regressiontests/LINKCHECK/replay4.spinn

  ./visualiser -c chiptemp48.ini   -r regressiontests/CHIPTEMP/replay48.spinn
  ./visualiser -c chiptemp4.ini    -r regressiontests/CHIPTEMP/replay4.spinn 6
  
  ./visualiser -c integratorfg.ini -r regressiontests/INTEGRATIONFG/replay-oscillator.spinn 1.152


There are currrently no other regression tests, nor is there a regression test for the Global to Local mapping.







