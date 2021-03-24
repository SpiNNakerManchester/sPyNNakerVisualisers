# Copyright (c) 2017-2021 The University of Manchester
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

from OpenGL.GL import *  # @UnusedWildImport
from OpenGL.GLUT import *  # @UnusedWildImport
import random
import sys
import time

from spynnaker_visualisers.heat import display, protocol, state
from spynnaker_visualisers.heat.constants import \
    BOXSIZE, CONTROLBOXES, GAP, KEYWIDTH, Direction
from spynnaker_visualisers.heat.sdp \
    import all_desired_chips, is_board_port_set
from spynnaker_visualisers.heat.utils import timestamp

SCROLL_UP = 3
SCROLL_DOWN = 4


def safelyshut():
    if not state.safelyshutcalls:
        state.safelyshutcalls = True
        if is_board_port_set():
            for i in all_desired_chips():
                protocol.stop_heatmap_cell(i)
    sys.exit(0)


def reshape(width, height):
    state.windowWidth = width
    state.plotwidth = width - 2 * state.windowBorder - KEYWIDTH
    if state.fullscreen:
        state.windowWidth += KEYWIDTH
        state.plotwidth = state.windowWidth - KEYWIDTH
    if state.windowWidth < 2 * state.windowBorder + KEYWIDTH:
        state.windowWidth = 2 * state.windowBorder + KEYWIDTH
        state.plotwidth = 0
    state.windowHeight = height

    # turn off label printing if too small, and on if larger than this
    # threshold.
    state.printlabels = not (
        state.plotwidth <= 1 or height - 2 * state.windowBorder <= 1)

    glViewport(0, 0, width, height)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()

    # an orthographic projection
    glOrtho(0.0, width, 0.0, height, -50.0, 50.0)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

    # indicate we will need to refresh the screen
    display.trigger_refresh()


def toggle_fullscreen():
    if state.fullscreen:
        state.windowBorder = state.oldWindowBorder
        state.windowWidth -= KEYWIDTH
        state.plotwidth = \
            state.windowWidth - 2 * state.windowBorder - KEYWIDTH
    else:
        state.oldWindowBorder = state.windowBorder
        state.windowBorder = 0
        state.windowWidth += KEYWIDTH
        state.plotwidth = state.windowWidth - KEYWIDTH
    state.fullscreen = not state.fullscreen


def pause():
    for i in all_desired_chips():
        protocol.pause_heatmap_cell(i)
    state.freezedisplay = True
    state.freezetime = timestamp()
    trigger_menu_rebuild()


def resume():
    for i in all_desired_chips():
        protocol.resume_heatmap_cell(i)
    state.freezedisplay = False
    trigger_menu_rebuild()


def keyDown(key, x, y):  # @UnusedVariable
    if key == 'f':
        toggle_fullscreen()
    elif key == 'c':
        state.cleardown()
    elif key == 'q':
        safelyshut()
    elif key == '"':
        pause()
    elif key == 'p':
        resume()
    elif key == 'b':
        state.gridlines = not state.gridlines
    elif key == '#':
        state.plotvaluesinblocks = not state.plotvaluesinblocks
    elif key == 'd':
        state.rotateflip = not state.rotateflip
    elif key == 'v':
        state.vectorflip = not state.vectorflip
    elif key == 'x':
        state.xflip = not state.xflip
    elif key == 'y':
        state.yflip = not state.yflip
    elif key == '+':
        if state.livebox == Direction.NORTH:
            state.alternorth += state.alter_step
        elif state.livebox == Direction.SOUTH:
            state.altersouth += state.alter_step
        elif state.livebox == Direction.EAST:
            state.altereast += state.alter_step
        elif state.livebox == Direction.WEST:
            state.alterwest += state.alter_step
    elif key == '-':
        if state.livebox == Direction.NORTH:
            state.alternorth -= state.alter_step
        elif state.livebox == Direction.SOUTH:
            state.altersouth -= state.alter_step
        elif state.livebox == Direction.EAST:
            state.altereast -= state.alter_step
        elif state.livebox == Direction.WEST:
            state.alterwest -= state.alter_step
    elif key == 'n':
        if state.editmode:
            state.livebox = (
                -1 if state.livebox == Direction.NORTH else Direction.NORTH)
    elif key == 'e':
        if state.editmode:
            state.livebox = (
                -1 if state.livebox == Direction.EAST else Direction.EAST)
    elif key == 's':
        if state.editmode:
            state.livebox = (
                -1 if state.livebox == Direction.SOUTH else Direction.SOUTH)
    elif key == 'w':
        if state.editmode:
            state.livebox = (
                -1 if state.livebox == Direction.WEST else Direction.WEST)
    elif key == 'a':
        if not state.editmode:
            state.editmode = True
            state.livebox = -1
    elif key == 'g':
        if state.editmode:
            state.livebox = -1
            for i in all_desired_chips():
                protocol.set_heatmap_cell(i, state.alternorth, state.altereast,
                                          state.altersouth, state.alterwest)
    elif key == '9':
        for i in all_desired_chips():
            state.alternorth = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.altereast = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.altersouth = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.alterwest = random.uniform(
                state.lowwatermark, state.highwatermark)
            protocol.set_heatmap_cell(i, state.alternorth, state.altereast,
                                      state.altersouth, state.alterwest)
    elif key == '0':
        state.livebox = -1
        if state.alternorth < 1.0 and state.altereast < 1.0 and (
                state.altersouth < 1.0 and state.alterwest < 1.0):
            # If very low, reinitialise
            state.alternorth = 40.0
            state.altereast = 10.0
            state.altersouth = 10.0
            state.alterwest = 40.0
        else:
            state.alternorth = 0.0
            state.altereast = 0.0
            state.altersouth = 0.0
            state.alterwest = 0.0
        for i in all_desired_chips():
            protocol.set_heatmap_cell(i, state.alternorth, state.altereast,
                                      state.altersouth, state.alterwest)


def in_control_box(box, x, y):
    boxsize = BOXSIZE
    gap = 10
    sum = boxsize + gap  # @ReservedAssignment
    xorigin = state.windowWidth - 3 * sum
    yorigin = state.windowHeight - sum
    return xorigin + box * sum <= x < xorigin + box * sum + boxsize \
        and yorigin <= state.windowHeight - y < yorigin + boxsize


def handle_control_box_click(x, y):
    if in_control_box(0, x, y) and not state.freezedisplay:
        pause()
        display.trigger_refresh()
        return True
    if in_control_box(1, x, y) and state.freezedisplay:
        resume()
        display.trigger_refresh()
        return True
    if in_control_box(2, x, y):
        safelyshut()
        return True
    return False


def in_box(boxx, boxy, x, y):
    D = BOXSIZE + GAP
    x_o = state.windowWidth - (boxx + 1) * D
    y_o = state.windowHeight - D
    return x_o <= x < x_o + BOXSIZE and \
        y_o + boxy * D <= state.windowHeight - y < y_o + BOXSIZE + boxy * D


def get_box_id(x, y):
    for boxx in range(CONTROLBOXES):
        for boxy in range(CONTROLBOXES):
            if in_box(boxx, boxy, x, y) and (boxx == 1 or boxy == 1):
                return Direction(boxx * CONTROLBOXES + boxy)
    return -1


def handle_main_box_click(x, y):
    selectedbox = get_box_id(x, y)
    if selectedbox == -1:
        return False
    elif selectedbox == Direction.CENTRE:
        state.livebox = -1
        if not state.editmode:
            state.editmode = True
        else:
            for i in all_desired_chips():
                protocol.set_heatmap_cell(i, state.alternorth, state.altereast,
                                          state.altersouth, state.alterwest)
    elif state.editmode:
        if selectedbox == state.livebox:
            state.livebox = -1
        else:
            state.livebox = selectedbox
    display.trigger_refresh()
    return True


def mouse(button, mousestate, x, y):
    if mousestate != GLUT_DOWN:
        return
    if button == GLUT_LEFT_BUTTON:
        acted = handle_control_box_click(x, y) or handle_main_box_click(x, y)
        # if you didn't manage to do something useful, then likely greyspace
        # around the figure was clicked (should now deselect any selection)
        if not acted and state.livebox != -1:
            state.livebox = -1
            display.trigger_refresh()
            rebuild_menu()
    elif button == SCROLL_UP:
        if state.livebox == Direction.NORTH:
            state.alternorth += state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.SOUTH:
            state.altersouth += state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.EAST:
            state.altereast += state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.WEST:
            state.alterwest += state.alter_step
            display.trigger_refresh()
    elif button == SCROLL_DOWN:
        if state.livebox == Direction.NORTH:
            state.alternorth -= state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.SOUTH:
            state.altersouth -= state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.EAST:
            state.altereast -= state.alter_step
            display.trigger_refresh()
        elif state.livebox == Direction.WEST:
            state.alterwest -= state.alter_step
            display.trigger_refresh()


def idle():
    rebuild_menu_if_needed()
    frame_us = 1000000 / state.max_frame_rate
    howlongtowait = state.starttime + state.counter * frame_us - timestamp()
    if howlongtowait > 0:
        time.sleep(howlongtowait / 1000000.0)
    if state.pktgone > 0 and timestamp() > state.pktgone + 1000000:
        state.pktgone = 0
    display.trigger_refresh()
    if state.somethingtoplot:
        display.display()


# -------------------------------------------------------------------------

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


def menu_callback(option):
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
        for i in all_desired_chips():
            protocol.pause_heatmap_cell(i)
        state.freezedisplay = True
        state.freezetime = timestamp()
    elif option == MENU_RESUME:
        for i in all_desired_chips():
            protocol.resume_heatmap_cell(i)
        state.freezedisplay = False
    elif option == MENU_QUIT:
        safelyshut()
    trigger_menu_rebuild()


def rebuild_menu():
    global _RHMouseMenu
    if _RHMouseMenu is not None:
        glutDestroyMenu(_RHMouseMenu)
    _RHMouseMenu = glutCreateMenu(menu_callback)

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


def logifmenuopen(status):  # @UnusedVariable
    global _menuopen
    _menuopen = (status == GLUT_MENU_IN_USE)
    rebuild_menu_if_needed()


def trigger_menu_rebuild():
    global _needtorebuildmenu
    _needtorebuildmenu = True


def rebuild_menu_if_needed():
    global _needtorebuildmenu
    if _needtorebuildmenu and not _menuopen:
        rebuild_menu()
        _needtorebuildmenu = False


# -------------------------------------------------------------------------


def run_GUI(argv):
    glutInit(argv)

    display.init()
    rebuild_menu()

    glutDisplayFunc(display.display)
    glutReshapeFunc(reshape)
    glutIdleFunc(idle)
    glutKeyboardFunc(keyDown)
    glutMouseFunc(mouse)
    glutCloseFunc(safelyshut)
    glutMenuStateFunc(logifmenuopen)

    glutMainLoop()
