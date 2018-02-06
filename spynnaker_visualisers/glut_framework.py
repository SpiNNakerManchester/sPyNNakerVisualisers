'''
The MIT License

Copyright (c) 2010 Paul Solt, PaulSolt@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

@author: Converted to Python by Donal Fellows
'''

import OpenGL.GLUT as GLUT  # pylint: disable=import-error
from datetime import datetime
from spinn_utilities.abstract_base import AbstractBase, abstractmethod
from six import add_metaclass
import traceback
from spynnaker_visualisers.opengl_support import viewport, save_matrix, \
    enable, blend, line_smooth, disable, line_width, blend_function, \
    src_alpha, one_minus_src_alpha, rotate, scale, translate, raster_position


class _PerformanceTimer(object):
    __slots__ = [
        "_stamp_1", "_stamp_2", "_stopped"]

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


@add_metaclass(AbstractBase)
class GlutFramework(object):
    '''Base for code that wants to visualise using an OpenGL surface.
    '''
    __slots__ = [
        "displayTimer",
        "elapsedTimeInSeconds",
        "frameRateTimer",
        "frameTime",
        "frameTimeElapsed",
        "__loggederrors",
        "window"]

    def __init__(self):
        self.window = None
        self.frameTimeElapsed = 0.0
        self.frameTime = 0.0
        self.frameRateTimer = _PerformanceTimer()
        self.displayTimer = _PerformanceTimer()
        self.elapsedTimeInSeconds = 0.0
        self.__loggederrors = set()

    def startFramework(self, args, title, width, height, posx, posy, fps):
        """startFramework will initialize framework and start the Glut run\
        loop. It must be called after the GlutFramework class is created to\
        start the application."""
        # pylint: disable=too-many-arguments

        # Sets the instance to this, used in the callback wrapper functions
        self.frameTime = 1.0 / fps * 1000.0

        # Initialize GLUT
        GLUT.glutInit(args)
        GLUT.glutInitDisplayMode(GLUT.GLUT_RGB | GLUT.GLUT_DOUBLE)
        GLUT.glutInitWindowSize(width, height)
        GLUT.glutInitWindowPosition(posx, posy)
        self.window = GLUT.glutCreateWindow(title)

        self.init()  # Initialize

        # Function callbacks with wrapper functions
        GLUT.glutDisplayFunc(self.__displayFramework)
        GLUT.glutReshapeFunc(self.__reshapeFramework)
        GLUT.glutIdleFunc(self.__run)
        GLUT.glutMouseFunc(self.__mouseButtonPress)
        GLUT.glutMotionFunc(self.__mouseMove)
        GLUT.glutKeyboardFunc(self.__keyboardDown)
        GLUT.glutKeyboardUpFunc(self.__keyboardUp)
        GLUT.glutSpecialFunc(self.__specialKeyboardDown)
        GLUT.glutSpecialUpFunc(self.__specialKeyboardUp)

        GLUT.glutMainLoop()

    def init(self):
        """Initialises GLUT and registers any extra callback functions."""
        pass

    @abstractmethod
    def display(self, dTime):
        """The display function is called at a specified frames-per-second\
        (FPS). Any animation drawing code can be run in the display method.

        :param dTime: the change in time (seconds)
        """

    def reshape(self, width, height):
        """Called when the window dimensions change.

        :param width: the width of the window in pixels
        :param height: the height of the window in pixels
        """
        viewport(0, 0, width, height)

    def mouseButtonPress(self, button, state, x, y):
        """Called when the mouse buttons are pressed.

        :param button: the mouse buttons
        :param state: the state of the buttons
        :param x: the x coordinate
        :param y: the y coordinate
        """
        pass

    def mouseMove(self, x, y):
        """Called when the mouse moves on the screen.

        :param x: the x coordinate
        :param y: the y coordinate
        """
        pass

    def keyboardDown(self, key, x, y):
        """The keyboard function is called when a standard key is pressed\
        down.

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """
        pass

    def keyboardUp(self, key, x, y):
        """The keyboard function is called when a standard key is "unpressed".

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """
        pass

    def specialKeyboardDown(self, key, x, y):
        """The keyboard function is called when a special key is pressed down\
        (F1 keys, Home, Inser, Delete, Page Up/Down, End, arrow keys).\
        http://www.opengl.org/resources/libraries/glut/spec3/node54.html

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """
        pass

    def specialKeyboardUp(self, key, x, y):
        """The keyboard function is called when a special key is "unpressed"\
        (F1 keys, Home, Inser, Delete, Page Up/Down, End, arrow keys).

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """
        pass

    def run(self):
        """The run method is called by GLUT and contains the logic to set the\
        frame rate of the application.
        """
        if self.frameRateTimer.stopped:
            self.frameRateTimer.start()

        # stop the timer and calculate time since last frame
        self.frameRateTimer.stop()
        milliseconds = self.frameRateTimer.elapsedMilliseconds
        self.frameTimeElapsed += milliseconds

        if self.frameTimeElapsed >= self.frameTime:
            # If the time exceeds a certain "frame rate" then show the next
            # frame
            GLUT.glutPostRedisplay()

            # remove a "frame" and start counting up again
            self.frameTimeElapsed -= self.frameTime
        self.frameRateTimer.start()

    def displayFramework(self):
        """The displayFramework() function sets up initial GLUT state and\
        calculates the change in time between each frame. It calls the\
        display(float) function which can be subclassed.
        """
        if self.displayTimer.stopped:
            self.displayTimer.start()
        self.displayTimer.stop()
        elapsedTimeInSeconds = self.displayTimer.elapsedSeconds
        if GLUT.glutGetWindow() == self.window:
            self.display(elapsedTimeInSeconds)
            GLUT.glutSwapBuffers()
        self.displayTimer.start()

    def reshapeFramework(self, width, height):
        if GLUT.glutGetWindow() == self.window:
            self.reshape(width, height)

    def write_large(self, x, y, string, *args):
        """Utility function: write a string to a given location as a bitmap.
        """
        if args:
            string = string % args
        raster_position(x, y)
        for ch in string:
            GLUT.glutBitmapCharacter(GLUT.GLUT_BITMAP_TIMES_ROMAN_24, ord(ch))

    def write_small(self, x, y, size, rotation, string, *args):
        """Utility function: write a string to a given location as a strokes.
        """
        # pylint: disable=too-many-arguments
        if args:
            string = string % args

        with save_matrix():
            # antialias the font
            enable(blend, line_smooth)
            blend_function(src_alpha, one_minus_src_alpha)
            line_width(1.5)

            translate(x, y, 0.0)
            scale(size, size, size)
            rotate(rotation, 0.0, 0.0, 1.0)
            for ch in string:
                GLUT.glutStrokeCharacter(GLUT.GLUT_STROKE_ROMAN, ord(ch))
            disable(blend, line_smooth)

    def __displayFramework(self):
        try:
            return self.displayFramework()
        except Exception:
            self.__logerror()

    def __reshapeFramework(self, width, height):
        try:
            return self.reshapeFramework(width, height)
        except Exception:
            self.__logerror()

    def __run(self):
        try:
            return self.run()
        except Exception:
            self.__logerror()

    def __mouseButtonPress(self, button, state, x, y):
        try:
            return self.mouseButtonPress(button, state, x, y)
        except Exception:
            self.__logerror()

    def __mouseMove(self, x, y):
        try:
            return self.mouseMove(x, y)
        except Exception:
            self.__logerror()

    def __keyboardDown(self, key, x, y):
        try:
            return self.keyboardDown(key, x, y)
        except Exception:
            self.__logerror()

    def __keyboardUp(self, key, x, y):
        try:
            return self.keyboardUp(key, x, y)
        except Exception:
            self.__logerror()

    def __specialKeyboardDown(self, key, x, y):
        try:
            return self.specialKeyboardDown(key, x, y)
        except Exception:
            self.__logerror()

    def __specialKeyboardUp(self, key, x, y):
        try:
            return self.specialKeyboardUp(key, x, y)
        except Exception:
            self.__logerror()

    def __logerror(self):
        tb = traceback.format_exc()
        if tb not in self.__loggederrors:
            self.__loggederrors.add(tb)
            traceback.print_exc()
