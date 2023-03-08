# Copyright (c) 2017 The University of Manchester
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import socket
import struct
import sys
import threading
from numpy import dot, cross, array, zeros, cos, sin, uint8, uint32
from numpy.linalg import norm
import spynnaker_visualisers.opengl_support as gl
import spynnaker_visualisers.glut_framework as glut


class RaytraceDrawer(glut.GlutFramework):
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
            display_mode=glut.displayModeDouble)

    def init(self):
        gl.enable(gl.blend, gl.depth_test)
        gl.blend_function(gl.src_alpha, gl.one_minus_src_alpha)

    def display(self, dTime):
        gl.clear_color(1.0, 1.0, 1.0, 0.001)
        # pylint: disable=unsupported-binary-operation
        gl.clear(gl.color_buffer_bit | gl.depth_buffer_bit)
        gl.draw_pixels(
            self._win_width, self._win_height, gl.rgb, gl.unsigned_byte,
            self._viewing_frame.data)

    def reshape(self, width, height):
        self._win_width = min(width, self._width)
        self._win_height = min(height, self._height)
        gl.viewport(0, 0, width, height)
        gl.load_identity()

    def special_keyboard_down(self, key, x, y):  # @UnusedVariable
        if key == glut.keyUp:
            self._turn_down = -1
        elif key == glut.keyDown:
            self._turn_down = 1
        elif key == glut.keyRight:
            self._rolling = -1
        elif key == glut.keyLeft:
            self._rolling = 1

    def special_keyboard_up(self, key, x, y):  # @UnusedVariable
        if key == glut.keyUp or key == glut.keyDown:
            self._turn_down = 0
        elif key == glut.keyLeft or key == glut.keyRight:
            self._rolling = 0

    def keyboard_down(self, key, x, y):  # @UnusedVariable
        if key == 'w':
            self._moving = 1
        elif key == 's':
            self._moving = -1
        elif key == 'a':
            self._turn_right = -1
        elif key == 'd':
            self._turn_right = 1
        elif key == 'q':
            self._strafing = 1
        elif key == 'e':
            self._strafing = -1
        elif key == '\x1b':  # Escape
            sys.exit()

    def keyboard_up(self, key, x, y):  # @UnusedVariable
        if key == 'w' or key == 's':
            self._moving = 0
        elif key == 'a' or key == 'd':
            self._turn_right = 0
        elif key == 'q' or key == 'e':
            self._strafing = 0

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
            if sdp_msg[7] == 3:  # sdp_msg.command
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


def main(args):
    drawer = RaytraceDrawer()
    drawer.start(args)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
