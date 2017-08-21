import datetime
import random
import sys
import threading
from OpenGL.GL import *  # @UnusedWildImport
from OpenGL.GLUT import *  # @UnusedWildImport

from .constants import XDIMENSIONS, YDIMENSIONS, EACHCHIPX, EACHCHIPY, \
    HIWATER, LOWATER, KEYWIDTH, HISTORYSIZE, UIColours
import spynnaker_visualisers.heat.config as config
import spynnaker_visualisers.heat.display as display
import spynnaker_visualisers.heat.events as events
import spynnaker_visualisers.heat.menu as menu
import spynnaker_visualisers.heat.sdp as sdp
import spynnaker_visualisers.heat.state as state

MAXDATA = 65535.0
MINDATA = -65535.0
NOTDEFINED = -66666.0


def timestamp():
    td = datetime.datetime.now()
    return td.microsecond + (td.second + td.day * 86400) * 10**6


def send_to_chip(id, port, command, *args):  # @ReservedAssignment
    x = id / (XDIMENSIONS / EACHCHIPX)
    y = id % (XDIMENSIONS / EACHCHIPX)
    dest = 256 * x + y
    sdp.sender(dest, port, command, *args)


def all_desired_chips():
    # for i in xrange((XDIMENSIONS * YDIMENSIONS) / (EACHCHIPX * EACHCHIPY)):
    #     yield i
    yield 1


def clamp(low, value, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def rand_num(limit, from_num=0):
    return random.randint(from_num, limit-1)


def glRectVertices(x1, y1, x2, y2):
    glVertex2f(x1, y1)
    glVertex2f(x1, y2)
    glVertex2f(x2, y2)
    glVertex2f(x2, y1)


def glOpenBoxVertices(x1, y1, x2, y2):
    glVertex2f(x1, y1)
    glVertex2f(x1, y2)
    glVertex2f(x2, y2)
    glVertex2f(x2, y1)


def is_defined(f):
    return f > NOTDEFINED + 1


def trigger_display_refresh():
    state.somethingtoplot = True


def color(colour_id):
    if colour_id == UIColours.BLACK:
        glColor4f(0, 0, 0, 1)
    elif colour_id == UIColours.WHITE:
        glColor4f(1, 1, 1, 1)
    elif colour_id == UIColours.RED:
        glColor4f(1, 0, 0, 1)
    elif colour_id == UIColours.GREEN:
        glColor4f(0, 0.6, 0, 1)
    elif colour_id == UIColours.CYAN:
        glColor4f(0, 1, 1, 1)
    elif colour_id == UIColours.GREY:
        glColor4f(0.8, 0.8, 0.8, 1)


def set_heatmap_cell(id, north, east, south, west):  # @ReservedAssignment
    send_to_chip(id, 0x21, 1, 0, 0, 0,
                 int(north * 65536), int(east * 65536),
                 int(south * 65536), int(west * 65536))

# -------------------------------------------------------------------

def cleardown():
    for i in xrange(state.xdim * state.ydim):
        state.immediate_data[i] = NOTDEFINED
    state.highwatermark = HIWATER
    state.lowwatermark = LOWATER
    state.xflip = False
    state.yflip = False
    state.vectorflip = False
    state.rotateflip = False


def safelyshut():
    if not state.safelyshutcalls:
        state.safelyshutcalls = True
        if sdp.is_board_port_set():
            for i in all_desired_chips():
                send_to_chip(i, 0x21, 0, 0, 0, 0, 0, 0, 0, 0)
        config.finalise_memory()
    sys.exit(0)


def run_GUI(argv):
    glutInit(argv)

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB)
    glutInitWindowSize(state.windowWidth + KEYWIDTH)
    glutInitWindowPosition(0, 100)
    glutCreateWindow("VisRT - plotting your network data in real time")

    glClearColor(0.0, 0.0, 0.0, 1.0)
    color(UIColours.WHITE)
    glShadeModel(GL_SMOOTH)

    menu.rebuild()

    glutDisplayFunc(display.display)
    glutReshapeFunc(events.reshape)
    glutIdleFunc(events.idle)
    glutKeyboardFunc(events.keyDown)
    glutMouseFunc(events.mouse)
    glutCloseFunc(safelyshut)
    glutMenuStateFunc(menu.logifmenuopen)
    glutMainLoop()


def main(argv):
    configfile = config.parse_arguments(argv)
    if configfile is None:
        configfile = "visparam.json"
    config.param_load(configfile)

    cleardown()
    state.starttimez = timestamp()

    state.history_data = [
        [NOTDEFINED for _ in xrange(state.xdim * state.ydim)]
        for _ in xrange(HISTORYSIZE)]
    
    sdp.init_listening()
    threading.Thread(target=sdp.input_thread)

    run_GUI(argv)
    print("goodbye")
    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv)
