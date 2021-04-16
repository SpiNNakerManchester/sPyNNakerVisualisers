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

import socket
import struct
import sys

from spynnaker_visualisers.heat.constants import (
    MTU, SPINN_HELLO, TIMEWINDOW, NOTDEFINED)
from spynnaker_visualisers.heat.state import State
from spynnaker_visualisers.heat.utils import timestamp


class HeatProtocol(State):
    """
    The Heatmap protocol, used for controlling what chips are doing
    """

    _PROTOCOL_ID = 0x21
    _SCP_HEADER = struct.Struct("<BxBBBBHHHHIII")
    _DEST = struct.Struct("!HH")
    _HALF = struct.Struct("<H")
    _WORD = struct.Struct("<I")
    _ALL_CHIPS = False

    def __init__(self, filename=None):
        State.__init__(self, filename)
        self.__board_ip = None
        self.__board_ip_set = False
        self.__board_port = None
        self.__board_address = None
        self.__sock_input = None
        self.__sock_output = None
        self.__last_history_line_updated = 0
        self.something_to_plot = False
        self.start_time = 0
        self.pkt_gone = 0
        self.plot_width = 0
        self._first_receive_time = 0
        # Network code cares about these
        self.freeze_display = False

    def stop_heatmap_cell(self, cell_id):
        self.send_to_chip(cell_id, self._PROTOCOL_ID, 0, 0, 0, 0, 0, 0, 0, 0)

    def set_heatmap_cell(self, cell_id, north, east, south, west):
        # Of particular interest is the encoding of the cell values
        self.send_to_chip(
            cell_id, self._PROTOCOL_ID, 1, 0, 0, 0,
            int(float(north) * 65536), int(float(east) * 65536),
            int(float(south) * 65536), int(float(west) * 65536))

    def pause_heatmap_cell(self, cell_id):
        self.send_to_chip(cell_id, self._PROTOCOL_ID, 2, 0, 0, 0, 0, 0, 0, 0)

    def resume_heatmap_cell(self, cell_id):
        self.send_to_chip(cell_id, self._PROTOCOL_ID, 3, 0, 0, 0, 0, 0, 0, 0)

    def send_to_chip(self, id, port, command, *args):  # @ReservedAssignment
        x, y = divmod(id, self.x_chips)
        dest = 256 * x + y
        self._send(dest, port, command, *args)

    def all_desired_chips(self):
        # FIXME!
        if self._ALL_CHIPS:
            for i in range(self.x_chips * self.y_chips):
                yield i
        else:
            yield 1

    def set_board_ip_address(self, address):
        self.__board_ip = address
        self.__board_ip_set = True

    def get_board_ip_address(self):
        return socket.gethostbyname(self.__board_ip)

    def is_board_address_set(self):
        return self.__board_ip_set

    def is_board_port_set(self):
        return self.__board_port is not None

    @staticmethod
    def __getaddrinfo(host, port):
        args = [host, port, socket.AF_INET, socket.SOCK_DGRAM]
        if not host:
            args += [socket.IPPROTO_UDP, socket.AI_PASSIVE]
        return socket.getaddrinfo(*args)

    def _init_listening(self):
        for family, socktype, proto, _, sockaddr in self.__getaddrinfo(
                None, self.our_port):
            try:
                self.__sock_input = socket.socket(family, socktype, proto)
                self.__sock_input.bind(sockaddr)
            except OSError as e:
                print("falling over to next possible address:", e)
                continue
            return self.__sock_input
        print("failed to bind socket for listening")
        sys.exit(1)

    def _init_sender(self):
        for family, socktype, proto, _, sockaddr in self.__getaddrinfo(
                self.__board_ip, str(self.__board_port)):
            try:
                self.__sock_output = socket.socket(family, socktype, proto)
            except OSError as e:
                print("falling over to next possible address:", e)
                continue
            self.__board_address = sockaddr
            return self.__sock_output
        print("failed to bind socket for listening")
        sys.exit(1)

    _UNBOOTED = struct.Struct("!I")

    def input_thread(self):
        sdp_header_len = 26
        while True:
            try:
                message, (host, port) = self.__sock_input.recvfrom(MTU)
            except OSError:
                return

            buf = memoryview(message)
            num_additional_bytes = len(buf) - sdp_header_len
            if self._UNBOOTED.unpack_from(buf, 2)[0] == SPINN_HELLO:
                continue

            if not self.is_board_address_set():
                self.__board_port = port
                self.set_board_ip_address(host)
                self._init_sender()
                print(f"packet received from {host} on port {port}")
            elif not self.is_board_port_set():
                self.__board_port = port
                self._init_sender()
                print(f"packet received from {host} on port {port}")

            nowtime = timestamp()
            if self._first_receive_time == 0:
                self._first_receive_time = nowtime

            if not self.freeze_display:
                self._update_history_data(nowtime)

            timeperindex = TIMEWINDOW / self.plot_width
            updateline = int(
                (nowtime - self.start_time) / timeperindex / 1000000) \
                % self.history_size
            self._process_heatmap_packet(buf, num_additional_bytes, updateline)

    def _update_history_data(self, updateline):
        linestoclear = updateline - self.__last_history_line_updated
        if linestoclear < 0:
            if updateline + 500 > self.__last_history_line_updated:
                linestoclear = 0
            else:
                linestoclear = updateline + self.history_size - (
                    self.__last_history_line_updated)

        num_pts = self.xdim * self.ydim
        for i in range(linestoclear):
            rowid = (1 + i + self.__last_history_line_updated) \
                % self.history_size
            for j in range(num_pts):
                self.history_data[rowid][j] = NOTDEFINED

        self.__last_history_line_updated = updateline

    def _process_heatmap_packet(self, buf, n_bytes, updateline):
        if self.freeze_display:
            return

        # take the chip ID and works out the chip X,Y coords
        src_addr, = self._HALF.unpack_from(buf, 6)
        xsrc, ysrc = divmod(src_addr, 256)

        # for all extra data (assuming regular array of 4 byte words)
        for i in range(n_bytes // 4):
            arrayindex = (self.x_chips * self.y_chips) \
                * (xsrc * self.x_chips + ysrc) + i
            if arrayindex > self.xdim * self.ydim:
                continue
            data = self._WORD.unpack_from(buf, 26 + i * 4)[0] \
                * self.fixed_point_factor
            self.immediate_data[arrayindex] = data
            self.history_data[updateline][arrayindex] = data
            self.something_to_plot = True

    def _send(self, dest_addr, dest_port, command, arg1, arg2, arg3, *args):
        # FIXME!
        ip_time_out = 0
        flags = 7
        tag = 255
        src_port = 0xFF
        src_addr = 0
        seq = 0

        msg = bytearray(self._SCP_HEADER.pack(
            ip_time_out, flags, tag, dest_port, src_port, 0, 0,
            command, seq, arg1, arg2, arg3))
        self._DEST.pack_into(msg, 6, dest_addr, src_addr)
        for a in args:
            msg += self._WORD.pack(a)

        if self.is_board_address_set():
            self.__sock_output.sendto(msg, self.__board_address)
        self.pkt_gone = timestamp()
