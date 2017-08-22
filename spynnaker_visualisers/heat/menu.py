from OpenGL.GLUT import glutCreateMenu, glutDestroyMenu, glutAddMenuEntry, \
    glutAttachMenu, GLUT_RIGHT_BUTTON, GLUT_MENU_IN_USE
import spynnaker_visualisers.heat.state as state
import spynnaker_visualisers.heat.sdp as sdp 
from spynnaker_visualisers.heat import utils
from spynnaker_visualisers.heat.visualiser import safelyshut
from spynnaker_visualisers.heat.events import toggle_fullscreen


_RHMouseMenu = None
_needtorebuildmenu = False
_menuopen = False
# Should be enum...
MENU_SEPARATOR = 0
XFORM_XFLIP = 1
XFORM_YFLIP = 2
XFORM_VECTORFLIP = 3
XFORM_ROTATEFLIP = 4
XFORM_REVERT = 5
MENU_BORDER_TOGGLE = 6
MENU_NUMBER_TOGGLE = 7
MENU_FULLSCREEN_TOGGLE = 8
MENU_PAUSE = 9
MENU_RESUME = 10
MENU_QUIT = 11


def callback(option):
    if option == XFORM_XFLIP:
        state.xflip = not state.xflip
    elif option == XFORM_YFLIP:
        state.yflip = not state.yflip
    elif option == XFORM_VECTORFLIP:
        state.vectorflip = not state.vectorflip
    elif option == XFORM_ROTATEFLIP:
        state.rotateflip = not state.rotateflip
    elif option == XFORM_REVERT:
        state.cleardown()
    elif option == MENU_BORDER_TOGGLE:
        state.gridlines = not state.gridlines
    elif option == MENU_NUMBER_TOGGLE:
        state.plotvaluesinblocks = not state.plotvaluesinblocks
    elif option == MENU_FULLSCREEN_TOGGLE:
        toggle_fullscreen()
    elif option == MENU_PAUSE:
        for i in sdp.all_desired_chips():
            sdp.send_to_chip(i, 0x21, 2, 0, 0, 0, 0, 0, 0, 0)
        state.freezedisplay = True
        state.freezetime = utils.timestamp()
    elif option == MENU_RESUME:
        for i in sdp.all_desired_chips():
            sdp.send_to_chip(i, 0x21, 3, 0, 0, 0, 0, 0, 0, 0)
        state.freezedisplay = False
    elif option == MENU_QUIT:
        safelyshut()
    _needtorebuildmenu = True


def rebuild():
    global _RHMouseMenu
    glutDestroyMenu(_RHMouseMenu)
    _RHMouseMenu = glutCreateMenu(callback)

    glutAddMenuEntry("(X) Mirror (left to right swap)", XFORM_XFLIP)
    glutAddMenuEntry("(Y) Reflect (top to bottom swap)", XFORM_YFLIP)
    glutAddMenuEntry("(V) Vector Swap (Full X+Y Reversal)", XFORM_VECTORFLIP)
    glutAddMenuEntry("90 (D)egree Rotate Toggle", XFORM_ROTATEFLIP)
    glutAddMenuEntry("(C) Revert changes back to default", XFORM_REVERT)
    glutAddMenuEntry("-----", MENU_SEPARATOR)
    if state.gridlines:
        glutAddMenuEntry("Grid (B)orders off", MENU_BORDER_TOGGLE)
    else:
        glutAddMenuEntry("Grid (B)orders on", MENU_BORDER_TOGGLE)
    if state.plotvaluesinblocks:
        glutAddMenuEntry("Numbers (#) off", MENU_NUMBER_TOGGLE)
    else:
        glutAddMenuEntry("Numbers (#) on", MENU_NUMBER_TOGGLE)
    if state.fullscreen:
        glutAddMenuEntry("(F)ull Screen off", MENU_FULLSCREEN_TOGGLE)
    else:
        glutAddMenuEntry("(F)ull Screen on", MENU_FULLSCREEN_TOGGLE)
    glutAddMenuEntry("-----", MENU_SEPARATOR)
    if not state.freezedisplay:
        glutAddMenuEntry("(\") Pause Plot", MENU_PAUSE)
    else:
        glutAddMenuEntry("(P)lay / Restart Plot", MENU_RESUME)
    glutAddMenuEntry("(Q)uit", MENU_QUIT)
    glutAttachMenu(GLUT_RIGHT_BUTTON)
    return _RHMouseMenu


def logifmenuopen(status, x, y):  # @UnusedVariable
    global _menuopen
    _menuopen = (status == GLUT_MENU_IN_USE)
    rebuild_if_needed()

def trigger_rebuild():
    _needtorebuildmenu = True

def rebuild_if_needed():
    if _needtorebuildmenu and not _menuopen:
        rebuild()
        _needtorebuildmenu = False
