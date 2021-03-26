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
from spynnaker_visualisers.heat.utils import timestamp


class SDP:
    _SCP_HEADER = struct.Struct("<BxBBBBHHHHIII")
    _DEST = struct.Struct("!HH")
    _HALF = struct.Struct("<H")
    _WORD = struct.Struct("<I")

    def __init__(self, state):
        self.__state = state
        self._board_ip = None
        self._board_ip_set = False
        self._board_port = None
        self._board_address = None
        self._sock_input = None
        self._sock_output = None
        self._last_history_line_updated = 0

    def send_to_chip(self, id, port, command, *args):  # @ReservedAssignment
        x, y = divmod(id, self.__state.x_chips)
        dest = 256 * x + y
        self._send(dest, port, command, *args)

    def all_desired_chips(self):
        # for i in xrange(state.x_chips * state.y_chips):
        #     yield i
        yield 1

    def set_board_ip_address(self, address):
        self._board_ip = address
        self._board_ip_set = True

    def get_board_ip_address(self):
        return socket.gethostbyname(self._board_ip)

    def is_board_address_set(self):
        return self._board_ip_set

    def is_board_port_set(self):
        return self._board_port is not None

    @staticmethod
    def __getaddrinfo(host, port):
        args = [host, port, socket.AF_INET, socket.SOCK_DGRAM]
        if not host:
            args += [socket.IPPROTO_UDP, socket.AI_PASSIVE]
        return socket.getaddrinfo(*args)

    def _init_listening(self):
        for family, socktype, proto, _, sockaddr in self.__getaddrinfo(
                None, self.__state.our_port):
            try:
                self._sock_input = socket.socket(family, socktype, proto)
                self._sock_input.bind(sockaddr)
                return self._sock_input
            except OSError as e:
                print("falling over to next possible address:", e)
        print("failed to bind socket for listening")
        sys.exit(1)

    def _init_sender(self):
        for family, socktype, proto, _, sockaddr in self.__getaddrinfo(
                self._board_ip, str(self._board_port)):
            try:
                self._sock_output = socket.socket(family, socktype, proto)
                self._board_address = sockaddr
                return self._sock_output
            except OSError as e:
                print("falling over to next possible address:", e)
        print("failed to bind socket for listening")
        sys.exit(1)

    _UNBOOTED = struct.Struct("!I")

    def input_thread(self):
        sdp_header_len = 26
        while True:
            try:
                message, (host, port) = self._sock_input.recvfrom(MTU)
            except OSError:
                return

            buf = memoryview(message)
            num_additional_bytes = len(buf) - sdp_header_len
            if self._UNBOOTED.unpack_from(buf, 2)[0] == SPINN_HELLO:
                continue

            if not self.is_board_address_set():
                self._board_port = port
                self.set_board_ip_address(host)
                self._init_sender()
                print(f"packet received from {host} on port {port}")
            elif not self.is_board_port_set():
                self._board_port = port
                self._init_sender()
                print(f"packet received from {host} on port {port}")

            nowtime = timestamp()
            if self.__state.firstreceivetime == 0:
                self.__state.firstreceivetime = nowtime

            if not self.__state.freezedisplay:
                self._update_history_data(nowtime)

            timeperindex = TIMEWINDOW / float(self.__state.plotwidth)
            updateline = int((nowtime - self.__state.starttime) / timeperindex
                             / 1000000) % self.__state.history_size
            self._process_heatmap_packet(buf, num_additional_bytes, updateline)

    def _update_history_data(self, updateline):
        linestoclear = updateline - self._last_history_line_updated
        if linestoclear < 0:
            if updateline + 500 > self._last_history_line_updated:
                linestoclear = 0
            else:
                linestoclear = (
                    updateline + self.__state.history_size -
                    self._last_history_line_updated)

        num_pts = self.__state.xdim * self.__state.ydim
        for i in range(linestoclear):
            rowid = (1 + i + self._last_history_line_updated) \
                % self.__state.history_size
            for j in range(num_pts):
                self.__state.history_data[rowid][j] = NOTDEFINED

        self._last_history_line_updated = updateline

    def _process_heatmap_packet(self, buf, n_bytes, updateline):
        if self.__state.freezedisplay:
            return

        # take the chip ID and works out the chip X,Y coords
        src_addr, = self._HALF.unpack_from(buf, 6)
        xsrc, ysrc = divmod(src_addr, 256)

        # for all extra data (assuming regular array of 4 byte words)
        for i in range(n_bytes // 4):
            arrayindex = (self.__state.x_chips * self.__state.y_chips) \
                * (xsrc * self.__state.x_chips + ysrc) + i
            if arrayindex > self.__state.xdim * self.__state.ydim:
                continue
            data = self._WORD.unpack_from(buf, 26 + i * 4)[0] \
                * self.__state.fixed_point_factor
            self.__state.immediate_data[arrayindex] = data
            self.__state.history_data[updateline][arrayindex] = data
            self.__state.somethingtoplot = True

    def _send(self, dest_addr, dest_port, command, arg1, arg2, arg3, *args):
        # FIXME!
        ip_time_out = 0
        flags = 7
        tag = 255
        src_port = 0xFF
        src_addr = 0
        seq = 0

        msg = self._SCP_HEADER.pack(
            ip_time_out, flags, tag, dest_port, src_port, 0, 0,
            command, seq, arg1, arg2, arg3)
        self._DEST.pack_into(msg, 6, dest_addr, src_addr)
        for a in args:
            msg += self._WORD.pack(a)

        if self.is_board_address_set():
            self._sock_output.sendto(msg, self._board_address)
        self.__state.pktgone = timestamp()


class HeatProtocol(SDP):
    """
    The Heatmap protocol, used for controlling what chips are doing
    """

    _PROTOCOL_ID = 0x21

    def __init__(self, state):
        SDP.__init__(self, state)
        self.__state = state

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
