from OpenGL.GL import *  # @UnusedWildImport
from OpenGL.GLUT import *  # @UnusedWildImport

from spynnaker_visualisers.heat.constants \
    import UIColours, EACHCHIPX, EACHCHIPY, YDIMENSIONS, YCHIPS, XDIMENSIONS,\
    MINDATA, MAXDATA, BOXSIZE, GAP, KEYWIDTH, CONTROLBOXES, Direction
import spynnaker_visualisers.heat.state as state
from spynnaker_visualisers.heat.sdp import is_board_address_set
from spynnaker_visualisers.heat.utils import clamp, is_defined


# ----------------------------------------------------------------------------


def printgl(x, y, style, fmt, *args):
    if len(args):
        fmt = fmt % tuple(args)
    glRasterPos(x, y)
    glutBitmapString(style, fmt)


def printglstroke(x, y, size, rotate, fmt, *args):
    style = GLUT_STROKE_ROMAN
    if len(args):
        fmt = fmt % tuple(args)
    glPushMatrix()
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    glEnable(GL_SMOOTH)
    glLineWidth(1.5)
    glTranslate(x, y, 0)
    glScale(size, size, size)
    glRotate(rotate, 0, 0, 1)
    glutStrokeString(style, fmt)
    glDisable(GL_LINE_SMOOTH)
    glDisable(GL_BLEND)
    glPopMatrix()


def color(colour_id):
    if colour_id == UIColours.BLACK:
        glColor(0, 0, 0)
    elif colour_id == UIColours.WHITE:
        glColor(1, 1, 1)
    elif colour_id == UIColours.RED:
        glColor(1, 0, 0)
    elif colour_id == UIColours.GREEN:
        glColor(0, 0.6, 0)
    elif colour_id == UIColours.CYAN:
        glColor(0, 1, 1)
    elif colour_id == UIColours.GREY:
        glColor(0.8, 0.8, 0.8)


def clear(colour_id):
    if colour_id == UIColours.BLACK:
        glClearColor(0, 0, 0, 1)
    elif colour_id == UIColours.WHITE:
        glClearColor(1, 1, 1, 1)
    elif colour_id == UIColours.RED:
        glClearColor(1, 0, 0, 1)
    elif colour_id == UIColours.GREEN:
        glClearColor(0, 0.6, 0, 1)
    elif colour_id == UIColours.CYAN:
        glClearColor(0, 1, 1, 1)
    elif colour_id == UIColours.GREY:
        glClearColor(0.8, 0.8, 0.8, 1)


def rectVertices(x1, y1, x2, y2):
    """Generate vertices for a filled box"""
    glVertex(x1, y1)
    glVertex(x1, y2)
    glVertex(x2, y2)
    glVertex(x2, y1)


def openBoxVertices(x1, y1, x2, y2):
    """Generate vertices for an open box"""
    glVertex(x1, y1)
    glVertex(x1, y2)
    glVertex(x2, y2)
    glVertex(x2, y1)


# ----------------------------------------------------------------------------


def convert_index_to_coord(index):
    tileid, elementid = divmod(index, EACHCHIPX * EACHCHIPY)
    elementx, elementy = divmod(elementid, EACHCHIPY)
    tilex, tiley = divmod(tileid, YDIMENSIONS / EACHCHIPY)
    return tilex * EACHCHIPX + elementx, tiley * EACHCHIPY + elementy


def convert_coord_to_index(x, y):
    tilex, elementx = divmod(x, EACHCHIPX)
    tiley, elementy = divmod(y, EACHCHIPY)
    elementid = elementx * EACHCHIPY + elementy
    return (EACHCHIPX * EACHCHIPY) * (tilex * YCHIPS + tiley) + elementid


def coordinate_manipulate(i):
    if state.xflip or state.yflip or state.vectorflip or state.rotateflip:
        chips_x = XDIMENSIONS / EACHCHIPX
        chips_y = YDIMENSIONS / EACHCHIPY
        tileid, elementid = divmod(i, EACHCHIPX * EACHCHIPY)
        elementx, elementy = divmod(elementid, EACHCHIPY)
        tilex, tiley = divmod(tileid, chips_y)

        # Flip ycoords
        if state.yflip:
            elementy = EACHCHIPY - 1 - elementy
            tiley = chips_y - 1 - tiley
        # Flip xcoords
        if state.xflip:
            elementx = EACHCHIPX - 1 - elementx
            tilex = chips_x - 1 - tilex

        elementid = elementx * EACHCHIPY + elementy
        i = (EACHCHIPX * EACHCHIPY) * (tilex * chips_x + tiley) + elementid

        # Go back to front (cumulative)
        if state.vectorflip:
            i = YDIMENSIONS * XDIMENSIONS - 1 - i
        # Rotate
        if state.rotateflip:
            xcoord, ycoord = convert_index_to_coord(i)
            i = convert_coord_to_index(ycoord, XDIMENSIONS - 1 - xcoord)
    return i


def _interpolate(gamut, idx, fillcolour):
    size = len(gamut) - 1
    val = clamp(0.0, fillcolour, 1.0)
    index = int(val * size)
    offset = (index + 1) - val * size
    return (1 - offset) * gamut[index+1][idx] + offset * gamut[index][idx]


GAMUT = [(0,0,0), (0,0,1), (0,1,1), (0,1,0), (1,1,0), (1,0,0)]


def colour_calculator(val, hi, lo):
    diff = float(hi - lo)
    if diff < 0.0001:
        fillcolour = 1.0
    else:
        fillcolour = (clamp(lo, val, hi) - lo) / diff
    r = _interpolate(GAMUT, 0, fillcolour)
    g = _interpolate(GAMUT, 1, fillcolour)
    b = _interpolate(GAMUT, 2, fillcolour)
    glColor(r, g, b)
    return fillcolour


def display_titles_labels():
    printgl(state.windowWidth / 2 - 200, state.windowHeight - 50,
            GLUT_BITMAP_TIMES_ROMAN_24, state.title)
    printgl(state.windowWidth / 2 - 250, state.windowHeight - 80,
            GLUT_BITMAP_HELVETICA_12, "Menu: right click.")

    xlabels = state.xdim
    delta = state.plotwidth / float(state.xdim)
    spacing = 24
    lastxplotted = -100

    # X-Axis
    printglstroke(state.windowWidth / 2 - 25, 20, 0.12, 0, "X Coord")
    for i in xrange(xlabels):
        if i > 100:
            spacing = 32
        xplotted = i * delta + state.windowBorder + (delta - 8) / 2 - 3
        if xplotted > lastxplotted + spacing:
            printgl(xplotted, 60, GLUT_BITMAP_HELVETICA_18, "%d", i)
            lastxplotted = xplotted

    ylabels = state.ydim
    delta = float(state.windowHeight - 2 * state.windowBorder) / state.ydim
    spacing = 16
    lastyplotted = -100

    # Y-Axis
    printglstroke(25, state.windowHeight / 2 - 50, 0.12, 90, "Y Coord")
    for i in xrange(ylabels):
        yplotted = i * delta + state.windowBorder + (delta - 18) / 2 + 2
        if yplotted > lastyplotted + spacing:
            printgl(60, yplotted, GLUT_BITMAP_HELVETICA_18, "%d", i)
            lastyplotted = yplotted


def display_key():
    color(UIColours.BLACK)
    keybase = state.windowBorder + 0.20 * (
        state.windowHeight - state.windowBorder)
    printgl(state.windowWidth - 55,
            state.windowHeight - state.windowBorder - 5,
            GLUT_BITMAP_HELVETICA_12, "%.2f", state.highwatermark)
    printgl(state.windowWidth - 55, keybase - 5,
            GLUT_BITMAP_HELVETICA_12, "%.2f", state.lowwatermark)
    interval = 1
    difference = state.highwatermark - state.lowwatermark
    i = 10000
    while i >= 0.1:
        if difference < i:
            interval = i / (20.0 if difference < i/2 else 10.0)
        i /= 10.0
    multipleprinted = 1
    linechunkiness = (state.windowHeight - state.windowBorder - keybase) / \
        float(state.highwatermark - state.lowwatermark)
    # key is only printed if big enough to print
    if state.windowHeight - state.windowBorder - keybase > 0:
        for i in xrange(state.windowHeight - state.windowBorder - keybase):
            temperaturehere = 1.0
            if linechunkiness > 0.0:
                temperaturehere = i / linechunkiness + state.lowwatermark
            colour_calculator(temperaturehere,
                              state.highwatermark, state.lowwatermark)
    
            glBegin(GL_LINES)
            glVertex(state.windowWidth - 65, i + keybase)
            glVertex(state.windowWidth - 65 - KEYWIDTH, i + keybase)
            glEnd()
    
            positiveoffset = temperaturehere - state.lowwatermark
            if positiveoffset >= interval * multipleprinted:
                color(UIColours.BLACK)
                glLineWidth(4.0)
    
                glBegin(GL_LINES)
                glVertex(state.windowWidth - 65, i + keybase)
                glVertex(state.windowWidth - 75, i + keybase)
                glVertex(state.windowWidth - 55 - KEYWIDTH, i + keybase)
                glVertex(state.windowWidth - 65 - KEYWIDTH, i + keybase)
                glEnd()
    
                glLineWidth(1.0)
                printgl(state.windowWidth - 55, i + keybase - 5,
                        GLUT_BITMAP_HELVETICA_12, "%.2f",
                        state.lowwatermark + multipleprinted * interval)
                multipleprinted += 1

        # draw line loop around the key
        color(UIColours.BLACK)
        glLineWidth(2.0)
        glBegin(GL_LINE_LOOP)
        openBoxVertices(state.windowWidth - 65 - KEYWIDTH, keybase,
                        state.windowWidth - 65,
                        state.windowHeight - state.windowBorder);
        glEnd()
        glLineWidth(1.0)


def display_controls():
    boxsize = BOXSIZE
    gap = 10
    xorigin = state.windowWidth - 3 * (boxsize + gap)
    yorigin = state.windowHeight - gap - boxsize
    for box in xrange(3):
        if (not state.freezedisplay and box == 0) \
                or (state.freezedisplay and box == 1) or box == 2:
            color(UIColours.BLACK)
            glBegin(GL_QUADS)
            rectVertices(xorigin + box * (boxsize + gap), yorigin + boxsize,
                         xorigin + box * (boxsize + gap) + boxsize, yorigin)
            glEnd()

            color(UIColours.RED)
            glLineWidth(15.0)
            # now draw shapes on boxes
            if box == 0:
                glBegin(GL_QUADS)
                rectVertices(xorigin + gap, yorigin + boxsize - gap,
                             xorigin + (boxsize + gap) / 2 - gap,
                             yorigin + gap)
                rectVertices(xorigin + (boxsize - gap) / 2 + gap,
                             yorigin + boxsize - gap, xorigin + boxsize - gap,
                             yorigin + gap)
                glEnd()
            elif box == 1:
                glBegin(GL_TRIANGLES)
                glVertex(xorigin + boxsize + 2 * gap,
                         yorigin + boxsize - gap)
                glVertex(xorigin + 2 * boxsize, yorigin + boxsize / 2)
                glVertex(xorigin + boxsize + gap * 2, yorigin + gap)
                glEnd()
            elif box == 2:
                glBegin(GL_LINES)
                glVertex(xorigin + 2 * boxsize + 3 * gap,
                         yorigin + boxsize - gap)
                glVertex(xorigin + 3 * boxsize + gap, yorigin + gap)
                glVertex(xorigin + 3 * boxsize + gap,
                         yorigin + boxsize - gap)
                glVertex(xorigin + 2 * boxsize + 3 * gap, yorigin + gap)
                glEnd()
            glLineWidth(1.0)


def display_gridlines(xsize, ysize):
    color(UIColours.GREY)
    # NB: we only draw if we are not going to completely obscure the data
    if xsize > 3.0:
        # vertical grid lines
        for xcord in xrange(state.xdim):
            glBegin(GL_LINES)
            glVertex(state.windowBorder + xcord * xsize, state.windowBorder)
            glVertex(state.windowBorder + xcord * xsize,
                     state.windowHeight - state.windowBorder)
            glEnd()
    if ysize > 3.0:
        # horizontal grid lines
        for ycord in xrange(state.ydim):
            glBegin(GL_LINES)
            glVertex(state.windowBorder, state.windowBorder + ycord * ysize);
            glVertex(state.windowWidth - state.windowBorder - KEYWIDTH,
                     state.windowBorder + ycord * ysize)
            glEnd()


def display_boxes():
    for box in xrange(CONTROLBOXES * CONTROLBOXES):
        boxx, boxy = divmod(box, CONTROLBOXES)
        if boxx != 1 and boxy != 1:
            continue
        box = Direction(box)
        # only plot NESW+centre
        color(UIColours.BLACK)
        if box == state.livebox:
            color(UIColours.CYAN)
        if state.editmode or box == Direction.CENTRE:
            if box == Direction.CENTRE and state.editmode:
                color(UIColours.GREEN)
    
            glBegin(GL_QUADS)
            rectVertices(
                state.windowWidth - (boxx + 1) * (BOXSIZE + GAP),
                state.yorigin + boxy * (BOXSIZE + GAP) + BOXSIZE,
                state.windowWidth - (boxx + 1) * (BOXSIZE + GAP) + BOXSIZE,
                state.yorigin + boxy * (BOXSIZE + GAP))
            glEnd()
        if box == Direction.CENTRE:
            color(UIColours.WHITE)
            printgl(state.windowWidth - (boxx + 1) * (BOXSIZE + GAP),
                    state.yorigin + boxy * (BOXSIZE + GAP) + BOXSIZE / 2 - 5,
                    GLUT_BITMAP_8_BY_13, " Go!" if state.editmode else "Alter")
        else:
            currentvalue = 0.0
            if box == Direction.NORTH:
                currentvalue = state.alternorth
            elif box == Direction.EAST:
                currentvalue = state.altereast
            elif box == Direction.SOUTH:
                currentvalue = state.altersouth
            elif box == Direction.WEST:
                currentvalue = state.alterwest
            color(UIColours.WHITE if state.editmode and box != state.livebox
                  else UIColours.BLACK)
            printgl(state.windowWidth - (boxx + 1) * (BOXSIZE + GAP),
                    state.yorigin + boxy * (BOXSIZE + GAP) + BOXSIZE / 2 - 5,
                    GLUT_BITMAP_8_BY_13, "%3.1f", currentvalue)


def display_mini_pixel(tileratio, i, ii, xcord, ycord):
    """draw little / mini tiled version in btm left - pixel size"""
    ysize = max(1.0, float(state.windowBorder - 6 * GAP) / state.ydim)
    xsize = max(1.0, ysize * tileratio)

    if is_defined(state.immediate_data[ii]):
        # work out what colour we should plot - sets 'ink' plotting colour
        colour_calculator(state.immediate_data[ii],
                          state.highwatermark, state.lowwatermark)

        # this plots the basic quad box filled as per colour above
        glBegin(GL_QUADS)
        rectVertices(2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                     2 * GAP + (xcord + 1) * xsize,
                     2 * GAP + (ycord + 1) * ysize)
        glEnd()

    # draw outlines for selected box in little / mini version
    if state.livebox == i:
        glLineWidth(1.0)
        # this plots the external black outline of the selected tile
        color(UIColours.BLACK)
        glBegin(GL_LINE_LOOP)
        openBoxVertices(2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                        2 * GAP + (xcord + 1) * xsize,
                        2 * GAP + (ycord + 1) * ysize)
        glEnd()

        # this plots the internal white outline of the selected tile
        color(UIColours.WHITE)
        glBegin(GL_LINE_LOOP)
        openBoxVertices(1 + 2 * GAP + xcord * xsize,
                        1 + 2 * GAP + ycord * ysize,
                        2 * GAP + (xcord + 1) * xsize - 1,
                        2 * GAP + (ycord + 1) * ysize - 1)
        glEnd()


def display_pixel(xsize, ysize, ii, xcord, ycord):
    magnitude = colour_calculator(state.immediate_data[ii],
                                  state.highwatermark, state.lowwatermark)

    # basic plot
    if is_defined(state.immediate_data[ii]):
        glBegin(GL_QUADS)
        rectVertices(state.windowBorder + xcord * xsize,
            state.windowBorder + ycord * ysize,
            state.windowBorder + (xcord + 1) * xsize,
            state.windowBorder + (ycord + 1) * ysize);
        glEnd()

    # if we want to plot values in blocks (and blocks big enough)
    if state.plotvaluesinblocks and xsize > 8 and \
            is_defined(state.immediate_data[ii]):
        # choose if light or dark labels
        color(UIColours.WHITE if magnitude <= 0.6 else UIColours.BLACK)
        printglstroke(state.windowBorder - 20 + (xcord + 0.5) * xsize,
                      state.windowBorder - 6 + (ycord + 0.5) * ysize, 0.12, 0,
                      "%3.2f", state.immediate_data[ii])

def display():
    glPointSize(0.1)
    state.counter += 1  # how many frames have we plotted in our history
    glLoadIdentity()
    clear(UIColours.GREY)
    glClear(GL_COLOR_BUFFER_BIT)
    color(UIColours.BLACK)

    # titles and labels are only printed if border is big enough
    if state.printlabels and not state.fullscreen:
        display_titles_labels()

    # clamp and scale all the values to plottable range
    for i in xrange(state.xdim * state.ydim):
        if is_defined(state.immediate_data[i]):
            datum = state.immediate_data[i]
            datum = clamp(MINDATA, datum, MAXDATA)
            state.immediate_data[i] = datum
            if is_board_address_set():
                if datum > state.highwatermark:
                    state.highwatermark = datum
                if datum < state.lowwatermark:
                    state.lowwatermark = datum

    xsize = max(state.plotwidth / state.xdim, 1.0)
    ysize = float(state.windowHeight - 2 * state.windowBorder) / state.ydim
    tileratio = xsize / ysize
    # plot the pixels
    for i in xrange(state.xdim * state.ydim):
        ii = coordinate_manipulate(i)
        xcord, ycord = convert_index_to_coord(i)
        if not state.fullscreen:
            display_mini_pixel(tileratio, i, ii, xcord, ycord)
        display_pixel(xsize, ysize, ii, xcord, ycord)

    color(UIColours.BLACK)

    # Various bits and pieces of overlay information
    if state.gridlines:
        display_gridlines()
    if not state.fullscreen:
        display_key()
        display_controls()
        if state.pktgone > 0:
            color(UIColours.BLACK)
            if is_board_address_set():
                printgl(state.windowWidth - 3 * (BOXSIZE + GAP) + 5,
                        state.windowHeight - GAP - BOXSIZE - 25,
                        GLUT_BITMAP_8_BY_13, "Packet Sent")
            else:
                printgl(state.windowWidth - 3 * (BOXSIZE + GAP) - 5,
                        state.windowHeight - GAP - BOXSIZE - 25,
                        GLUT_BITMAP_8_BY_13, "Target Unknown")
        display_boxes()

    glutSwapBuffers()
    state.somethingtoplot = False


def trigger_refresh():
    state.somethingtoplot = True


def init():
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB)
    glutInitWindowSize(state.windowWidth + KEYWIDTH)
    glutInitWindowPosition(0, 100)
    glutCreateWindow("VisRT - plotting your network data in real time")

    display.clear(UIColours.BLACK)
    display.color(UIColours.WHITE)
    glShadeModel(GL_SMOOTH)
