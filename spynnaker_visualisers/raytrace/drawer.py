from datetime import datetime
from OpenGL.GL import \
    glEnable, glDrawPixels, glClear, glClearColor, glViewport, \
    glLoadIdentity, glBlendFunc
from OpenGL.GL import \
    GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_RGB, GL_UNSIGNED_BYTE, \
    GL_BLEND, GL_SRC_ALPHA, GL_DEPTH_TEST, GL_ONE_MINUS_SRC_ALPHA
from OpenGL.GLUT import \
    glutInit, glutInitDisplayMode, glutInitWindowPosition, \
    glutInitWindowSize, glutCreateWindow, glutSwapBuffers, glutDisplayFunc, \
    glutReshapeFunc, glutIdleFunc, glutMainLoop, glutKeyboardFunc, \
    glutKeyboardUpFunc, glutSpecialFunc, glutSpecialUpFunc
from OpenGL.GLUT import \
    GLUT_KEY_UP, GLUT_KEY_DOWN, GLUT_KEY_LEFT, GLUT_KEY_RIGHT, GLUT_DOUBLE
import numpy
import socket
import struct
import sys
import threading

position = numpy.array([-220.0, 50.0, 0.0])
look = numpy.array([1.0, 0.0, 0.0])
up = numpy.array([0.0, 1.0, 0.0])

moving = 0
strafing = 0
turningLeftRight = 0
turningUpDown = 0
rolling = 0

moveAmount = 0.00003
turnAmount = 0.0000003

verticalFieldOfView = 50.0
horizontalFieldOfView = 60.0

INPUT_PORT_SPINNAKER = 17894
SDP_HEADER = struct.Struct("<HBBBBHHHHIII")
PIXEL_FORMAT = struct.Struct(">HHBBB")


class _PerformanceTimer(object):
    @staticmethod
    def _now():
        return datetime.now()

    def __init__(self):
        self._stopped = True
        self._stamp_1 = 0
        self._stamp_2 = 0

    def start(self):
        self._stopped = False
        self._stamp_1 = _PerformanceTimer._now()

    def stop(self):
        self._stamp_2 = _PerformanceTimer._now()
        self._stopped = True

    def resume(self):
        self._stopped = False

    @property
    def stopped(self):
        return self._stopped

    @property
    def elapsedMilliseconds(self):
        delta = self._stamp_2 - self._stamp_1
        return float(delta.seconds) * 1000 + float(delta.microseconds) / 1000

    @property
    def elapsedSeconds(self):
        delta = self._stamp_2 - self._stamp_1
        return float(delta.seconds) + float(delta.microseconds) / 1000000


timer = _PerformanceTimer()


def display():
    """Called every time OpenGL needs to update the display"""
    global windowWidth, windowHeight, viewingFrame
    glClearColor(1.0, 1.0, 1.0, 0.001)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glDrawPixels(windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE,
                 viewingFrame.data)
    glutSwapBuffers()


def reshape(width, height):
    global frameWidth, frameHeight, windowWidth, windowHeight
    windowWidth = min((width, frameWidth))
    windowHeight = min((height, frameHeight))
    glViewport(0, 0, width, height)
    glLoadIdentity()


def specialDown(key, x, y):  # @UnusedVariable
    global turningUpDown, rolling
    if key == GLUT_KEY_UP:
        turningUpDown = -1
    elif key == GLUT_KEY_DOWN:
        turningUpDown = 1
    elif key == GLUT_KEY_RIGHT:
        rolling = -1
    elif key == GLUT_KEY_LEFT:
        rolling = 1


def specialUp(key, x, y):  # @UnusedVariable
    global turningUpDown, rolling
    if key == GLUT_KEY_UP or key == GLUT_KEY_DOWN:
        turningUpDown = 0
    elif key == GLUT_KEY_RIGHT or key == GLUT_KEY_LEFT:
        rolling = 0


def keyDown(key, x, y):  # @UnusedVariable
    global moving, turningLeftRight, strafing
    if key == 'w':
        moving = 1
    elif key == 's':
        moving = -1
    elif key == 'a':
        turningLeftRight = -1
    elif key == 'd':
        turningLeftRight = 1
    elif key == 'q':
        strafing = 1
    elif key == 'e':
        strafing = -1


def keyUp(key, x, y):  # @UnusedVariable
    global moving, turningLeftRight, strafing
    if key == 'w' or key == 's':
        moving = 0
    elif key == 'a' or key == 'd':
        turningLeftRight = 0
    elif key == 'q' or key == 'e':
        strafing = 0


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


def calculate_movement(timestep):
    global position, look, up, moveAmount
    global moving, strafing, turningLeftRight, turningUpDown, rolling
    # Forward movement
    if moving != 0:
        position += look * (timestep * moveAmount * moving)
    right = numpy.cross(up, look)
    # Strafing movement
    if strafing != 0:
        position += right * (timestep * moveAmount * strafing)
    # To turn left/right, rotate the look vector around the up vector
    if turningLeftRight != 0:
        look = vector_rotate(
            look, up, timestep * turnAmount * turningLeftRight)
    # To turn up/down, rotate the look vector and up vector about the right
    # vector
    if turningUpDown != 0:
        look = vector_rotate(
            look, right, timestep * turnAmount * turningUpDown)
        up = vector_rotate(up, right, timestep * turnAmount * turningUpDown)
    # To roll, rotate the up vector around the look vector
    if rolling != 0:
        up = vector_rotate(up, look, timestep * turnAmount * rolling)


def idle():
    """Calculate movement ten times a second"""
    timer.stop()
    elapsed = timer.elapsedMilliseconds
    if elapsed > 100:
        timer.start()
        calculate_movement(elapsed * 1000)
        display()
    else:
        timer.resume()


def init_udp_server_spinnaker():
    """initialization of the port for receiving SpiNNaker frames"""
    global sockfd_input
    sockfd_input = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sockfd_input.bind(('0.0.0.0', INPUT_PORT_SPINNAKER))


def input_thread():
    global sockfd_input
    print "Drawer running (listening port: %d)..." % (INPUT_PORT_SPINNAKER)
    while True:
        msg = sockfd_input.recv(65536)
        sdp_msg = SDP_HEADER.unpack_from(msg)
        data = msg[SDP_HEADER.size:]               # sdp_msg.data
        if sdp_msg[7] == 3:                        # sdp_msg.command
            process_one_message(data, sdp_msg[9])  # sdp_msg.arg1


def process_one_message(data, number_of_pixels):
    global frameHeight, frameWidth, receivedFrame, viewingFrame
    for i in xrange(number_of_pixels):
        x, y, r, g, b = PIXEL_FORMAT.unpack_from(data, i * PIXEL_FORMAT.size)
        index = (frameHeight - y - 1) * frameWidth + x
        if index < frameWidth * frameHeight:
            num_received_for_pixel = receivedFrame[index]
            viewingFrame[index * 3] = (
                (r + num_received_for_pixel * viewingFrame[index * 3]) /
                (num_received_for_pixel + 1))
            viewingFrame[index * 3 + 1] = (
                (g + num_received_for_pixel * viewingFrame[index * 3 + 1]) /
                (num_received_for_pixel + 1))
            viewingFrame[index * 3 + 2] = (
                (b + num_received_for_pixel * viewingFrame[index * 3 + 2]) /
                (num_received_for_pixel + 1))
            receivedFrame[index] += 1


def main():
    global horizontalFieldOfView, verticalFieldOfView
    global frameHeight, frameWidth, prevTime, viewingFrame, receivedFrame

    init_udp_server_spinnaker()

    frameHeight = 256                            # Gotta be something!
    frameWidth = int(horizontalFieldOfView * frameHeight / verticalFieldOfView)

    timer.start()
    viewingFrame = numpy.zeros(frameWidth * frameHeight * 3, dtype=numpy.uint8)
    receivedFrame = numpy.zeros(frameWidth * frameHeight, dtype=numpy.uint32)

    threading.Thread(target=input_thread).start()

    glutInit(sys.argv)                           # Initialise OpenGL
    glutInitDisplayMode(GLUT_DOUBLE)             # Set the display mode
    glutInitWindowSize(frameWidth, frameHeight)  # Set the window size
    glutInitWindowPosition(0, 0)                 # Set the window position
    glutCreateWindow("Path Tracer")              # Create the window

    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    glEnable(GL_DEPTH_TEST)

    glutDisplayFunc(display)
    glutReshapeFunc(reshape)
    glutSpecialFunc(specialDown)
    glutSpecialUpFunc(specialUp)
    glutKeyboardFunc(keyDown)
    glutKeyboardUpFunc(keyUp)
    glutIdleFunc(idle)

    glutMainLoop()                               # Enter the main OpenGL loop

    return 0


if __name__ == "__main__":
    sys.exit(main())
