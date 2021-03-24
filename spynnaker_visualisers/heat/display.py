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

import spynnaker_visualisers.opengl_support as gl
from spynnaker_visualisers.glut_framework import Font, GlutFramework as glut
from spynnaker_visualisers.heat.constants import (
    MINDATA, MAXDATA, BOXSIZE, GAP, KEYWIDTH, CONTROLBOXES,
    UIColours, Direction)
from spynnaker_visualisers.heat.state import state
from spynnaker_visualisers.heat.sdp import is_board_address_set
from spynnaker_visualisers.heat.utils import clamp, is_defined


# ----------------------------------------------------------------------------


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


def _interpolate(gamut, idx, fillcolour):
    size = len(gamut) - 1
    val = clamp(0.0, fillcolour, 1.0)
    index = int(val * size)
    offset = (index + 1) - val * size
    return (1 - offset) * gamut[index + 1][idx] + offset * gamut[index][idx]


GAMUT = [(0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0), (1, 1, 0), (1, 0, 0)]


def colour_calculator(val, hi, lo):
    diff = float(hi - lo)
    if diff < 0.0001:
        fillcolour = 1.0
    else:
        fillcolour = (clamp(lo, val, hi) - lo) / diff
    r = _interpolate(GAMUT, 0, fillcolour)
    g = _interpolate(GAMUT, 1, fillcolour)
    b = _interpolate(GAMUT, 2, fillcolour)
    gl.color(r, g, b)
    return fillcolour


# ----------------------------------------------------------------------------


def _draw_filled_box(x1, y1, x2, y2):
    """Generate vertices for a filled box"""
    with gl.draw(gl.quads):
        gl.vertex(x1, y1)
        gl.vertex(x1, y2)
        gl.vertex(x2, y2)
        gl.vertex(x2, y1)


def _draw_open_box(x1, y1, x2, y2):
    """Generate vertices for an open box"""
    with gl.draw(gl.line_loop):
        gl.vertex(x1, y1)
        gl.vertex(x1, y2)
        gl.vertex(x2, y2)
        gl.vertex(x2, y1)


# ----------------------------------------------------------------------------


def convert_index_to_coord(index):
    tileid, elementid = divmod(index, state.each_x * state.each_y)
    elementx, elementy = divmod(elementid, state.each_y)
    tilex, tiley = divmod(tileid, state.y_chips)
    return tilex * state.each_x + elementx, tiley * state.each_y + elementy


def convert_coord_to_index(x, y):
    tilex, elementx = divmod(x, state.each_x)
    tiley, elementy = divmod(y, state.each_y)
    elementid = elementx * state.each_y + elementy
    return elementid + state.each_x * state.each_y * (
        tilex * state.y_chips + tiley)


def coordinate_manipulate(i):
    if state.xflip or state.yflip or state.vectorflip or state.rotateflip:
        tileid, elementid = divmod(i, state.each_x * state.each_y)
        elementx, elementy = divmod(elementid, state.each_y)
        tilex, tiley = divmod(tileid, state.y_chips)

        # Flip ycoords
        if state.yflip:
            elementy = state.each_y - 1 - elementy
            tiley = state.y_chips - 1 - tiley
        # Flip xcoords
        if state.xflip:
            elementx = state.each_x - 1 - elementx
            tilex = state.x_chips - 1 - tilex

        elementid = elementx * state.each_y + elementy
        i = elementid + state.each_x * state.each_y * (
            tilex * state.x_chips + tiley)

        # Go back to front (cumulative)
        if state.vectorflip:
            i = state.ydim * state.xdim - 1 - i
        # Rotate
        if state.rotateflip:
            xcoord, ycoord = convert_index_to_coord(i)
            i = convert_coord_to_index(ycoord, state.xdim - 1 - xcoord)
    return i


# ----------------------------------------------------------------------------


def _display_titles_labels():
    glut.write_large(state.windowWidth // 2 - 200, state.windowHeight - 50,
                     state.title)
    glut.write_large(state.windowWidth // 2 - 250, state.windowHeight - 80,
                     "Menu: right click.",
                     font=Font.Helvetica12)

    xlabels = state.xdim
    delta = state.plotwidth / float(state.xdim)
    spacing = 24
    lastxplotted = -100

    # X-Axis
    glut.write_small(state.windowWidth // 2 - 25, 20, 0.12, 0, "X Coord")
    for i in range(xlabels):
        if i > 100:
            spacing = 32
        xplotted = i * delta + state.windowBorder + (delta - 8) // 2 - 3
        if xplotted > lastxplotted + spacing:
            glut.write_large(xplotted, 60, "%d", i,
                             font=Font.Helvetica18)
            lastxplotted = xplotted

    ylabels = state.ydim
    delta = float(state.windowHeight - 2 * state.windowBorder) / state.ydim
    spacing = 16
    lastyplotted = -100

    # Y-Axis
    glut.write_small(25, state.windowHeight // 2 - 50, 0.12, 90, "Y Coord")
    for i in range(ylabels):
        yplotted = i * delta + state.windowBorder + (delta - 18) // 2 + 2
        if yplotted > lastyplotted + spacing:
            glut.write_large(60, yplotted, "%d", i,
                             font=Font.Helvetica18)
            lastyplotted = yplotted


def _display_key():
    color(UIColours.BLACK)
    keybase = state.windowBorder + 0.20 * (
        state.windowHeight - state.windowBorder)
    glut.write_large(state.windowWidth - 55,
                     state.windowHeight - state.windowBorder - 5,
                     "%.2f", state.highwatermark,
                     font=Font.Helvetica12)
    glut.write_large(state.windowWidth - 55, keybase - 5,
                     "%.2f", state.lowwatermark,
                     font=Font.Helvetica12)
    interval = 1
    difference = state.highwatermark - state.lowwatermark
    i = 10000
    while i >= 0.1:
        if difference < i:
            interval = i / (20.0 if difference < i / 2 else 10.0)
        i /= 10.0
    multipleprinted = 1
    linechunkiness = (state.windowHeight - state.windowBorder - keybase) / \
        float(state.highwatermark - state.lowwatermark)
    # key is only printed if big enough to print
    if state.windowHeight - state.windowBorder - keybase > 0:
        for i in range(int(
                state.windowHeight - state.windowBorder - keybase)):
            temperaturehere = 1.0
            if linechunkiness > 0.0:
                temperaturehere = i / linechunkiness + state.lowwatermark
            colour_calculator(temperaturehere,
                              state.highwatermark, state.lowwatermark)

            with gl.draw(gl.lines):
                gl.vertex(state.windowWidth - 65, i + keybase)
                gl.vertex(state.windowWidth - 65 - KEYWIDTH, i + keybase)

            positiveoffset = temperaturehere - state.lowwatermark
            if positiveoffset >= interval * multipleprinted:
                color(UIColours.BLACK)
                gl.line_width(4.0)

                with gl.draw(gl.lines):
                    gl.vertex(state.windowWidth - 65, i + keybase)
                    gl.vertex(state.windowWidth - 75, i + keybase)
                    gl.vertex(state.windowWidth - 55 - KEYWIDTH, i + keybase)
                    gl.vertex(state.windowWidth - 65 - KEYWIDTH, i + keybase)

                gl.line_width(1.0)
                glut.write_large(
                    state.windowWidth - 55, i + keybase - 5, "%.2f",
                    state.lowwatermark + multipleprinted * interval,
                    font=Font.Helvetica12)
                multipleprinted += 1

        # draw line loop around the key
        color(UIColours.BLACK)
        gl.line_width(2.0)
        _draw_open_box(state.windowWidth - 65 - KEYWIDTH, keybase,
                       state.windowWidth - 65,
                       state.windowHeight - state.windowBorder)
        gl.line_width(1.0)


def _display_controls():
    boxsize = BOXSIZE
    gap = 10
    xorigin = state.windowWidth - 3 * (boxsize + gap)
    yorigin = state.windowHeight - gap - boxsize
    for box in range(3):
        if (not state.freezedisplay and box == 0) \
                or (state.freezedisplay and box == 1) or box == 2:
            color(UIColours.BLACK)
            _draw_filled_box(xorigin + box * (boxsize + gap),
                             yorigin + boxsize,
                             xorigin + box * (boxsize + gap) + boxsize,
                             yorigin)

            color(UIColours.RED)
            gl.line_width(15.0)
            # now draw shapes on boxes
            if box == 0:
                _draw_filled_box(xorigin + gap, yorigin + boxsize - gap,
                                 xorigin + (boxsize + gap) // 2 - gap,
                                 yorigin + gap)
                _draw_filled_box(xorigin + (boxsize - gap) // 2 + gap,
                                 yorigin + boxsize - gap,
                                 xorigin + boxsize - gap,
                                 yorigin + gap)
            elif box == 1:
                with gl.draw(gl.triangles):
                    gl.vertex(xorigin + boxsize + 2 * gap,
                              yorigin + boxsize - gap)
                    gl.vertex(xorigin + 2 * boxsize, yorigin + boxsize // 2)
                    gl.vertex(xorigin + boxsize + gap * 2, yorigin + gap)
            elif box == 2:
                with gl.draw(gl.lines):
                    gl.vertex(xorigin + 2 * boxsize + 3 * gap,
                              yorigin + boxsize - gap)
                    gl.vertex(xorigin + 3 * boxsize + gap, yorigin + gap)
                    gl.vertex(xorigin + 3 * boxsize + gap,
                              yorigin + boxsize - gap)
                    gl.vertex(xorigin + 2 * boxsize + 3 * gap, yorigin + gap)
            gl.line_width(1.0)


def _display_gridlines(xsize, ysize):
    color(UIColours.GREY)
    # NB: we only draw if we are not going to completely obscure the data
    if xsize > 3.0:
        # vertical grid lines
        for xcord in range(state.xdim):
            with gl.draw(gl.lines):
                gl.vertex(
                    state.windowBorder + xcord * xsize, state.windowBorder)
                gl.vertex(
                    state.windowBorder + xcord * xsize,
                    state.windowHeight - state.windowBorder)
    if ysize > 3.0:
        # horizontal grid lines
        for ycord in range(state.ydim):
            with gl.draw(gl.lines):
                gl.vertex(
                    state.windowBorder, state.windowBorder + ycord * ysize)
                gl.vertex(
                    state.windowWidth - state.windowBorder - KEYWIDTH,
                    state.windowBorder + ycord * ysize)


def _display_boxes():
    for box in range(CONTROLBOXES * CONTROLBOXES):
        boxx, boxy = divmod(box, CONTROLBOXES)
        if boxx != 1 and boxy != 1:
            continue
        x_o = state.windowWidth - (boxx + 1) * (BOXSIZE + GAP)
        y_o = state.yorigin + boxy * (BOXSIZE + GAP)
        box = Direction(box)
        # only plot NESW+centre
        color(UIColours.BLACK)
        if box == state.livebox:
            color(UIColours.CYAN)
        if state.editmode or box == Direction.CENTRE:
            if box == Direction.CENTRE and state.editmode:
                color(UIColours.GREEN)

            _draw_filled_box(x_o, y_o + BOXSIZE, x_o + BOXSIZE, y_o)
        if box == Direction.CENTRE:
            color(UIColours.WHITE)
            glut.write_large(x_o, y_o + BOXSIZE // 2 - 5,
                             " Go!" if state.editmode else "Alter",
                             font=Font.Bitmap8x13)
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
            glut.write_large(x_o, y_o + BOXSIZE // 2 - 5,
                             "%3.1f", currentvalue,
                             font=Font.Bitmap8x13)


def _display_mini_pixel(tileratio, i, ii, xcord, ycord):
    """draw little / mini tiled version in btm left - pixel size"""
    ysize = max(1.0, float(state.windowBorder - 6 * GAP) / state.ydim)
    xsize = max(1.0, ysize * tileratio)

    if is_defined(state.immediate_data[ii]):
        # work out what colour we should plot - sets 'ink' plotting colour
        colour_calculator(state.immediate_data[ii],
                          state.highwatermark, state.lowwatermark)

        # this plots the basic quad box filled as per colour above
        _draw_filled_box(2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                         2 * GAP + (xcord + 1) * xsize,
                         2 * GAP + (ycord + 1) * ysize)

    # draw outlines for selected box in little / mini version
    if state.livebox == i:
        gl.line_width(1.0)
        # this plots the external black outline of the selected tile
        color(UIColours.BLACK)
        _draw_open_box(2 * GAP + xcord * xsize, 2 * GAP + ycord * ysize,
                       2 * GAP + (xcord + 1) * xsize,
                       2 * GAP + (ycord + 1) * ysize)

        # this plots the internal white outline of the selected tile
        color(UIColours.WHITE)
        _draw_open_box(1 + 2 * GAP + xcord * xsize,
                       1 + 2 * GAP + ycord * ysize,
                       2 * GAP + (xcord + 1) * xsize - 1,
                       2 * GAP + (ycord + 1) * ysize - 1)


def _display_pixel(xsize, ysize, ii, xcord, ycord):
    magnitude = colour_calculator(state.immediate_data[ii],
                                  state.highwatermark, state.lowwatermark)

    # basic plot
    if is_defined(state.immediate_data[ii]):
        _draw_filled_box(state.windowBorder + xcord * xsize,
                         state.windowBorder + ycord * ysize,
                         state.windowBorder + (xcord + 1) * xsize,
                         state.windowBorder + (ycord + 1) * ysize)

    # if we want to plot values in blocks (and blocks big enough)
    if state.plotvaluesinblocks and xsize > 8 and \
            is_defined(state.immediate_data[ii]):
        # choose if light or dark labels
        color(UIColours.WHITE if magnitude <= 0.6 else UIColours.BLACK)
        glut.write_small(
            state.windowBorder - 20 + (xcord + 0.5) * xsize,
            state.windowBorder - 6 + (ycord + 0.5) * ysize,
            0.12, 0, "%3.2f", state.immediate_data[ii])


def display():
    gl.point_size(0.1)
    state.counter += 1  # how many frames have we plotted in our history
    gl.load_identity()
    clear(UIColours.GREY)
    gl.clear(gl.color_buffer_bit)
    color(UIColours.BLACK)

    # titles and labels are only printed if border is big enough
    if state.printlabels and not state.fullscreen:
        _display_titles_labels()

    # clamp and scale all the values to plottable range
    for i in range(state.xdim * state.ydim):
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
    for i in range(state.xdim * state.ydim):
        ii = coordinate_manipulate(i)
        xcord, ycord = convert_index_to_coord(i)
        if not state.fullscreen:
            _display_mini_pixel(tileratio, i, ii, xcord, ycord)
        _display_pixel(xsize, ysize, ii, xcord, ycord)

    color(UIColours.BLACK)

    # Various bits and pieces of overlay information
    if state.gridlines:
        _display_gridlines(xsize, ysize)
    if not state.fullscreen:
        _display_key()
        _display_controls()
        if state.pktgone > 0:
            color(UIColours.BLACK)
            if is_board_address_set():
                glut.write_large(state.windowWidth - 3 * (BOXSIZE + GAP) + 5,
                                 state.windowHeight - GAP - BOXSIZE - 25,
                                 "Packet Sent",
                                 font=Font.Bitmap8x13)
            else:
                glut.write_large(state.windowWidth - 3 * (BOXSIZE + GAP) - 5,
                                 state.windowHeight - GAP - BOXSIZE - 25,
                                 "Target Unknown",
                                 font=Font.Bitmap8x13)
        _display_boxes()

    state.somethingtoplot = False


def trigger_refresh():
    state.somethingtoplot = True


def init():
    clear(UIColours.BLACK)
    color(UIColours.WHITE)
    gl.shade_model(gl.smooth)
