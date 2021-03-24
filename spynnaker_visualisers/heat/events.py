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

from enum import IntEnum
import random
import time

import spynnaker_visualisers.opengl_support as gl
import spynnaker_visualisers.glut_framework as glut
from spynnaker_visualisers.heat import display, protocol, state
from spynnaker_visualisers.heat.constants import (
    BOXSIZE, CONTROLBOXES, GAP, KEYWIDTH, Direction)
from spynnaker_visualisers.heat.sdp import (
    all_desired_chips, is_board_port_set)
from spynnaker_visualisers.heat.utils import timestamp

SCROLL_UP = 3
SCROLL_DOWN = 4


class MenuItem(IntEnum):
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


class GUI(glut.GlutFramework):
    def __init__(self):
        super().__init__()
        self._RHMouseMenu = None
        self._needtorebuildmenu = False
        self._menuopen = False

    def _terminate(self, exit_code=0):
        if not state.safelyshutcalls:
            state.safelyshutcalls = True
            if is_board_port_set():
                for i in all_desired_chips():
                    protocol.stop_heatmap_cell(i)
        super()._terminate(exit_code)

    def reshape(self, width, height):
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

        gl.viewport(0, 0, width, height)
        gl.matrix_mode(gl.projection)
        gl.load_identity()

        # an orthographic projection
        gl.orthographic_projction(0.0, width, 0.0, height, -50.0, 50.0)
        gl.matrix_mode(gl.model_view)
        gl.load_identity()

        # indicate we will need to refresh the screen
        display.trigger_refresh()

    def toggle_fullscreen(self):
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

    def pause(self):
        for i in all_desired_chips():
            protocol.pause_heatmap_cell(i)
        state.freezedisplay = True
        state.freezetime = timestamp()
        self.trigger_menu_rebuild()

    def resume(self):
        for i in all_desired_chips():
            protocol.resume_heatmap_cell(i)
        state.freezedisplay = False
        self.trigger_menu_rebuild()

    def keyboard_down(self, key, x, y):
        if key == 'f':
            self.toggle_fullscreen()
        elif key == 'c':
            state.cleardown()
        elif key == 'q':
            self._terminate()
        elif key == '"':
            self.pause()
        elif key == 'p':
            self.resume()
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
                    -1 if state.livebox == Direction.NORTH
                    else Direction.NORTH)
        elif key == 'e':
            if state.editmode:
                state.livebox = (
                    -1 if state.livebox == Direction.EAST
                    else Direction.EAST)
        elif key == 's':
            if state.editmode:
                state.livebox = (
                    -1 if state.livebox == Direction.SOUTH
                    else Direction.SOUTH)
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
                    protocol.set_heatmap_cell(
                        i, state.alternorth, state.altereast,
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
                protocol.set_heatmap_cell(
                    i, state.alternorth, state.altereast,
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
                protocol.set_heatmap_cell(
                    i, state.alternorth, state.altereast,
                    state.altersouth, state.alterwest)

    def in_control_box(self, box, x, y):
        boxsize = BOXSIZE
        gap = 10
        sum = boxsize + gap  # @ReservedAssignment
        xorigin = state.windowWidth - 3 * sum
        yorigin = state.windowHeight - sum
        return xorigin + box * sum <= x < xorigin + box * sum + boxsize \
            and yorigin <= state.windowHeight - y < yorigin + boxsize

    def handle_control_box_click(self, x, y):
        if self.in_control_box(0, x, y) and not state.freezedisplay:
            self.pause()
            display.trigger_refresh()
            return True
        if self.in_control_box(1, x, y) and state.freezedisplay:
            self.resume()
            display.trigger_refresh()
            return True
        if self.in_control_box(2, x, y):
            self._terminate()
            return True
        return False

    def in_box(self, boxx, boxy, x, y):
        D = BOXSIZE + GAP
        x_o = state.windowWidth - (boxx + 1) * D
        y_o = state.windowHeight - D
        return x_o <= x < x_o + BOXSIZE and \
            y_o + boxy * D <= state.windowHeight - y < y_o + BOXSIZE + boxy * D

    def get_box_id(self, x, y):
        for boxx in range(CONTROLBOXES):
            for boxy in range(CONTROLBOXES):
                if self.in_box(boxx, boxy, x, y) and (boxx == 1 or boxy == 1):
                    return Direction(boxx * CONTROLBOXES + boxy)
        return -1

    def handle_main_box_click(self, x, y):
        selectedbox = self.get_box_id(x, y)
        if selectedbox == -1:
            return False
        elif selectedbox == Direction.CENTRE:
            state.livebox = -1
            if not state.editmode:
                state.editmode = True
            else:
                for i in all_desired_chips():
                    protocol.set_heatmap_cell(
                        i, state.alternorth, state.altereast,
                        state.altersouth, state.alterwest)
        elif state.editmode:
            if selectedbox == state.livebox:
                state.livebox = -1
            else:
                state.livebox = selectedbox
        display.trigger_refresh()
        return True

    def mouse_button_press(self, button, mousestate, x, y):
        # pylint: disable=arguments-differ
        if mousestate != glut.mouseDown:
            return
        if button == glut.leftButton:
            acted = self.handle_control_box_click(x, y) or \
                self.handle_main_box_click(x, y)
            # if you didn't manage to do something useful, then likely
            # greyspace around the figure was clicked (should now deselect
            # any selection)
            if not acted and state.livebox != -1:
                state.livebox = -1
                display.trigger_refresh()
                self.rebuild_menu()
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

    def idle(self):
        self.rebuild_menu_if_needed()
        frame_us = 1000000 / state.max_frame_rate
        howlongtowait = (
            state.starttime + state.counter * frame_us - timestamp())
        if howlongtowait > 0:
            time.sleep(howlongtowait / 1000000.0)
        if state.pktgone > 0 and timestamp() > state.pktgone + 1000000:
            state.pktgone = 0
        display.trigger_refresh()
        if state.somethingtoplot:
            display.display()

    # -------------------------------------------------------------------------

    def menu_callback(self, option):
        if option == MenuItem.XFORM_XFLIP:
            state.xflip = not state.xflip
        elif option == MenuItem.XFORM_YFLIP:
            state.yflip = not state.yflip
        elif option == MenuItem.XFORM_VECTORFLIP:
            state.vectorflip = not state.vectorflip
        elif option == MenuItem.XFORM_ROTATEFLIP:
            state.rotateflip = not state.rotateflip
        elif option == MenuItem.XFORM_REVERT:
            state.cleardown()
        elif option == MenuItem.MENU_BORDER_TOGGLE:
            state.gridlines = not state.gridlines
        elif option == MenuItem.MENU_NUMBER_TOGGLE:
            state.plotvaluesinblocks = not state.plotvaluesinblocks
        elif option == MenuItem.MENU_FULLSCREEN_TOGGLE:
            self.toggle_fullscreen()
        elif option == MenuItem.MENU_PAUSE:
            for i in all_desired_chips():
                protocol.pause_heatmap_cell(i)
            state.freezedisplay = True
            state.freezetime = timestamp()
        elif option == MenuItem.MENU_RESUME:
            for i in all_desired_chips():
                protocol.resume_heatmap_cell(i)
            state.freezedisplay = False
        elif option == MenuItem.MENU_QUIT:
            self._terminate()
        self.trigger_menu_rebuild()

    def rebuild_menu(self):
        if self._RHMouseMenu is not None:
            self.destroy_menu(self._RHMouseMenu)
        self._RHMouseMenu = self.menu(self.menu_callback, [
            ("(X) Mirror (left to right swap)", MenuItem.XFORM_XFLIP),
            ("(Y) Reflect (top to bottom swap)", MenuItem.XFORM_YFLIP),
            ("(V) Vector Swap (Full X+Y Reversal)", MenuItem.XFORM_VECTORFLIP),
            ("90 (D)egree Rotate Toggle", MenuItem.XFORM_ROTATEFLIP),
            ("(C) Revert changes back to default", MenuItem.XFORM_REVERT),
            ("-----", MenuItem.MENU_SEPARATOR),
            ("Grid (B)orders off" if state.gridlines else "Grid (B)orders on",
             MenuItem.MENU_BORDER_TOGGLE),
            ("Numbers (#) off" if state.plotvaluesinblocks
             else "Numbers (#) on",
             MenuItem.MENU_NUMBER_TOGGLE),
            ("(F)ull Screen off" if state.fullscreen else "(F)ull Screen on",
             MenuItem.MENU_FULLSCREEN_TOGGLE),
            ("-----", MenuItem.MENU_SEPARATOR),
            ("(\") Pause Plot", MenuItem.MENU_PAUSE) if not state.freezedisplay
            else ("(P)lay / Restart Plot", MenuItem.MENU_RESUME),
            ("(Q)uit", MenuItem.MENU_QUIT)])
        self.attach_current_menu(glut.rightButton)
        return self._RHMouseMenu

    def menu_state(self, menu_open):
        self._menuopen = menu_open
        self.rebuild_menu_if_needed()

    def trigger_menu_rebuild(self):
        self._needtorebuildmenu = True

    def rebuild_menu_if_needed(self):
        if self._needtorebuildmenu and not self._menuopen:
            self.rebuild_menu()
            self._needtorebuildmenu = False

    # -------------------------------------------------------------------------

    def display(self, dTime):
        display.display()

    def launch(self, args):
        self.start_framework(
            args, "VisRT - plotting your network data in real time",
            state.windowWidth + KEYWIDTH, state.windowHeight, 0, 100, 10)

    def init(self):
        display.init()
        self.rebuild_menu()
