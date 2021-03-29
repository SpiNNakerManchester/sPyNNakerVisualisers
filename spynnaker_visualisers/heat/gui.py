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
    WINBORDER, WINHEIGHT, WINWIDTH, HIWATER, LOWATER,
    UIColours, Direction)
from spynnaker_visualisers.heat.protocol import HeatProtocol
from spynnaker_visualisers.heat.utils import clamp, is_defined, timestamp


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


class GUI(glut.GlutFramework, HeatProtocol):
    """
    The main class that implements the Heat Map client.
    """
    def __init__(self, filename=None):
        glut.GlutFramework.__init__(self)
        HeatProtocol.__init__(self, filename)
        self._right_mouse_menu = None
        self._need_to_rebuild_menu = False
        self._menu_open = False
        self._plot_values_in_blocks = False
        self._safely_shut_calls = False
        self._grid_lines = False
        self._full_screen = False
        self._x_flip = False
        self._y_flip = False
        self._vector_flip = False
        self._rotate_flip = False
        self._edit_mode = True
        self._live_box = -1
        self._alter_north = 40.0
        self._alter_south = 10.0
        self._alter_east = 10.0
        self._alter_west = 40.0
        self._win_border = WINBORDER
        self._win_height = WINHEIGHT
        self._win_width = WINWIDTH + KEYWIDTH
        self.__old_win_border = 0
        self._x_origin = 0
        self._y_origin = GAP
        self._print_labels = False
        self._low_water_mark = HIWATER
        self._high_water_mark = LOWATER
        self._counter = 0
        self._freeze_time = 0

    @glut.key_press_handler("q", "Q")
    def _terminate(self, exit_code=0):
        if not self._safely_shut_calls:
            self._safely_shut_calls = True
            if self.is_board_port_set():
                for i in self.all_desired_chips():
                    self.stop_heatmap_cell(i)
        super()._terminate(exit_code)

    def reshape(self, width, height):
        self._win_width = width
        self.plot_width = width - 2 * self._win_border - KEYWIDTH
        if self._full_screen:
            self._win_width += KEYWIDTH
            self.plot_width = self._win_width - KEYWIDTH
        if self._win_width < 2 * self._win_border + KEYWIDTH:
            self._win_width = 2 * self._win_border + KEYWIDTH
            self.plot_width = 0
        self._win_height = height

        # turn off label printing if too small, and on if larger than this
        # threshold.
        self._print_labels = self.plot_width > 1 and (
            height - 2 * self._win_border > 1)

        gl.viewport(0, 0, width, height)
        gl.matrix_mode(gl.projection)
        gl.load_identity()

        # an orthographic projection
        gl.orthographic_projction(0.0, width, 0.0, height, -50.0, 50.0)
        gl.matrix_mode(gl.model_view)
        gl.load_identity()

        # indicate we will need to refresh the screen
        self.trigger_refresh()

    @glut.key_press_handler("f", "F")
    def toggle_fullscreen(self):
        if self._full_screen:
            self._win_border = self.__old_win_border
            self._win_width -= KEYWIDTH
            self.plot_width = self._win_width - 2 * self._win_border - KEYWIDTH
        else:
            self.__old_win_border = self._win_border
            self._win_border = 0
            self._win_width += KEYWIDTH
            self.plot_width = self._win_width - KEYWIDTH
        self._full_screen = not self._full_screen

    @glut.key_press_handler('"')
    def pause(self):
        for i in self.all_desired_chips():
            self.pause_heatmap_cell(i)
        self.freeze_display = True
        self._freeze_time = timestamp()
        self.trigger_menu_rebuild()

    @glut.key_press_handler("p", "P")
    def resume(self):
        for i in self.all_desired_chips():
            self.resume_heatmap_cell(i)
        self.freeze_display = False
        self.trigger_menu_rebuild()

    @glut.key_press_handler("c", "C")
    def cleardown(self):
        super().cleardown()
        self._x_flip = False
        self._y_flip = False
        self._vector_flip = False
        self._rotate_flip = False
        self._high_water_mark = HIWATER
        self._low_water_mark = LOWATER

    def param_load(self, filename):
        super().param_load(filename)

        self._win_border = WINBORDER
        self._win_height = WINHEIGHT
        self._win_width = WINWIDTH + KEYWIDTH
        self.plot_width = self._win_width - 2 * self._win_border - KEYWIDTH
        self._print_labels = (self._win_border >= 100)

        self._x_origin = self._win_width + KEYWIDTH - CONTROLBOXES * (
            BOXSIZE + GAP)

    @glut.key_press_handler("b", "B")
    def __gridlines_toggle(self):
        self._grid_lines = not self._grid_lines

    @glut.key_press_handler("#")
    def __values_toggle(self):
        self._plot_values_in_blocks = not self._plot_values_in_blocks

    @glut.key_press_handler("d", "D")
    def __rotate_flip(self):
        self._rotate_flip = not self._rotate_flip

    @glut.key_press_handler("v", "V")
    def __vector_flip(self):
        self._vector_flip = not self._vector_flip

    @glut.key_press_handler("x", "X")
    def __x_flip(self):
        self._x_flip = not self._x_flip

    @glut.key_press_handler("y", "Y")
    def __y_flip(self):
        self._y_flip = not self._y_flip

    @glut.key_press_handler("+")
    def __increment(self):
        if self._live_box == Direction.NORTH:
            self._alter_north += self.alter_step
        elif self._live_box == Direction.SOUTH:
            self._alter_south += self.alter_step
        elif self._live_box == Direction.EAST:
            self._alter_east += self.alter_step
        elif self._live_box == Direction.WEST:
            self._alter_west += self.alter_step

    @glut.key_press_handler("-")
    def __decrement(self):
        if self._live_box == Direction.NORTH:
            self._alter_north -= self.alter_step
        elif self._live_box == Direction.SOUTH:
            self._alter_south -= self.alter_step
        elif self._live_box == Direction.EAST:
            self._alter_east -= self.alter_step
        elif self._live_box == Direction.WEST:
            self._alter_west -= self.alter_step

    @glut.key_press_handler("n", "N")
    def __north(self):
        if self._edit_mode:
            self._live_box = (
                -1 if self._live_box == Direction.NORTH else Direction.NORTH)

    @glut.key_press_handler("s", "S")
    def __south(self):
        if self._edit_mode:
            self._live_box = (
                -1 if self._live_box == Direction.SOUTH else Direction.SOUTH)

    @glut.key_press_handler("e", "E")
    def __east(self):
        if self._edit_mode:
            self._live_box = (
                -1 if self._live_box == Direction.EAST else Direction.EAST)

    @glut.key_press_handler("w", "W")
    def __west(self):
        if self._edit_mode:
            self._live_box = (
                -1 if self._live_box == Direction.WEST else Direction.WEST)

    @glut.key_press_handler("a", "A")
    def __alter(self):
        if not self._edit_mode:
            self._edit_mode = True
            self._live_box = -1

    @glut.key_press_handler("g", "G")
    def __send_edit(self):
        if self._edit_mode:
            self._live_box = -1
            for i in self.all_desired_chips():
                self.set_heatmap_cell(
                    i, self._alter_north, self._alter_east,
                    self._alter_south, self._alter_west)

    @glut.key_press_handler("9")
    def __randomise(self):
        for i in self.all_desired_chips():
            self._alter_north = random.uniform(
                self._low_water_mark, self._high_water_mark)
            self._alter_east = random.uniform(
                self._low_water_mark, self._high_water_mark)
            self._alter_south = random.uniform(
                self._low_water_mark, self._high_water_mark)
            self._alter_west = random.uniform(
                self._low_water_mark, self._high_water_mark)
            self.set_heatmap_cell(
                i, self._alter_north, self._alter_east,
                self._alter_south, self._alter_west)

    @glut.key_press_handler("0")
    def __reset_alterations(self):
        self._live_box = -1
        if self._alter_north < 1.0 and self._alter_east < 1.0 and (
                self._alter_south < 1.0) and self._alter_west < 1.0:
            # If very low, reinitialise
            self._alter_north = 40.0
            self._alter_east = 10.0
            self._alter_south = 10.0
            self._alter_west = 40.0
        else:
            self._alter_north = 0.0
            self._alter_east = 0.0
            self._alter_south = 0.0
            self._alter_west = 0.0
        for i in self.all_desired_chips():
            self.set_heatmap_cell(
                i, self._alter_north, self._alter_east,
                self._alter_south, self._alter_west)

    def in_control_box(self, box, x, y):
        gap = 10
        sum = BOXSIZE + gap  # @ReservedAssignment
        xorigin = self._win_width - CONTROLBOXES * sum
        yorigin = self._win_height - sum
        return (xorigin + box * sum <= x < xorigin + box * sum + BOXSIZE) and (
            yorigin <= self._win_height - y < yorigin + BOXSIZE)

    def handle_control_box_click(self, x, y):
        if self.in_control_box(0, x, y) and not self.freeze_display:
            self.pause()
            self.trigger_refresh()
            return True
        if self.in_control_box(1, x, y) and self.freeze_display:
            self.resume()
            self.trigger_refresh()
            return True
        if self.in_control_box(2, x, y):
            self._terminate()
            return True
        return False

    def in_box(self, boxx, boxy, x, y):
        D = BOXSIZE + GAP
        x_o = self._win_width - (boxx + 1) * D
        y_o = self._win_height - D
        return x_o <= x < x_o + BOXSIZE and (
            y_o + boxy * D <= self._win_height - y < (
                y_o + BOXSIZE + boxy * D))

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
            self._live_box = -1
            if not self._edit_mode:
                self._edit_mode = True
            else:
                for i in self.all_desired_chips():
                    self.set_heatmap_cell(
                        i, self._alter_north, self._alter_east,
                        self._alter_south, self._alter_west)
        elif self._edit_mode:
            if selectedbox == self._live_box:
                self._live_box = -1
            else:
                self._live_box = selectedbox
        self.trigger_refresh()
        return True

    @glut.mouse_down_handler(glut.leftButton)
    def __left(self, x, y):
        acted = self.handle_control_box_click(x, y) or \
            self.handle_main_box_click(x, y)
        # if you didn't manage to do something useful, then likely
        # greyspace around the figure was clicked (should now deselect
        # any selection)
        if not acted and self._live_box != -1:
            self._live_box = -1
            self.trigger_refresh()
            self.rebuild_menu()

    @glut.mouse_down_handler(glut.scrollUp)
    def __scroll_up(self, _x, _y):
        if self._live_box == Direction.NORTH:
            self._alter_north += self.alter_step
        elif self._live_box == Direction.SOUTH:
            self._alter_south += self.alter_step
        elif self._live_box == Direction.EAST:
            self._alter_east += self.alter_step
        elif self._live_box == Direction.WEST:
            self._alter_west += self.alter_step
        else:
            return
        self.trigger_refresh()

    @glut.mouse_down_handler(glut.scrollDown)
    def __scroll_down(self, _x, _y):
        if self._live_box == Direction.NORTH:
            self._alter_north -= self.alter_step
        elif self._live_box == Direction.SOUTH:
            self._alter_south -= self.alter_step
        elif self._live_box == Direction.EAST:
            self._alter_east -= self.alter_step
        elif self._live_box == Direction.WEST:
            self._alter_west -= self.alter_step
        else:
            return
        self.trigger_refresh()

    def idle(self):
        self.rebuild_menu_if_needed()
        frame_us = 1000000 / self.max_frame_rate
        howlongtowait = self.start_time + self._counter * frame_us - timestamp()
        if howlongtowait > 0:
            time.sleep(howlongtowait / 1000000.0)
        if self.pkt_gone > 0 and timestamp() > self.pkt_gone + 1000000:
            self.pkt_gone = 0
        self.trigger_refresh()
        if self.something_to_plot:
            self.display(None)

    # -------------------------------------------------------------------------

    def __menu_callback(self, option):
        if option == MenuItem.XFORM_XFLIP:
            self.__x_flip()
        elif option == MenuItem.XFORM_YFLIP:
            self.__y_flip()
        elif option == MenuItem.XFORM_VECTORFLIP:
            self.__vector_flip()
        elif option == MenuItem.XFORM_ROTATEFLIP:
            self.__rotate_flip()
        elif option == MenuItem.XFORM_REVERT:
            self.cleardown()
        elif option == MenuItem.MENU_BORDER_TOGGLE:
            self.__gridlines_toggle()
        elif option == MenuItem.MENU_NUMBER_TOGGLE:
            self.__values_toggle()
        elif option == MenuItem.MENU_FULLSCREEN_TOGGLE:
            self.toggle_fullscreen()
        elif option == MenuItem.MENU_PAUSE:
            self.pause()
        elif option == MenuItem.MENU_RESUME:
            self.resume()
        elif option == MenuItem.MENU_QUIT:
            self._terminate()
        self.trigger_menu_rebuild()

    def rebuild_menu(self):
        if self._right_mouse_menu is not None:
            self.destroy_menu(self._right_mouse_menu)
        self._right_mouse_menu = self.menu(self.__menu_callback, [
            ("(X) Mirror (left to right swap)", MenuItem.XFORM_XFLIP),
            ("(Y) Reflect (top to bottom swap)", MenuItem.XFORM_YFLIP),
            ("(V) Vector Swap (Full X+Y Reversal)", MenuItem.XFORM_VECTORFLIP),
            ("90 (D)egree Rotate Toggle", MenuItem.XFORM_ROTATEFLIP),
            ("(C) Revert changes back to default", MenuItem.XFORM_REVERT),
            ("-----", MenuItem.MENU_SEPARATOR),
            ("Grid (B)orders off" if self._grid_lines
             else "Grid (B)orders on",
             MenuItem.MENU_BORDER_TOGGLE),
            ("Numbers (#) off" if self._plot_values_in_blocks
             else "Numbers (#) on",
             MenuItem.MENU_NUMBER_TOGGLE),
            ("(F)ull Screen off" if self._full_screen
             else "(F)ull Screen on",
             MenuItem.MENU_FULLSCREEN_TOGGLE),
            ("-----", MenuItem.MENU_SEPARATOR),
            (("(\") Pause Plot", MenuItem.MENU_PAUSE)
             if not self.freeze_display
             else ("(P)lay / Restart Plot", MenuItem.MENU_RESUME)),
            ("(Q)uit", MenuItem.MENU_QUIT)])
        self.attach_current_menu(glut.rightButton)
        return self._right_mouse_menu

    def menu_state(self, menu_open):
        self._menu_open = menu_open
        self.rebuild_menu_if_needed()

    def trigger_menu_rebuild(self):
        self._need_to_rebuild_menu = True

    def rebuild_menu_if_needed(self):
        if self._need_to_rebuild_menu and not self._menu_open:
            self.rebuild_menu()
            self._need_to_rebuild_menu = False

    # -------------------------------------------------------------------------

    def launch(self, args):
        self.start_time = timestamp()
        self._init_listening()
        threading.Thread(target=self.input_thread)
        self.start_framework(
            args, "VisRT - plotting your network data in real time",
            self._win_width + KEYWIDTH, self._win_height, 0, 100, 10)

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

    def colour_calculator(self, val):
        hi = self._high_water_mark
        lo = self._low_water_mark
        diff = float(hi - lo)
        if diff < 0.0001:
            fillcolour = 1.0
        else:
            fillcolour = (clamp(lo, val, hi) - lo) / diff
        r = self._interpolate(self.GAMUT, 0, fillcolour)
        g = self._interpolate(self.GAMUT, 1, fillcolour)
        b = self._interpolate(self.GAMUT, 2, fillcolour)
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

    def _index_to_coord(self, index):
        tileid, elementid = divmod(index, self.each_x * self.each_y)
        elementx, elementy = divmod(elementid, self.each_y)
        tilex, tiley = divmod(tileid, self.y_chips)
        return (tilex * self.each_x + elementx,
                tiley * self.each_y + elementy)

    def _coord_to_index(self, x, y):
        tilex, elementx = divmod(x, self.each_x)
        tiley, elementy = divmod(y, self.each_y)
        elementid = elementx * self.each_y + elementy
        return elementid + self.each_x * self.each_y * (
            tilex * self.y_chips + tiley)

    def coordinate_manipulate(self, i):
        if self._x_flip or self._y_flip or self._vector_flip or (
                self._rotate_flip):
            tileid, elementid = divmod(
                i, self.each_x * self.each_y)
            elementx, elementy = divmod(elementid, self.each_y)
            tilex, tiley = divmod(tileid, self.y_chips)

            # Flip ycoords
            if self._y_flip:
                elementy = self.each_y - 1 - elementy
                tiley = self.y_chips - 1 - tiley
            # Flip xcoords
            if self._x_flip:
                elementx = self.each_x - 1 - elementx
                tilex = self.x_chips - 1 - tilex

            elementid = elementx * self.each_y + elementy
            i = elementid + self.each_x * self.each_y * (
                tilex * self.x_chips + tiley)

            # Go back to front (cumulative)
            if self._vector_flip:
                i = self.ydim * self.xdim - 1 - i
            # Rotate
            if self._rotate_flip:
                xcoord, ycoord = self._index_to_coord(i)
                i = self._coord_to_index(ycoord, self.xdim - 1 - xcoord)
        return i

    # ----------------------------------------------------------------------------

    def _display_titles_labels(self):
        self.write_large(
            self._win_width // 2 - 200, self._win_height - 50, self.title)
        self.write_large(
            self._win_width // 2 - 250, self._win_height - 80,
            "Menu: right click.",
            font=glut.Font.Helvetica12)

        xlabels = self.xdim
        delta = self.plot_width / float(self.xdim)
        spacing = 24
        lastxplotted = -100

        # X-Axis
        self.write_small(self._win_width // 2 - 25, 20, 0.12, 0, "X Coord")
        for i in range(xlabels):
            if i > 100:
                spacing = 32
            xplotted = i * delta + self._win_border + (delta - 8) // 2 - 3
            if xplotted > lastxplotted + spacing:
                self.write_large(
                    xplotted, 60, "%d", i,
                    font=glut.Font.Helvetica18)
                lastxplotted = xplotted

        ylabels = self.ydim
        delta = (self._win_height - 2 * self._win_border) / self.ydim
        spacing = 16
        lastyplotted = -100

        # Y-Axis
        self.write_small(25, self._win_height // 2 - 50, 0.12, 90, "Y Coord")
        for i in range(ylabels):
            yplotted = i * delta + self._win_border + (delta - 18) // 2 + 2
            if yplotted > lastyplotted + spacing:
                self.write_large(
                    60, yplotted, "%d", i,
                    font=glut.Font.Helvetica18)
                lastyplotted = yplotted

    def _display_key(self):
        self.color(UIColours.BLACK)
        keybase = self._win_border + 0.20 * (
            self._win_height - self._win_border)
        self.write_large(self._win_width - 55,
                         self._win_height - self._win_border - 5,
                         "%.2f", self._high_water_mark,
                         font=glut.Font.Helvetica12)
        self.write_large(self._win_width - 55, keybase - 5,
                         "%.2f", self._low_water_mark,
                         font=glut.Font.Helvetica12)
        interval = 1
        difference = self._high_water_mark - self._low_water_mark
        i = 10000
        while i >= 0.1:
            if difference < i:
                interval = i / (20.0 if difference < i / 2 else 10.0)
            i /= 10.0
        multipleprinted = 1
        linechunkiness = (
            self._win_height - self._win_border - keybase) / (
                self._high_water_mark - self._low_water_mark)
        # key is only printed if big enough to print
        if self._win_height - self._win_border - keybase > 0:
            for i in range(int(
                    self._win_height - self._win_border - keybase)):
                temperaturehere = 1.0
                if linechunkiness > 0.0:
                    temperaturehere = i / linechunkiness + self._low_water_mark
                self.colour_calculator(temperaturehere)

                with gl.draw(gl.lines):
                    gl.vertex(self._win_width - 65, i + keybase)
                    gl.vertex(self._win_width - 65 - KEYWIDTH, i + keybase)

                positiveoffset = temperaturehere - self._low_water_mark
                if positiveoffset >= interval * multipleprinted:
                    self.color(UIColours.BLACK)
                    gl.line_width(4.0)

                    with gl.draw(gl.lines):
                        gl.vertex(self._win_width - 65, i + keybase)
                        gl.vertex(self._win_width - 75, i + keybase)
                        gl.vertex(self._win_width - 55 - KEYWIDTH, i + keybase)
                        gl.vertex(self._win_width - 65 - KEYWIDTH, i + keybase)

                    gl.line_width(1.0)
                    self.write_large(
                        self._win_width - 55, i + keybase - 5, "%.2f",
                        self._low_water_mark + multipleprinted * interval,
                        font=glut.Font.Helvetica12)
                    multipleprinted += 1

            # draw line loop around the key
            self.color(UIColours.BLACK)
            gl.line_width(2.0)
            self._draw_open_box(
                self._win_width - 65 - KEYWIDTH, keybase,
                self._win_width - 65,
                self._win_height - self._win_border)
            gl.line_width(1.0)

    def _display_controls(self):
        boxsize = BOXSIZE
        gap = 10
        xorigin = self._win_width - 3 * (boxsize + gap)
        yorigin = self._win_height - gap - boxsize
        for box in range(3):
            if (not self.freeze_display and box == 0) or (
                    self.freeze_display and box == 1) or box == 2:
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
                        self._win_border + xcord * xsize,
                        self._win_border)
                    gl.vertex(
                        self._win_border + xcord * xsize,
                        self._win_height - self._win_border)
        if ysize > 3.0:
            # horizontal grid lines
            for ycord in range(self.ydim):
                with gl.draw(gl.lines):
                    gl.vertex(
                        self._win_border,
                        self._win_border + ycord * ysize)
                    gl.vertex(
                        self._win_width - self._win_border - KEYWIDTH,
                        self._win_border + ycord * ysize)

    def _display_boxes(self):
        for box in range(CONTROLBOXES * CONTROLBOXES):
            boxx, boxy = divmod(box, CONTROLBOXES)
            if boxx != 1 and boxy != 1:
                continue
            x_o = self._win_width - (boxx + 1) * (BOXSIZE + GAP)
            y_o = self._y_origin + boxy * (BOXSIZE + GAP)
            box = Direction(box)
            # only plot NESW+centre
            self.color(UIColours.BLACK)
            if box == self._live_box:
                self.color(UIColours.CYAN)
            if self._edit_mode or box == Direction.CENTRE:
                if box == Direction.CENTRE and self._edit_mode:
                    self.color(UIColours.GREEN)
                self._draw_filled_box(x_o, y_o + BOXSIZE, x_o + BOXSIZE, y_o)
            if box == Direction.CENTRE:
                self.color(UIColours.WHITE)
                self.write_large(
                    x_o, y_o + BOXSIZE // 2 - 5,
                    " Go!" if self._edit_mode else "Alter",
                    font=glut.Font.Bitmap8x13)
            else:
                currentvalue = 0.0
                if box == Direction.NORTH:
                    currentvalue = self._alter_north
                elif box == Direction.EAST:
                    currentvalue = self._alter_east
                elif box == Direction.SOUTH:
                    currentvalue = self._alter_south
                elif box == Direction.WEST:
                    currentvalue = self._alter_west
                self.color(UIColours.WHITE
                           if self._edit_mode and box != self._live_box
                           else UIColours.BLACK)
                self.write_large(
                    x_o, y_o + BOXSIZE // 2 - 5,
                    "%3.1f", currentvalue,
                    font=glut.Font.Bitmap8x13)

    def _display_mini_pixel(self, tileratio, i, ii, xcord, ycord):
        """draw little / mini tiled version in btm left - pixel size"""
        ysize = max(1.0, (self._win_border - 6 * GAP) / self.ydim)
        xsize = max(1.0, ysize * tileratio)

        if is_defined(self.immediate_data[ii]):
            # work out what colour we should plot - sets 'ink' plotting colour
            self.colour_calculator(self.immediate_data[ii])

            # this plots the basic quad box filled as per colour above
            self._draw_filled_box(
                2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                2 * GAP + (xcord + 1) * xsize,
                2 * GAP + (ycord + 1) * ysize)

        # draw outlines for selected box in little / mini version
        if self._live_box == i:
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
        magnitude = self.colour_calculator(self.immediate_data[ii])

        # basic plot
        if is_defined(self.immediate_data[ii]):
            self._draw_filled_box(
                self._win_border + xcord * xsize,
                self._win_border + ycord * ysize,
                self._win_border + (xcord + 1) * xsize,
                self._win_border + (ycord + 1) * ysize)

        # if we want to plot values in blocks (and blocks big enough)
        if self._plot_values_in_blocks and xsize > 8 and \
                is_defined(self.immediate_data[ii]):
            # choose if light or dark labels
            self.color(
                UIColours.WHITE if magnitude <= 0.6 else UIColours.BLACK)
            self.write_small(
                self._win_border - 20 + (xcord + 0.5) * xsize,
                self._win_border - 6 + (ycord + 0.5) * ysize,
                0.12, 0, "%3.2f", self.immediate_data[ii])

    def display(self, dTime):
        gl.point_size(0.1)
        self._counter += 1
        # how many frames have we plotted in our history
        gl.load_identity()
        self.clear(UIColours.GREY)
        gl.clear(gl.color_buffer_bit)
        self.color(UIColours.BLACK)

        # titles and labels are only printed if border is big enough
        if self._print_labels and not self._full_screen:
            self._display_titles_labels()

        # clamp and scale all the values to plottable range
        for i in range(self.xdim * self.ydim):
            if is_defined(self.immediate_data[i]):
                datum = clamp(MINDATA, self.immediate_data[i], MAXDATA)
                self.immediate_data[i] = datum
                if self.is_board_address_set():
                    if datum > self._high_water_mark:
                        self._high_water_mark = datum
                    if datum < self._low_water_mark:
                        self._low_water_mark = datum

        xsize = max(self.plot_width / self.xdim, 1.0)
        ysize = float(
            self._win_height - 2 * self._win_border) / \
            self.ydim
        tileratio = xsize / ysize
        # plot the pixels
        for i in range(self.xdim * self.ydim):
            ii = self.coordinate_manipulate(i)
            xcord, ycord = self._index_to_coord(i)
            if not self._full_screen:
                self._display_mini_pixel(tileratio, i, ii, xcord, ycord)
            self._display_pixel(xsize, ysize, ii, xcord, ycord)

        self.color(UIColours.BLACK)

        # Various bits and pieces of overlay information
        if self._grid_lines:
            self._display_gridlines(xsize, ysize)
        if not self._full_screen:
            self._display_key()
            self._display_controls()
            if self.pkt_gone > 0:
                self.color(UIColours.BLACK)
                if self.is_board_address_set():
                    self.write_large(
                        self._win_width - 3 * (BOXSIZE + GAP) + 5,
                        self._win_height - GAP - BOXSIZE - 25,
                        "Packet Sent",
                        font=glut.Font.Bitmap8x13)
                else:
                    self.write_large(
                        self._win_width - 3 * (BOXSIZE + GAP) - 5,
                        self._win_height - GAP - BOXSIZE - 25,
                        "Target Unknown",
                        font=glut.Font.Bitmap8x13)
            self._display_boxes()

        self.something_to_plot = False

    def trigger_refresh(self):
        self.something_to_plot = True

    def init(self):
        self.clear(UIColours.BLACK)
        self.color(UIColours.WHITE)
        gl.shade_model(gl.smooth)
        self.rebuild_menu()
