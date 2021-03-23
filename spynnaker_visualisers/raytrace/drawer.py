import socket
import struct
import sys
import threading
import numpy
import spynnaker_visualisers.opengl_support as gl
import spynnaker_visualisers.glut_framework as glut


class RaytraceDrawer(glut.GlutFramework):
    moveAmount = 0.00003
    turnAmount = 0.0000003

    verticalFieldOfView = 50.0
    horizontalFieldOfView = 60.0

    INPUT_PORT_SPINNAKER = 17894
    SDP_HEADER = struct.Struct("<HBBBBHHHHIII")
    PIXEL_FORMAT = struct.Struct(">HHBBB")
    RECV_BUFFER_SIZE = 1500  # Ethernet MTU

    def __init__(self, size=256):
        super().__init__()
        self.moving = 0
        self.strafing = 0
        self.turningLeftRight = 0
        self.turningUpDown = 0
        self.rolling = 0
        self.position = numpy.array([-220.0, 50.0, 0.0])
        self.look = numpy.array([1.0, 0.0, 0.0])
        self.up = numpy.array([0.0, 1.0, 0.0])
        self.frameHeight = size
        self.frameWidth = int(
            self.horizontalFieldOfView * self.frameHeight
            / self.verticalFieldOfView)
        self.viewingFrame = numpy.zeros(
            self.frameWidth * self.frameHeight * 3, dtype=numpy.uint8)
        self.receivedFrame = numpy.zeros(
            self.frameWidth * self.frameHeight, dtype=numpy.uint32)
        self._init_udp_server_spinnaker()

    def start(self, args):
        threading.Thread(target=self._input_thread, daemon=True).start()
        self.start_framework(
            args, "Path Tracer", self.frameWidth, self.frameHeight, 0, 0, 10,
            display_mode=glut.displayModeDouble)

    def init(self):
        gl.enable(gl.blend, gl.depth_test)
        gl.blend_function(gl.src_alpha, gl.one_minus_src_alpha)

    def display(self, dTime):
        gl.clear_color(1.0, 1.0, 1.0, 0.001)
        gl.clear(gl.color_buffer_bit | gl.depth_buffer_bit)
        gl.draw_pixels(
            self.windowWidth, self.windowHeight, gl.rgb, gl.unsigned_byte,
            self.viewingFrame.data)

    def reshape(self, width, height):
        self.windowWidth = min((width, self.frameWidth))
        self.windowHeight = min((height, self.frameHeight))
        gl.viewport(0, 0, width, height)
        gl.load_identity()

    def special_keyboard_down(self, key, x, y):  # @UnusedVariable
        if key == glut.keyUp:
            self.turningUpDown = -1
        elif key == glut.keyDown:
            self.turningUpDown = 1
        elif key == glut.keyRight:
            self.rolling = -1
        elif key == glut.keyLeft:
            self.rolling = 1

    def special_keyboard_up(self, key, x, y):  # @UnusedVariable
        if key == glut.keyUp or key == glut.keyDown:
            self.turningUpDown = 0
        elif key == glut.keyLeft or key == glut.keyRight:
            self.rolling = 0

    def keyboard_down(self, key, x, y):  # @UnusedVariable
        if key == 'w':
            self.moving = 1
        elif key == 's':
            self.moving = -1
        elif key == 'a':
            self.turningLeftRight = -1
        elif key == 'd':
            self.turningLeftRight = 1
        elif key == 'q':
            self.strafing = 1
        elif key == 'e':
            self.strafing = -1
        elif key == '\x1b':  # Escape
            sys.exit()

    def keyboard_up(self, key, x, y):  # @UnusedVariable
        if key == 'w' or key == 's':
            self.moving = 0
        elif key == 'a' or key == 'd':
            self.turningLeftRight = 0
        elif key == 'q' or key == 'e':
            self.strafing = 0

    @staticmethod
    def vector_rotate(rotated, rotateAbout, theta):
        """Rotate the first vector around the second"""
        # https://gist.github.com/fasiha/6c331b158d4c40509bd180c5e64f7924
        par = (numpy.dot(rotated, rotateAbout) /
               numpy.dot(rotateAbout, rotateAbout) * rotateAbout)
        perp = rotated - par
        w = numpy.cross(rotateAbout, perp)
        w = w / numpy.linalg.norm(w)
        result = (par + perp * numpy.cos(theta) +
                  numpy.linalg.norm(perp) * w * numpy.sin(theta))
        return result / numpy.linalg.norm(result)

    def calculate_movement(self, timestep):
        # Forward movement
        if self.moving:
            self.position += self.look * (
                timestep * self.moveAmount * self.moving)
        right = numpy.cross(self.up, self.look)
        # Strafing movement
        if self.strafing:
            self.position += right * (
                timestep * self.moveAmount * self.strafing)
        # To turn left/right, rotate the look vector around the up vector
        if self.turningLeftRight:
            self.look = self.vector_rotate(
                self.look, self.up,
                timestep * self.turnAmount * self.turningLeftRight)
        # To turn up/down, rotate the look vector and up vector about the right
        # vector
        if self.turningUpDown:
            self.look = self.vector_rotate(
                self.look, right,
                timestep * self.turnAmount * self.turningUpDown)
            self.up = self.vector_rotate(
                self.up, right,
                timestep * self.turnAmount * self.turningUpDown)
        # To roll, rotate the up vector around the look vector
        if self.rolling:
            self.up = self.vector_rotate(
                self.up, self.look, timestep *
                self.turnAmount * self.rolling)

    def run(self):
        """Calculate movement ten times a second"""
        super().run()
        self.calculate_movement(self.frame_time_elapsed * 1000)

    def _init_udp_server_spinnaker(self):
        """initialization of the port for receiving SpiNNaker frames"""
        self._sockfd_input = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sockfd_input.bind(('0.0.0.0', self.INPUT_PORT_SPINNAKER))

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

    @staticmethod
    def _pixelinfo(data, number_of_pixels):
        for i in range(number_of_pixels):
            yield self.PIXEL_FORMAT.unpack_from(
                data, i * self.PIXEL_FORMAT.size)

    def process_one_pixel(self, x, y, r, g, b):
        index = (self.frameHeight - y - 1) * self.frameWidth + x
        if index < self.frameWidth * self.frameHeight:
            ix3 = index * 3
            count = self.receivedFrame[index]
            cp1 = count + 1
            self.viewingFrame[ix3] = (
                (r + count * self.viewingFrame[ix3]) // cp1)
            self.viewingFrame[ix3 + 1] = (
                (g + count * self.viewingFrame[ix3 + 1]) // cp1)
            self.viewingFrame[ix3 + 2] = (
                (b + count * self.viewingFrame[ix3 + 2]) // cp1)
            self.receivedFrame[index] += 1


def main(args):
    drawer = RaytraceDrawer()
    drawer.start(args)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
