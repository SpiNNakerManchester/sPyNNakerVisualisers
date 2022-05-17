# Copyright (c) 2018-2021 The University of Manchester
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
import threading
from numpy import dot, cross, array, zeros, cos, sin, uint8, uint32
from numpy.linalg import norm
from spynnaker_visualisers.opengl import (
    GlutFramework, key_press_handler, key_release_handler,
    key_up, key_down, key_left, key_right,
    display_mode_double, blend, depth_test, src_alpha, one_minus_src_alpha,
    color_buffer_bit, depth_buffer_bit, rgb, unsigned_byte,
    blend_function, enable, clear_color, clear, draw_pixels,
    viewport, load_identity)


class RaytraceDrawer(GlutFramework):
    __slots__ = (
        "_moving", "_strafing", "_turn_down", "_turn_right", "_rolling",
        "_height", "_width", "_win_height", "_win_width",
        "_viewing_frame", "_received_frame", "_sockfd_input",
        "_look", "_up", "_position")
    moveAmount = 0.00003
    turnAmount = 0.0000003

    # Fields of view
    VERT_FOV = 50.0
    HORIZ_FOV = 60.0

    INPUT_PORT_SPINNAKER = 17894
    SDP_HEADER = struct.Struct("<HBBBBHHHHIII")
    PIXEL_FORMAT = struct.Struct(">HHBBB")
    RECV_BUFFER_SIZE = 1500  # Ethernet MTU; SpiNNaker doesn't jumbo
    PIXEL_DATA_CMD = 3

    def __init__(self, size=256):
        super().__init__()
        self._moving = 0
        self._strafing = 0
        # Turn left is negative
        self._turn_right = 0
        # Turn up is negative
        self._turn_down = 0
        self._rolling = 0
        self._position = array([-220.0, 50.0, 0.0])
        self._look = array([1.0, 0.0, 0.0])
        self._up = array([0.0, 1.0, 0.0])
        self._height = size
        self._width = int(self.HORIZ_FOV * self._height / self.VERT_FOV)
        self._win_height = self._height
        self._win_width = self._width
        self._viewing_frame = zeros(
            self._width * self._height * 3, dtype=uint8)
        self._received_frame = zeros(
            self._width * self._height, dtype=uint32)
        self._sockfd_input = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sockfd_input.bind(('0.0.0.0', self.INPUT_PORT_SPINNAKER))

    def start(self, args):
        threading.Thread(target=self._input_thread, daemon=True).start()
        self.start_framework(
            args, "Path Tracer", self._width, self._height, 0, 0, 10,
            display_mode=display_mode_double)

    def init(self):
        enable(blend, depth_test)
        blend_function(src_alpha, one_minus_src_alpha)

    def display(self, dTime):
        clear_color(1.0, 1.0, 1.0, 0.001)
        clear(color_buffer_bit | depth_buffer_bit)
        draw_pixels(
            self._win_width, self._win_height, rgb, unsigned_byte,
            self._viewing_frame.data)

    def reshape(self, width, height):
        self._win_width = min(width, self._width)
        self._win_height = min(height, self._height)
        viewport(0, 0, width, height)
        load_identity()

    @key_press_handler(key_up)
    def _look_up(self):
        self._turn_down = -1

    @key_press_handler(key_down)
    def _look_down(self):
        self._turn_down = 1

    @key_release_handler(key_up, key_down)
    def _not_up_down(self):
        self._turn_down = 0

    @key_press_handler(key_left)
    def _roll_left(self):
        self._turn_down = -1

    @key_press_handler(key_right)
    def _roll_right(self):
        self._turn_down = 1

    @key_release_handler(key_left, key_right)
    def _not_roll(self):
        self._turn_down = 0

    @key_press_handler('w')
    def _go_forward(self):
        self._moving = 1

    @key_press_handler('s')
    def _go_backward(self):
        self._moving = -1

    @key_release_handler('w', 's')
    def _not_forward_backward(self):
        self._moving = 0

    @key_press_handler('a')
    def _left(self):
        self._turn_right = -1

    @key_press_handler('d')
    def _right(self):
        self._turn_right = 1

    @key_release_handler('a', 'd')
    def _not_left_right(self):
        self._turn_right = 0

    @key_press_handler('q')
    def _strafe_left(self):
        self._strafing = 1

    @key_press_handler('e')
    def _strafe_right(self):
        self._strafing = -1

    @key_release_handler('q', 'e')
    def _not_strafe(self):
        self._strafing = 0

    @key_press_handler('\x1b')  # Escape
    def _done(self):
        sys.exit()

    @staticmethod
    def vector_rotate(rotated, axis, theta):
        """Rotate the first vector around the second"""
        # https://gist.github.com/fasiha/6c331b158d4c40509bd180c5e64f7924
        par = (dot(rotated, axis) / dot(axis, axis) * axis)
        perp = rotated - par
        w = cross(axis, perp)
        w = w / norm(w)
        result = par + perp * cos(theta) + norm(perp) * w * sin(theta)
        return result / norm(result)

    def calculate_movement(self, dt):
        # Forward movement
        if self._moving:
            self._position += self._look * dt * self.moveAmount * self._moving
        right = cross(self._up, self._look)
        # Strafing movement
        if self._strafing:
            self._position += right * dt * self.moveAmount * self._strafing
        # To turn left/right, rotate the look vector around the up vector
        if self._turn_right:
            self._look = self.vector_rotate(
                self._look, self._up, dt * self.turnAmount * self._turn_right)
        # To turn up/down, rotate the look vector and up vector about the right
        # vector
        if self._turn_down:
            self._look = self.vector_rotate(
                self._look, right, dt * self.turnAmount * self._turn_down)
            self._up = self.vector_rotate(
                self._up, right, dt * self.turnAmount * self._turn_down)
        # To roll, rotate the up vector around the look vector
        if self._rolling:
            self._up = self.vector_rotate(
                self._up, self._look, dt * self.turnAmount * self._rolling)

    def run(self):
        """Calculate movement ten times a second"""
        super().run()
        self.calculate_movement(self.frame_time_elapsed * 1000)

    def _input_thread(self):
        print(
            f"Drawer running (listening port: {self.INPUT_PORT_SPINNAKER})...")
        while True:
            msg = self._sockfd_input.recv(self.RECV_BUFFER_SIZE)
            sdp_msg = self.SDP_HEADER.unpack_from(msg)
            data = msg[self.SDP_HEADER.size:]  # sdp_msg.data
            if sdp_msg[7] == self.PIXEL_DATA_CMD:  # sdp_msg.command
                for pixel_datum in self._pixelinfo(
                        data, sdp_msg[9]):  # sdp_msg.arg1
                    self.process_one_pixel(*pixel_datum)

    @classmethod
    def _pixelinfo(cls, data, number_of_pixels):
        for i in range(number_of_pixels):
            yield cls.PIXEL_FORMAT.unpack_from(
                data, i * cls.PIXEL_FORMAT.size)

    def process_one_pixel(self, x, y, r, g, b):
        index = (self._height - y - 1) * self._width + x
        if index < self._width * self._height:
            ix3 = index * 3
            count = self._received_frame[index]
            cp1 = count + 1
            self._viewing_frame[ix3] = (
                (r + count * self._viewing_frame[ix3]) // cp1)
            self._viewing_frame[ix3 + 1] = (
                (g + count * self._viewing_frame[ix3 + 1]) // cp1)
            self._viewing_frame[ix3 + 2] = (
                (b + count * self._viewing_frame[ix3 + 2]) // cp1)
            self._received_frame[index] += 1

    @staticmethod
    def run_application(args):
        drawer = RaytraceDrawer()
        drawer.start(args)
        return 0


if __name__ == "__main__":
    sys.exit(RaytraceDrawer.run_application(sys.argv))
