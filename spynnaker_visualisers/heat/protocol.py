# The Heatmap protocol, used for controlling what chips are doing

from spynnaker_visualisers.heat.constants import EACHCHIPX, XDIMENSIONS
from spynnaker_visualisers.heat import sdp


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
    x, y = divmod(id, XDIMENSIONS / EACHCHIPX)
    sdp.sender(256 * x + y, 0x21, command, *args)
