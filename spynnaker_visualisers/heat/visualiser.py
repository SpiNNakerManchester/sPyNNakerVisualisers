import sys
import threading
from OpenGL.GL import *  # @UnusedWildImport
from OpenGL.GLUT import *  # @UnusedWildImport

from .constants import KEYWIDTH, HISTORYSIZE, NOTDEFINED, UIColours
import spynnaker_visualisers.heat.config as config
import spynnaker_visualisers.heat.display as display
import spynnaker_visualisers.heat.events as events
import spynnaker_visualisers.heat.menu as menu
import spynnaker_visualisers.heat.sdp as sdp
import spynnaker_visualisers.heat.state as state
import spynnaker_visualisers.heat.utils as utils


def clamp(low, value, high):
    if value < low:
        return low
    if value > high:
        return high
    return value


def is_defined(f):
    return f > NOTDEFINED + 1


def trigger_display_refresh():
    state.somethingtoplot = True


def set_heatmap_cell(id, north, east, south, west):  # @ReservedAssignment
    sdp.send_to_chip(id, 0x21, 1, 0, 0, 0,
                     int(north * 65536), int(east * 65536),
                     int(south * 65536), int(west * 65536))

# -------------------------------------------------------------------

def safelyshut():
    if not state.safelyshutcalls:
        state.safelyshutcalls = True
        if sdp.is_board_port_set():
            for i in sdp.all_desired_chips():
                sdp.send_to_chip(i, 0x21, 0, 0, 0, 0, 0, 0, 0, 0)
        config.finalise_memory()
    sys.exit(0)


def run_GUI(argv):
    glutInit(argv)

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB)
    glutInitWindowSize(state.windowWidth + KEYWIDTH)
    glutInitWindowPosition(0, 100)
    glutCreateWindow("VisRT - plotting your network data in real time")

    display.clear(UIColours.BLACK)
    display.color(UIColours.WHITE)
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

    state.cleardown()
    state.starttimez = utils.timestamp()

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
