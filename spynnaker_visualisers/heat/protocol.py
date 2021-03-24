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

# The Heatmap protocol, used for controlling what chips are doing

from spynnaker_visualisers.heat import sdp
from spynnaker_visualisers.heat.state import state


def stop_heatmap_cell(id):  # @ReservedAssignment
    _send_to_chip(id, 0, 0, 0, 0, 0, 0, 0, 0)


def set_heatmap_cell(id, north, east, south, west):  # @ReservedAssignment
    # Of particular interest is the encoding of the cell values
    _send_to_chip(id, 1, 0, 0, 0,
                  int(float(north) * 65536), int(float(east) * 65536),
                  int(float(south) * 65536), int(float(west) * 65536))


def pause_heatmap_cell(id):  # @ReservedAssignment
    _send_to_chip(id, 2, 0, 0, 0, 0, 0, 0, 0)


def resume_heatmap_cell(id):  # @ReservedAssignment
    _send_to_chip(id, 3, 0, 0, 0, 0, 0, 0, 0)


def _send_to_chip(id, command, *args):  # @ReservedAssignment
    x, y = divmod(id, state.x_chips)
    sdp.sender(256 * x + y, 0x21, command, *args)
