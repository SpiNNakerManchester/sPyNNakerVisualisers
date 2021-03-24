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

import json
import os.path
from spynnaker_visualisers.heat.constants import (
    WINBORDER, WINHEIGHT, WINWIDTH, KEYWIDTH, HIWATER, LOWATER, NOTDEFINED,
    BOXSIZE, GAP, EACHCHIPX, EACHCHIPY, FIXEDPOINT, ALTERSTEPSIZE, SDPPORT,
    HISTORYSIZE, XDIMENSIONS, YDIMENSIONS, MAXFRAMERATE, CONTROLBOXES)


class State:
    def __init__(self):
        self.title = "NO SIMULATION TITLE SUPPLIED"

        self.xdim, self.ydim = XDIMENSIONS, YDIMENSIONS
        self.each_x, self.each_y = EACHCHIPX, EACHCHIPY
        self.x_chips, self.y_chips = 0, 0
        self.plotwidth = 0
        self.windowBorder = WINBORDER
        self.windowHeight = WINHEIGHT
        self.windowWidth = WINWIDTH + KEYWIDTH
        self.oldWindowBorder = 0
        self.xorigin = 0
        self.yorigin = GAP
        self.lowwatermark = HIWATER
        self.highwatermark = LOWATER

        self.plotvaluesinblocks = False
        self.somethingtoplot = False
        self.freezedisplay = False
        self.safelyshutcalls = False
        self.gridlines = False
        self.fullscreen = False
        self.xflip = False
        self.yflip = False
        self.vectorflip = False
        self.rotateflip = False
        self.printlabels = False
        self.editmode = True

        self.livebox = -1
        self.alternorth = 40.0
        self.altersouth = 10.0
        self.altereast = 10.0
        self.alterwest = 40.0
        self.max_frame_rate = MAXFRAMERATE

        self.fixed_point_factor = 0.5 ** FIXEDPOINT
        self.alter_step = ALTERSTEPSIZE
        self.our_port = SDPPORT

        self.counter = 0
        self.freezetime = 0
        self.firstreceivetime = 0
        self.starttime = 0
        self.pktgone = 0

        self.history_size = HISTORYSIZE
        self.immediate_data = list()
        self.history_data = list()

    def param_load(self, filename):
        if not os.path.isfile(filename):
            filename = os.path.join(os.path.dirname(__file__), filename)
        with open(filename) as f:
            data = json.load(f)

        self.title = data.get("title", self.title)
        self.xdim, self.ydim = data.get("dimensions", (self.xdim, self.ydim))
        self.each_x, self.each_y = data.get(
            "chip_size", (self.each_x, self.each_y))
        self.x_chips, self.y_chips = data.get(
            "num_chips", (self.xdim // self.each_x, self.ydim // self.each_y))
        self.history_size = int(data.get("history_size", self.history_size))
        self.max_frame_rate = float(
            data.get("max_frame_rate", self.max_frame_rate))
        self.our_port = int(data.get("sdp_port", self.our_port))
        self.fixed_point_factor = 0.5 ** data.get(
            "fixed_point_digits", FIXEDPOINT)
        self.alter_step = data.get("alter_step_size", self.alter_step)

        self.windowBorder = WINBORDER
        self.windowHeight = WINHEIGHT
        self.windowWidth = WINWIDTH + KEYWIDTH
        self.plotwidth = self.windowWidth - 2 * self.windowBorder - KEYWIDTH
        self.printlabels = (self.windowBorder >= 100)

        self.xorigin = self.windowWidth + KEYWIDTH - CONTROLBOXES * (
            BOXSIZE + GAP)

        n_elems = self.xdim * self.ydim
        self.history_data = [[0.0 for _ in range(n_elems)]
                             for _ in range(self.history_size)]
        self.immediate_data = [0.0 for _ in range(n_elems)]

    def cleardown(self):
        for i in range(self.xdim * self.ydim):
            self.immediate_data[i] = NOTDEFINED
        self.highwatermark = HIWATER
        self.lowwatermark = LOWATER
        self.xflip = False
        self.yflip = False
        self.vectorflip = False
        self.rotateflip = False


state = State()
