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

MAXDATA = 65535.0
MINDATA = -65535.0
NOTDEFINED = -66666.0

XDIMENSIONS = 32
YDIMENSIONS = 32
EACHCHIPX = 4
EACHCHIPY = 4
XCHIPS = XDIMENSIONS // EACHCHIPX
YCHIPS = YDIMENSIONS // EACHCHIPY

TIMEWINDOW = 3.5
HISTORYSIZE = 3500

HIWATER = 10.0
LOWATER = 0.0
ALTERSTEPSIZE = 1.0

WINBORDER = 110
WINHEIGHT = 700
WINWIDTH = 850
KEYWIDTH = 50

CONTROLBOXES = 3
BOXSIZE = 40
GAP = 5

MAXFRAMERATE = 25
SDPPORT = 17894
FIXEDPOINT = 16

MAXBLOCKSIZE = 364
SPINN_HELLO = 0x41
P2P_SPINN_PACKET = 0x3A
STIM_IN_SPINN_PACKET = 0x49
MTU = 1515


class UIColours(IntEnum):
    BLACK = 0
    WHITE = 1
    RED = 2
    GREEN = 3
    CYAN = 4
    GREY = 5


class Direction(IntEnum):
    EAST = 1
    SOUTH = 3
    CENTRE = 4
    NORTH = 5
    WEST = 7
