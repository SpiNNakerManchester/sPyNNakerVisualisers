from OpenGL.GL import *  # @UnusedWildImport
from OpenGL.GLUT import *  # @UnusedWildImport
import random

import spynnaker_visualisers.heat.menu as menu
import spynnaker_visualisers.heat.display as display
import spynnaker_visualisers.heat.state as state
from spynnaker_visualisers.heat.constants import KEYWIDTH, BOXSIZE, GAP,\
    CONTROLBOXES, ALTERSTEPSIZE, Direction, MAXFRAMERATE
from spynnaker_visualisers.heat.sdp import all_desired_chips, send_to_chip
from spynnaker_visualisers.heat.utils import timestamp
from spynnaker_visualisers.heat.visualiser import safelyshut, set_heatmap_cell
from spynnaker_visualisers.heat.state import cleardown
import time


SCROLL_UP = 3
SCROLL_DOWN = 4


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
    for i in xrange(all_desired_chips()):
        send_to_chip(i, 0x21, 2, 0, 0, 0, 0, 0, 0, 0)
    state.freezedisplay = True
    state.freezetime = timestamp()
    menu.trigger_rebuild()


def resume():
    for i in xrange(all_desired_chips()):
        send_to_chip(i, 0x21, 3, 0, 0, 0, 0, 0, 0, 0)
    state.freezedisplay = False
    menu.trigger_rebuild()


def keyDown(key, x, y):  # @UnusedVariable
    if key == 'f':
        toggle_fullscreen()
    elif key == 'c':
        cleardown()
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
            state.alternorth += ALTERSTEPSIZE
        elif state.livebox == Direction.SOUTH:
            state.altersouth += ALTERSTEPSIZE
        elif state.livebox == Direction.EAST:
            state.altereast += ALTERSTEPSIZE
        elif state.livebox == Direction.WEST:
            state.alterwest += ALTERSTEPSIZE
    elif key == '-':
        if state.livebox == Direction.NORTH:
            state.alternorth -= ALTERSTEPSIZE
        elif state.livebox == Direction.SOUTH:
            state.altersouth -= ALTERSTEPSIZE
        elif state.livebox == Direction.EAST:
            state.altereast -= ALTERSTEPSIZE
        elif state.livebox == Direction.WEST:
            state.alterwest -= ALTERSTEPSIZE
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
            for i in xrange(all_desired_chips()):
                set_heatmap_cell(i, state.alternorth, state.altereast,
                                 state.altersouth, state.alterwest)
    elif key == '9':
        for i in xrange(all_desired_chips()):
            state.alternorth = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.altereast = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.altersouth = random.uniform(
                state.lowwatermark, state.highwatermark)
            state.alterwest = random.uniform(
                state.lowwatermark, state.highwatermark)
            set_heatmap_cell(i, state.alternorth, state.altereast,
                             state.altersouth, state.alterwest)
    elif key == '0':
        state.livebox = -1
        if (state.alternorth < 1.0 and state.altereast < 1.0 and
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
        for i in xrange(all_desired_chips()):
            set_heatmap_cell(i, state.alternorth, state.altereast,
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
    for boxx in xrange(CONTROLBOXES):
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
            for i in xrange(all_desired_chips()):
                set_heatmap_cell(i, state.alternorth, state.altereast,
                                 state.altersouth, state.alterwest)
    elif state.editmode:
        if selectedbox == state.livebox:
            state.livebox = -1
        else:
            state.livebox = selectedbox
    display.trigger_refresh()
    return True


def mouse(button, state, x, y):
    if state != GLUT_DOWN:
        return
    if button == GLUT_LEFT_BUTTON:
        acted = handle_control_box_click(x, y) or handle_main_box_click(x, y)
        # if you didn't manage to do something useful, then likely greyspace
        # around the figure was clicked (should now deselect any selection)
        if not acted and state.livebox != -1:
            state.livebox = -1
            display.trigger_refresh()
            menu.rebuild()
    elif button == SCROLL_UP:
        if state.livebox == Direction.NORTH:
            state.alternorth += ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.SOUTH:
            state.altersouth += ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.EAST:
            state.altereast += ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.WEST:
            state.alterwest += ALTERSTEPSIZE
            display.trigger_refresh()
    elif button == SCROLL_DOWN:
        if state.livebox == Direction.NORTH:
            state.alternorth -= ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.SOUTH:
            state.altersouth -= ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.EAST:
            state.altereast -= ALTERSTEPSIZE
            display.trigger_refresh()
        elif state.livebox == Direction.WEST:
            state.alterwest -= ALTERSTEPSIZE
            display.trigger_refresh()


def idle():
    menu.rebuild_if_needed()
    frame_us = 1000000 / MAXFRAMERATE
    howlongtowait = state.starttime + state.counter * frame_us - timestamp()
    if howlongtowait > 0:
        time.sleep(howlongtowait / 1000000.0)
    if state.pktgone > 0 and timestamp() > state.pktgone + 1000000:
        state.pktgone = 0
    display.trigger_refresh()
    if state.somethingtoplot:
        display.display()
