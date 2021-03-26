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
import threading
import time

import spynnaker_visualisers.opengl_support as gl
from spynnaker_visualisers import glut_framework as glut
from spynnaker_visualisers.heat.constants import (
    MINDATA, MAXDATA, BOXSIZE, GAP, KEYWIDTH, CONTROLBOXES,
    UIColours, Direction)
from spynnaker_visualisers.heat.protocol import HeatProtocol
from spynnaker_visualisers.heat.state import State
from spynnaker_visualisers.heat.utils import clamp, is_defined, timestamp

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


class GUI(glut.GlutFramework, State, HeatProtocol):
    def __init__(self, filename=None):
        State.__init__(filename)
        glut.GlutFramework.__init__(self)
        HeatProtocol.__init__(self, self)
        self._RHMouseMenu = None
        self._needtorebuildmenu = False
        self._menuopen = False

    def _terminate(self, exit_code=0):
        if not self.safelyshutcalls:
            self.safelyshutcalls = True
            if self.is_board_port_set():
                for i in self.all_desired_chips():
                    self.stop_heatmap_cell(i)
        super()._terminate(exit_code)

    def reshape(self, width, height):
        self.windowWidth = width
        self.plotwidth = width - 2 * self.windowBorder - KEYWIDTH
        if self.fullscreen:
            self.windowWidth += KEYWIDTH
            self.plotwidth = self.windowWidth - KEYWIDTH
        if self.windowWidth < 2 * self.windowBorder + KEYWIDTH:
            self.windowWidth = 2 * self.windowBorder + KEYWIDTH
            self.plotwidth = 0
        self.windowHeight = height

        # turn off label printing if too small, and on if larger than this
        # threshold.
        self.printlabels = self.plotwidth > 1 and (
            height - 2 * self.windowBorder > 1)

        gl.viewport(0, 0, width, height)
        gl.matrix_mode(gl.projection)
        gl.load_identity()

        # an orthographic projection
        gl.orthographic_projction(0.0, width, 0.0, height, -50.0, 50.0)
        gl.matrix_mode(gl.model_view)
        gl.load_identity()

        # indicate we will need to refresh the screen
        self.trigger_refresh()

    def toggle_fullscreen(self):
        if self.fullscreen:
            self.windowBorder = self.oldWindowBorder
            self.windowWidth -= KEYWIDTH
            self.plotwidth = \
                self.windowWidth - 2 * self.windowBorder - KEYWIDTH
        else:
            self.oldWindowBorder = self.windowBorder
            self.windowBorder = 0
            self.windowWidth += KEYWIDTH
            self.plotwidth = self.windowWidth - KEYWIDTH
        self.fullscreen = not self.fullscreen

    def pause(self):
        for i in self.all_desired_chips():
            self.pause_heatmap_cell(i)
        self.freezedisplay = True
        self.freezetime = timestamp()
        self.trigger_menu_rebuild()

    def resume(self):
        for i in self.all_desired_chips():
            self.resume_heatmap_cell(i)
        self.freezedisplay = False
        self.trigger_menu_rebuild()

    def keyboard_down(self, key, x, y):
        if key == 'f':
            self.toggle_fullscreen()
        elif key == 'c':
            self.cleardown()
        elif key == 'q':
            self._terminate()
        elif key == '"':
            self.pause()
        elif key == 'p':
            self.resume()
        elif key == 'b':
            self.gridlines = not self.gridlines
        elif key == '#':
            self.plotvaluesinblocks = not self.plotvaluesinblocks
        elif key == 'd':
            self.rotateflip = not self.rotateflip
        elif key == 'v':
            self.vectorflip = not self.vectorflip
        elif key == 'x':
            self.xflip = not self.xflip
        elif key == 'y':
            self.yflip = not self.yflip
        elif key == '+':
            if self.livebox == Direction.NORTH:
                self.alternorth += self.alter_step
            elif self.livebox == Direction.SOUTH:
                self.altersouth += self.alter_step
            elif self.livebox == Direction.EAST:
                self.altereast += self.alter_step
            elif self.livebox == Direction.WEST:
                self.alterwest += self.alter_step
        elif key == '-':
            if self.livebox == Direction.NORTH:
                self.alternorth -= self.alter_step
            elif self.livebox == Direction.SOUTH:
                self.altersouth -= self.alter_step
            elif self.livebox == Direction.EAST:
                self.altereast -= self.alter_step
            elif self.livebox == Direction.WEST:
                self.alterwest -= self.alter_step
        elif key == 'n':
            if self.editmode:
                self.livebox = (
                    -1 if self.livebox == Direction.NORTH
                    else Direction.NORTH)
        elif key == 'e':
            if self.editmode:
                self.livebox = (
                    -1 if self.livebox == Direction.EAST
                    else Direction.EAST)
        elif key == 's':
            if self.editmode:
                self.livebox = (
                    -1 if self.livebox == Direction.SOUTH
                    else Direction.SOUTH)
        elif key == 'w':
            if self.editmode:
                self.livebox = (
                    -1 if self.livebox == Direction.WEST
                    else Direction.WEST)
        elif key == 'a':
            if not self.editmode:
                self.editmode = True
                self.livebox = -1
        elif key == 'g':
            if self.editmode:
                self.livebox = -1
                for i in self.all_desired_chips():
                    self.set_heatmap_cell(
                        i, self.alternorth, self.altereast,
                        self.altersouth, self.alterwest)
        elif key == '9':
            for i in self.all_desired_chips():
                self.alternorth = random.uniform(
                    self.lowwatermark, self.highwatermark)
                self.altereast = random.uniform(
                    self.lowwatermark, self.highwatermark)
                self.altersouth = random.uniform(
                    self.lowwatermark, self.highwatermark)
                self.alterwest = random.uniform(
                    self.lowwatermark, self.highwatermark)
                self.set_heatmap_cell(
                    i, self.alternorth, self.altereast,
                    self.altersouth, self.alterwest)
        elif key == '0':
            self.livebox = -1
            if self.alternorth < 1.0 and self.altereast < 1.0 and (
                    self.altersouth < 1.0) and self.alterwest < 1.0:
                # If very low, reinitialise
                self.alternorth = 40.0
                self.altereast = 10.0
                self.altersouth = 10.0
                self.alterwest = 40.0
            else:
                self.alternorth = 0.0
                self.altereast = 0.0
                self.altersouth = 0.0
                self.alterwest = 0.0
            for i in self.all_desired_chips():
                self.set_heatmap_cell(
                    i, self.alternorth, self.altereast,
                    self.altersouth, self.alterwest)

    def in_control_box(self, box, x, y):
        boxsize = BOXSIZE
        gap = 10
        sum = boxsize + gap  # @ReservedAssignment
        xorigin = self.windowWidth - 3 * sum
        yorigin = self.windowHeight - sum
        return (xorigin + box * sum <= x < xorigin + box * sum + boxsize) and (
            yorigin <= self.windowHeight - y < yorigin + boxsize)

    def handle_control_box_click(self, x, y):
        if self.in_control_box(0, x, y) and not self.freezedisplay:
            self.pause()
            self.trigger_refresh()
            return True
        if self.in_control_box(1, x, y) and self.freezedisplay:
            self.resume()
            self.trigger_refresh()
            return True
        if self.in_control_box(2, x, y):
            self._terminate()
            return True
        return False

    def in_box(self, boxx, boxy, x, y):
        D = BOXSIZE + GAP
        x_o = self.windowWidth - (boxx + 1) * D
        y_o = self.windowHeight - D
        return x_o <= x < x_o + BOXSIZE and (
            y_o + boxy * D <= self.windowHeight - y < y_o + BOXSIZE + boxy * D)

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
            self.livebox = -1
            if not self.editmode:
                self.editmode = True
            else:
                for i in self.all_desired_chips():
                    self.set_heatmap_cell(
                        i, self.alternorth, self.altereast,
                        self.altersouth, self.alterwest)
        elif self.editmode:
            if selectedbox == self.livebox:
                self.livebox = -1
            else:
                self.livebox = selectedbox
        self.trigger_refresh()
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
            if not acted and self.livebox != -1:
                self.livebox = -1
                self.trigger_refresh()
                self.rebuild_menu()
        elif button == SCROLL_UP:
            if self.livebox == Direction.NORTH:
                self.alternorth += self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.SOUTH:
                self.altersouth += self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.EAST:
                self.altereast += self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.WEST:
                self.alterwest += self.alter_step
                self.trigger_refresh()
        elif button == SCROLL_DOWN:
            if self.livebox == Direction.NORTH:
                self.alternorth -= self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.SOUTH:
                self.altersouth -= self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.EAST:
                self.altereast -= self.alter_step
                self.trigger_refresh()
            elif self.livebox == Direction.WEST:
                self.alterwest -= self.alter_step
                self.trigger_refresh()

    def idle(self):
        self.rebuild_menu_if_needed()
        frame_us = 1000000 / self.max_frame_rate
        howlongtowait = self.starttime + self.counter * frame_us - timestamp()
        if howlongtowait > 0:
            time.sleep(howlongtowait / 1000000.0)
        if self.pktgone > 0 and timestamp() > self.pktgone + 1000000:
            self.pktgone = 0
        self.trigger_refresh()
        if self.somethingtoplot:
            self.display(None)

    # -------------------------------------------------------------------------

    def menu_callback(self, option):
        if option == MenuItem.XFORM_XFLIP:
            self.xflip = not self.xflip
        elif option == MenuItem.XFORM_YFLIP:
            self.yflip = not self.yflip
        elif option == MenuItem.XFORM_VECTORFLIP:
            self.vectorflip = not self.vectorflip
        elif option == MenuItem.XFORM_ROTATEFLIP:
            self.rotateflip = not self.rotateflip
        elif option == MenuItem.XFORM_REVERT:
            self.cleardown()
        elif option == MenuItem.MENU_BORDER_TOGGLE:
            self.gridlines = not self.gridlines
        elif option == MenuItem.MENU_NUMBER_TOGGLE:
            self.plotvaluesinblocks = not self.plotvaluesinblocks
        elif option == MenuItem.MENU_FULLSCREEN_TOGGLE:
            self.toggle_fullscreen()
        elif option == MenuItem.MENU_PAUSE:
            for i in self.all_desired_chips():
                self.pause_heatmap_cell(i)
            self.freezedisplay = True
            self.freezetime = timestamp()
        elif option == MenuItem.MENU_RESUME:
            for i in self.all_desired_chips():
                self.resume_heatmap_cell(i)
            self.freezedisplay = False
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
            ("Grid (B)orders off" if self.gridlines
             else "Grid (B)orders on",
             MenuItem.MENU_BORDER_TOGGLE),
            ("Numbers (#) off" if self.plotvaluesinblocks
             else "Numbers (#) on",
             MenuItem.MENU_NUMBER_TOGGLE),
            ("(F)ull Screen off" if self.fullscreen
             else "(F)ull Screen on",
             MenuItem.MENU_FULLSCREEN_TOGGLE),
            ("-----", MenuItem.MENU_SEPARATOR),
            (("(\") Pause Plot", MenuItem.MENU_PAUSE)
             if not self.freezedisplay
             else ("(P)lay / Restart Plot", MenuItem.MENU_RESUME)),
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

    def launch(self, args):
        self.__state.starttime = timestamp()
        self._init_listening()
        threading.Thread(target=self.input_thread)
        self.start_framework(
            args, "VisRT - plotting your network data in real time",
            self.windowWidth + KEYWIDTH, self.windowHeight,
            0, 100, 10)

    # -------------------------------------------------------------------------

    @staticmethod
    def color(colour_id):
        if colour_id == UIColours.BLACK:
            gl.color(0, 0, 0)
        elif colour_id == UIColours.WHITE:
            gl.color(1, 1, 1)
        elif colour_id == UIColours.RED:
            gl.color(1, 0, 0)
        elif colour_id == UIColours.GREEN:
            gl.color(0, 0.6, 0)
        elif colour_id == UIColours.CYAN:
            gl.color(0, 1, 1)
        elif colour_id == UIColours.GREY:
            gl.color(0.8, 0.8, 0.8)

    @staticmethod
    def clear(colour_id):
        if colour_id == UIColours.BLACK:
            gl.clear_color(0, 0, 0, 1)
        elif colour_id == UIColours.WHITE:
            gl.clear_color(1, 1, 1, 1)
        elif colour_id == UIColours.RED:
            gl.clear_color(1, 0, 0, 1)
        elif colour_id == UIColours.GREEN:
            gl.clear_color(0, 0.6, 0, 1)
        elif colour_id == UIColours.CYAN:
            gl.clear_color(0, 1, 1, 1)
        elif colour_id == UIColours.GREY:
            gl.clear_color(0.8, 0.8, 0.8, 1)

    @staticmethod
    def _interpolate(gamut, idx, fillcolour):
        size = len(gamut) - 1
        val = clamp(0.0, fillcolour, 1.0)
        index = int(val * size)
        offset = (index + 1) - val * size
        return (1 - offset) * gamut[index + 1][idx] + \
            offset * gamut[index][idx]

    GAMUT = [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0), (1, 1, 0), (1, 0, 0)]

    @classmethod
    def colour_calculator(cls, val, hi, lo):
        diff = float(hi - lo)
        if diff < 0.0001:
            fillcolour = 1.0
        else:
            fillcolour = (clamp(lo, val, hi) - lo) / diff
        r = cls._interpolate(cls.GAMUT, 0, fillcolour)
        g = cls._interpolate(cls.GAMUT, 1, fillcolour)
        b = cls._interpolate(cls.GAMUT, 2, fillcolour)
        gl.color(r, g, b)
        return fillcolour

    # ----------------------------------------------------------------------------

    @staticmethod
    def _draw_filled_box(x1, y1, x2, y2):
        """Generate vertices for a filled box"""
        with gl.draw(gl.quads):
            gl.vertex(x1, y1)
            gl.vertex(x1, y2)
            gl.vertex(x2, y2)
            gl.vertex(x2, y1)

    @staticmethod
    def _draw_open_box(x1, y1, x2, y2):
        """Generate vertices for an open box"""
        with gl.draw(gl.line_loop):
            gl.vertex(x1, y1)
            gl.vertex(x1, y2)
            gl.vertex(x2, y2)
            gl.vertex(x2, y1)

    # ----------------------------------------------------------------------------

    def convert_index_to_coord(self, index):
        tileid, elementid = divmod(index, self.each_x * self.each_y)
        elementx, elementy = divmod(elementid, self.each_y)
        tilex, tiley = divmod(tileid, self.y_chips)
        return (tilex * self.each_x + elementx,
                tiley * self.each_y + elementy)

    def convert_coord_to_index(self, x, y):
        tilex, elementx = divmod(x, self.each_x)
        tiley, elementy = divmod(y, self.each_y)
        elementid = elementx * self.each_y + elementy
        return elementid + self.each_x * self.each_y * (
            tilex * self.y_chips + tiley)

    def coordinate_manipulate(self, i):
        if self.xflip or self.yflip or self.vectorflip or \
                self.rotateflip:
            tileid, elementid = divmod(
                i, self.each_x * self.each_y)
            elementx, elementy = divmod(elementid, self.each_y)
            tilex, tiley = divmod(tileid, self.y_chips)

            # Flip ycoords
            if self.yflip:
                elementy = self.each_y - 1 - elementy
                tiley = self.y_chips - 1 - tiley
            # Flip xcoords
            if self.xflip:
                elementx = self.each_x - 1 - elementx
                tilex = self.x_chips - 1 - tilex

            elementid = elementx * self.each_y + elementy
            i = elementid + self.each_x * self.each_y * (
                tilex * self.x_chips + tiley)

            # Go back to front (cumulative)
            if self.vectorflip:
                i = self.ydim * self.xdim - 1 - i
            # Rotate
            if self.rotateflip:
                xcoord, ycoord = self.convert_index_to_coord(i)
                i = self.convert_coord_to_index(
                    ycoord, self.xdim - 1 - xcoord)
        return i

    # ----------------------------------------------------------------------------

    def _display_titles_labels(self):
        self.write_large(
            self.windowWidth // 2 - 200, self.windowHeight - 50, self.title)
        self.write_large(
            self.windowWidth // 2 - 250, self.windowHeight - 80,
            "Menu: right click.",
            font=glut.Font.Helvetica12)

        xlabels = self.xdim
        delta = self.plotwidth / float(self.xdim)
        spacing = 24
        lastxplotted = -100

        # X-Axis
        self.write_small(self.windowWidth // 2 - 25, 20, 0.12, 0, "X Coord")
        for i in range(xlabels):
            if i > 100:
                spacing = 32
            xplotted = i * delta + self.windowBorder + (delta - 8) // 2 - 3
            if xplotted > lastxplotted + spacing:
                self.write_large(
                    xplotted, 60, "%d", i,
                    font=glut.Font.Helvetica18)
                lastxplotted = xplotted

        ylabels = self.ydim
        delta = (self.windowHeight - 2 * self.windowBorder) / self.ydim
        spacing = 16
        lastyplotted = -100

        # Y-Axis
        self.write_small(25, self.windowHeight // 2 - 50, 0.12, 90, "Y Coord")
        for i in range(ylabels):
            yplotted = i * delta + self.windowBorder + (delta - 18) // 2 + 2
            if yplotted > lastyplotted + spacing:
                self.write_large(
                    60, yplotted, "%d", i,
                    font=glut.Font.Helvetica18)
                lastyplotted = yplotted

    def _display_key(self):
        self.color(UIColours.BLACK)
        keybase = self.windowBorder + 0.20 * (
            self.windowHeight - self.windowBorder)
        self.write_large(self.windowWidth - 55,
                         self.windowHeight - self.windowBorder - 5,
                         "%.2f", self.highwatermark,
                         font=glut.Font.Helvetica12)
        self.write_large(self.windowWidth - 55, keybase - 5,
                         "%.2f", self.lowwatermark,
                         font=glut.Font.Helvetica12)
        interval = 1
        difference = self.highwatermark - self.lowwatermark
        i = 10000
        while i >= 0.1:
            if difference < i:
                interval = i / (20.0 if difference < i / 2 else 10.0)
            i /= 10.0
        multipleprinted = 1
        linechunkiness = (
            self.windowHeight - self.windowBorder - keybase) / \
            (self.highwatermark - self.lowwatermark)
        # key is only printed if big enough to print
        if self.windowHeight - self.windowBorder - keybase > 0:
            for i in range(int(
                    self.windowHeight - self.windowBorder - keybase)):
                temperaturehere = 1.0
                if linechunkiness > 0.0:
                    temperaturehere = i / linechunkiness + self.lowwatermark
                self.colour_calculator(
                    temperaturehere, self.highwatermark, self.lowwatermark)

                with gl.draw(gl.lines):
                    gl.vertex(self.windowWidth - 65, i + keybase)
                    gl.vertex(self.windowWidth - 65 - KEYWIDTH,
                              i + keybase)

                positiveoffset = temperaturehere - self.lowwatermark
                if positiveoffset >= interval * multipleprinted:
                    self.color(UIColours.BLACK)
                    gl.line_width(4.0)

                    with gl.draw(gl.lines):
                        gl.vertex(self.windowWidth - 65, i + keybase)
                        gl.vertex(self.windowWidth - 75, i + keybase)
                        gl.vertex(self.windowWidth - 55 - KEYWIDTH,
                                  i + keybase)
                        gl.vertex(self.windowWidth - 65 - KEYWIDTH,
                                  i + keybase)

                    gl.line_width(1.0)
                    self.write_large(
                        self.windowWidth - 55, i + keybase - 5, "%.2f",
                        self.lowwatermark + multipleprinted * interval,
                        font=glut.Font.Helvetica12)
                    multipleprinted += 1

            # draw line loop around the key
            self.color(UIColours.BLACK)
            gl.line_width(2.0)
            self._draw_open_box(
                self.windowWidth - 65 - KEYWIDTH, keybase,
                self.windowWidth - 65,
                self.windowHeight - self.windowBorder)
            gl.line_width(1.0)

    def _display_controls(self):
        boxsize = BOXSIZE
        gap = 10
        xorigin = self.windowWidth - 3 * (boxsize + gap)
        yorigin = self.windowHeight - gap - boxsize
        for box in range(3):
            if (not self.freezedisplay and box == 0) \
                    or (self.freezedisplay and box == 1) or box == 2:
                self.color(UIColours.BLACK)
                self._draw_filled_box(
                    xorigin + box * (boxsize + gap),
                    yorigin + boxsize,
                    xorigin + box * (boxsize + gap) + boxsize,
                    yorigin)

                self.color(UIColours.RED)
                gl.line_width(15.0)
                # now draw shapes on boxes
                if box == 0:
                    self._draw_filled_box(
                        xorigin + gap, yorigin + boxsize - gap,
                        xorigin + (boxsize + gap) // 2 - gap,
                        yorigin + gap)
                    self._draw_filled_box(
                        xorigin + (boxsize - gap) // 2 + gap,
                        yorigin + boxsize - gap,
                        xorigin + boxsize - gap,
                        yorigin + gap)
                elif box == 1:
                    with gl.draw(gl.triangles):
                        gl.vertex(xorigin + boxsize + 2 * gap,
                                  yorigin + boxsize - gap)
                        gl.vertex(
                            xorigin + 2 * boxsize, yorigin + boxsize // 2)
                        gl.vertex(xorigin + boxsize + gap * 2, yorigin + gap)
                elif box == 2:
                    with gl.draw(gl.lines):
                        gl.vertex(xorigin + 2 * boxsize + 3 * gap,
                                  yorigin + boxsize - gap)
                        gl.vertex(xorigin + 3 * boxsize + gap, yorigin + gap)
                        gl.vertex(xorigin + 3 * boxsize + gap,
                                  yorigin + boxsize - gap)
                        gl.vertex(
                            xorigin + 2 * boxsize + 3 * gap, yorigin + gap)
                gl.line_width(1.0)

    def _display_gridlines(self, xsize, ysize):
        self.color(UIColours.GREY)
        # NB: we only draw if we are not going to completely obscure the data
        if xsize > 3.0:
            # vertical grid lines
            for xcord in range(self.xdim):
                with gl.draw(gl.lines):
                    gl.vertex(
                        self.windowBorder + xcord * xsize,
                        self.windowBorder)
                    gl.vertex(
                        self.windowBorder + xcord * xsize,
                        self.windowHeight - self.windowBorder)
        if ysize > 3.0:
            # horizontal grid lines
            for ycord in range(self.ydim):
                with gl.draw(gl.lines):
                    gl.vertex(
                        self.windowBorder,
                        self.windowBorder + ycord * ysize)
                    gl.vertex(
                        self.windowWidth - self.windowBorder - KEYWIDTH,
                        self.windowBorder + ycord * ysize)

    def _display_boxes(self):
        for box in range(CONTROLBOXES * CONTROLBOXES):
            boxx, boxy = divmod(box, CONTROLBOXES)
            if boxx != 1 and boxy != 1:
                continue
            x_o = self.windowWidth - (boxx + 1) * (BOXSIZE + GAP)
            y_o = self.yorigin + boxy * (BOXSIZE + GAP)
            box = Direction(box)
            # only plot NESW+centre
            self.color(UIColours.BLACK)
            if box == self.livebox:
                self.color(UIColours.CYAN)
            if self.editmode or box == Direction.CENTRE:
                if box == Direction.CENTRE and self.editmode:
                    self.color(UIColours.GREEN)
                self._draw_filled_box(x_o, y_o + BOXSIZE, x_o + BOXSIZE, y_o)
            if box == Direction.CENTRE:
                self.color(UIColours.WHITE)
                self.write_large(
                    x_o, y_o + BOXSIZE // 2 - 5,
                    " Go!" if self.editmode else "Alter",
                    font=glut.Font.Bitmap8x13)
            else:
                currentvalue = 0.0
                if box == Direction.NORTH:
                    currentvalue = self.alternorth
                elif box == Direction.EAST:
                    currentvalue = self.altereast
                elif box == Direction.SOUTH:
                    currentvalue = self.altersouth
                elif box == Direction.WEST:
                    currentvalue = self.alterwest
                self.color(UIColours.WHITE
                           if self.editmode and box != self.livebox
                           else UIColours.BLACK)
                self.write_large(
                    x_o, y_o + BOXSIZE // 2 - 5,
                    "%3.1f", currentvalue,
                    font=glut.Font.Bitmap8x13)

    def _display_mini_pixel(self, tileratio, i, ii, xcord, ycord):
        """draw little / mini tiled version in btm left - pixel size"""
        ysize = max(1.0, (self.windowBorder - 6 * GAP) / self.ydim)
        xsize = max(1.0, ysize * tileratio)

        if is_defined(self.immediate_data[ii]):
            # work out what colour we should plot - sets 'ink' plotting colour
            self.colour_calculator(
                self.immediate_data[ii],
                self.highwatermark, self.lowwatermark)

            # this plots the basic quad box filled as per colour above
            self._draw_filled_box(
                2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                2 * GAP + (xcord + 1) * xsize,
                2 * GAP + (ycord + 1) * ysize)

        # draw outlines for selected box in little / mini version
        if self.livebox == i:
            gl.line_width(1.0)
            # this plots the external black outline of the selected tile
            self.color(UIColours.BLACK)
            self._draw_open_box(
                2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                2 * GAP + (xcord + 1) * xsize,
                2 * GAP + (ycord + 1) * ysize)

            # this plots the internal white outline of the selected tile
            self.color(UIColours.WHITE)
            self._draw_open_box(
                1 + 2 * GAP + xcord * xsize,
                1 + 2 * GAP + ycord * ysize,
                2 * GAP + (xcord + 1) * xsize - 1,
                2 * GAP + (ycord + 1) * ysize - 1)

    def _display_pixel(self, xsize, ysize, ii, xcord, ycord):
        magnitude = self.colour_calculator(
            self.immediate_data[ii],
            self.highwatermark, self.lowwatermark)

        # basic plot
        if is_defined(self.immediate_data[ii]):
            self._draw_filled_box(
                self.windowBorder + xcord * xsize,
                self.windowBorder + ycord * ysize,
                self.windowBorder + (xcord + 1) * xsize,
                self.windowBorder + (ycord + 1) * ysize)

        # if we want to plot values in blocks (and blocks big enough)
        if self.plotvaluesinblocks and xsize > 8 and \
                is_defined(self.immediate_data[ii]):
            # choose if light or dark labels
            self.color(
                UIColours.WHITE if magnitude <= 0.6 else UIColours.BLACK)
            self.write_small(
                self.windowBorder - 20 + (xcord + 0.5) * xsize,
                self.windowBorder - 6 + (ycord + 0.5) * ysize,
                0.12, 0, "%3.2f", self.immediate_data[ii])

    def display(self, dTime):
        gl.point_size(0.1)
        self.counter += 1
        # how many frames have we plotted in our history
        gl.load_identity()
        self.clear(UIColours.GREY)
        gl.clear(gl.color_buffer_bit)
        self.color(UIColours.BLACK)

        # titles and labels are only printed if border is big enough
        if self.printlabels and not self.fullscreen:
            self._display_titles_labels()

        # clamp and scale all the values to plottable range
        for i in range(self.xdim * self.ydim):
            if is_defined(self.immediate_data[i]):
                datum = self.immediate_data[i]
                datum = clamp(MINDATA, datum, MAXDATA)
                self.immediate_data[i] = datum
                if self.is_board_address_set():
                    if datum > self.highwatermark:
                        self.highwatermark = datum
                    if datum < self.lowwatermark:
                        self.lowwatermark = datum

        xsize = max(self.plotwidth / self.xdim, 1.0)
        ysize = float(
            self.windowHeight - 2 * self.windowBorder) / \
            self.ydim
        tileratio = xsize / ysize
        # plot the pixels
        for i in range(self.xdim * self.ydim):
            ii = self.coordinate_manipulate(i)
            xcord, ycord = self.convert_index_to_coord(i)
            if not self.fullscreen:
                self._display_mini_pixel(tileratio, i, ii, xcord, ycord)
            self._display_pixel(xsize, ysize, ii, xcord, ycord)

        self.color(UIColours.BLACK)

        # Various bits and pieces of overlay information
        if self.gridlines:
            self._display_gridlines(xsize, ysize)
        if not self.fullscreen:
            self._display_key()
            self._display_controls()
            if self.pktgone > 0:
                self.color(UIColours.BLACK)
                if self.is_board_address_set():
                    self.write_large(
                        self.windowWidth - 3 * (BOXSIZE + GAP) + 5,
                        self.windowHeight - GAP - BOXSIZE - 25,
                        "Packet Sent",
                        font=glut.Font.Bitmap8x13)
                else:
                    self.write_large(
                        self.windowWidth - 3 * (BOXSIZE + GAP) - 5,
                        self.windowHeight - GAP - BOXSIZE - 25,
                        "Target Unknown",
                        font=glut.Font.Bitmap8x13)
            self._display_boxes()

        self.somethingtoplot = False

    def trigger_refresh(self):
        self.somethingtoplot = True

    def init(self):
        self.clear(UIColours.BLACK)
        self.color(UIColours.WHITE)
        gl.shade_model(gl.smooth)
        self.rebuild_menu()
