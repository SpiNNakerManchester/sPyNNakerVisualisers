from .constants import \
    WINBORDER, WINHEIGHT, WINWIDTH, KEYWIDTH, HIWATER, LOWATER, NOTDEFINED

xdim = 0
ydim = 0
plotwidth = 0
windowBorder = WINBORDER
windowHeight = WINHEIGHT
windowWidth = WINWIDTH + KEYWIDTH
oldWindowBorder = 0
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

freezetime = 0
firstreceivetime = 0
starttime = 0

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
