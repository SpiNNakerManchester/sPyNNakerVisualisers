import json
import os.path
from spynnaker_visualisers.heat.constants import \
    WINBORDER, WINHEIGHT, WINWIDTH, KEYWIDTH, HIWATER, LOWATER, NOTDEFINED, \
    BOXSIZE, GAP, EACHCHIPX, EACHCHIPY, FIXEDPOINT, ALTERSTEPSIZE, SDPPORT, \
    HISTORYSIZE, XDIMENSIONS, YDIMENSIONS, MAXFRAMERATE, CONTROLBOXES

title = "NO SIMULATION TITLE SUPPLIED"

xdim, ydim = XDIMENSIONS, YDIMENSIONS
each_x, each_y = EACHCHIPX, EACHCHIPY
x_chips, y_chips = 0, 0 
plotwidth = 0
windowBorder = WINBORDER
windowHeight = WINHEIGHT
windowWidth = WINWIDTH + KEYWIDTH
oldWindowBorder = 0
xorigin = 0
yorigin = GAP
lowwatermark = HIWATER
highwatermark = LOWATER

plotvaluesinblocks = False
somethingtoplot = False
freezedisplay = False
safelyshutcalls = False
gridlines = False
fullscreen = False
xflip = False
yflip = False
vectorflip = False
rotateflip = False
printlabels = False
editmode = True

livebox = -1
alternorth = 40.0
altersouth = 10.0
altereast = 10.0
alterwest = 40.0
max_frame_rate = MAXFRAMERATE

fixed_point_factor = 0.5 ** FIXEDPOINT
alter_step = ALTERSTEPSIZE
our_port = SDPPORT

counter = 0
freezetime = 0
firstreceivetime = 0
starttime = 0
pktgone = 0

history_size = HISTORYSIZE
immediate_data = list()
history_data = list()


def param_load(filename):
    global title, xdim, ydim, each_x, each_y, x_chips, y_chips
    global max_frame_rate, fixed_point_factor, alter_step, printlabels
    global history_size, history_data, immediate_data, our_port
    global windowBorder, windowHeight, windowWidth, plotwidth, xorigin

    if not os.path.isfile(filename):
        filename = os.path.join(os.path.dirname(__file__), filename)
    with open(filename) as f:
        data = json.load(f)

    title = data.get("title", title)
    xdim, ydim = data.get("dimensions", [xdim, ydim])
    each_x, each_y = data.get("chip_size", [each_x, each_y])
    x_chips, y_chips = data.get("num_chips", [xdim / each_x, ydim / each_y])
    history_size = int(data.get("history_size", history_size))
    max_frame_rate = float(data.get("max_frame_rate", max_frame_rate))
    our_port = int(data.get("sdp_port", our_port))
    fixed_point_factor = 0.5 ** data.get("fixed_point_digits", FIXEDPOINT)
    alter_step = data.get("alter_step_size", alter_step)

    windowBorder = WINBORDER
    windowHeight = WINHEIGHT
    windowWidth = WINWIDTH + KEYWIDTH
    plotwidth = windowWidth - 2 * windowBorder - KEYWIDTH
    printlabels = (windowBorder >= 100)

    xorigin = windowWidth + KEYWIDTH - CONTROLBOXES * (BOXSIZE + GAP)

    n_elems = xdim * ydim
    history_data = [[0.0 for _ in xrange(n_elems)]
                    for _ in xrange(history_size)]
    immediate_data = [0.0 for _ in xrange(n_elems)]


def cleardown():
    global immediate_data, highwatermark, lowwatermark
    global xflip, yflip, vectorflip, rotateflip 
    for i in xrange(xdim * ydim):
        immediate_data[i] = NOTDEFINED
    highwatermark = HIWATER
    lowwatermark = LOWATER
    xflip = False
    yflip = False
    vectorflip = False
    rotateflip = False
