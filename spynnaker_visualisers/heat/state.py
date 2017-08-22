from .constants import \
    WINBORDER, WINHEIGHT, WINWIDTH, KEYWIDTH, HIWATER, LOWATER, NOTDEFINED
from spynnaker_visualisers.heat.constants import GAP

title = "XXXXXXXXXXXXXXXXXXXXXXXXX"

xdim = 0
ydim = 0
plotwidth = 0
windowBorder = WINBORDER
windowHeight = WINHEIGHT
windowWidth = WINWIDTH + KEYWIDTH
oldWindowBorder = 0
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

editmode = False
livebox = -1
alternorth = 40.0
altersouth = 10.0
altereast = 10.0
alterwest = 40.0

counter = 0
freezetime = 0
firstreceivetime = 0
starttime = 0
pktgone = 0

immediate_data = list()
history_data = list()

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
