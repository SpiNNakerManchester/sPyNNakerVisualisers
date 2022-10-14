"""
A basic framework that implements the basic system handling for a GUI
application that uses OpenGL and GLUT to do the GUI work.
"""
# The MIT License
#
# Copyright (c) 2010 Paul Solt, PaulSolt@gmail.com
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# @author: Converted to Python by Donal Fellows

from datetime import datetime
from enum import Enum
import os
import traceback
import OpenGL.error
from spinn_utilities.abstract_base import AbstractBase, abstractmethod
from .opengl_support import (
    viewport, save_matrix, enable, blend, line_smooth, disable, line_width,
    blend_function, src_alpha, one_minus_src_alpha, rotate, scale, translate,
    raster_position)
try:
    # this fails in <=2020 versions of Python on OS X 11.x
    import OpenGL.GLUT  # noqa: F401
except ImportError:
    # Hack for macOS Big Sur
    from ._bigsurhack import patch_ctypes
    patch_ctypes()
import OpenGL.GLUT as GLUT

key_up = GLUT.GLUT_KEY_UP
key_down = GLUT.GLUT_KEY_DOWN
key_left = GLUT.GLUT_KEY_LEFT
key_right = GLUT.GLUT_KEY_RIGHT
display_mode_double = GLUT.GLUT_DOUBLE
left_button = GLUT.GLUT_LEFT_BUTTON
right_button = GLUT.GLUT_RIGHT_BUTTON
scroll_up = 3
scroll_down = 4
mouse_down = GLUT.GLUT_DOWN
mouse_up = GLUT.GLUT_UP


class Font(Enum):
    # pylint: disable=no-member
    Bitmap8x13 = GLUT.GLUT_BITMAP_8_BY_13
    Helvetica12 = GLUT.GLUT_BITMAP_HELVETICA_12
    Helvetica18 = GLUT.GLUT_BITMAP_HELVETICA_18
    TimesRoman24 = GLUT.GLUT_BITMAP_TIMES_ROMAN_24


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
        """ Start the timer. """
        self._stopped = False
        self._stamp_1 = _PerformanceTimer._now()

    def stop(self):
        """ Stop the timer. """
        self._stamp_2 = _PerformanceTimer._now()
        self._stopped = True

    @property
    def stopped(self):
        """ Is the timer stopped? """
        return self._stopped

    @property
    def elapsed_milliseconds(self):
        """ How long elapsed in the last timing run? In milliseconds.

        ..note::
            Only valid when the timer has previously been run and is currently\
            stopped.
        """
        delta = self._stamp_2 - self._stamp_1
        return float(delta.seconds) * 1000 + float(delta.microseconds) / 1000

    @property
    def elapsed_seconds(self):
        """ How long elapsed in the last timing run? In seconds.

        ..note::
            Only valid when the timer has previously been run and is currently\
            stopped.
        """
        delta = self._stamp_2 - self._stamp_1
        return float(delta.seconds) + float(delta.microseconds) / 1000000


def key_press_handler(*args):
    """
    Marks a method as being a handler for key presses that do not require the
    X,Y coordinates of the mouse at the time of pressing.
    """
    def wrapper(meth):
        # pylint: disable=protected-access
        meth._key_presses = tuple(args)
        return meth
    return wrapper


def key_release_handler(*args):
    """
    Marks a method as being a handler for key releases that do not require the
    X,Y coordinates of the mouse at the time of pressing.
    """
    def wrapper(meth):
        # pylint: disable=protected-access
        meth._key_releases = tuple(args)
        return meth
    return wrapper


def mouse_down_handler(*args):
    """
    Marks a method as being a handler for simple mouse presses.
    """
    def wrapper(meth):
        # pylint: disable=protected-access
        meth._mouse_presses = tuple(args)
        return meth
    return wrapper


def mouse_up_handler(*args):
    """
    Marks a method as being a handler for simple mouse releases.
    """
    def wrapper(meth):
        # pylint: disable=protected-access
        meth._mouse_releases = tuple(args)
        return meth
    return wrapper


class _Registry(AbstractBase):
    def __new__(cls, name, bases, namespace, **kwargs):
        new_cls = AbstractBase.__new__(cls, name, bases, namespace, **kwargs)
        for meth in namespace.values():
            for key in getattr(meth, "_key_presses", ()):
                new_cls._key_press_handlers[key] = meth
            for key in getattr(meth, "_key_releases", ()):
                new_cls._key_release_handlers[key] = meth
            for button in getattr(meth, "_mouse_presses", ()):
                new_cls._mouse_press_handlers[button] = meth
            for button in getattr(meth, "_mouse_releases", ()):
                new_cls._mouse_release_handlers[button] = meth
        return new_cls


class GlutFramework(object, metaclass=_Registry):
    ''' Base for code that wants to visualise using an OpenGL surface.
    '''
    # pylint: disable=broad-except
    __slots__ = [
        "display_timer",
        "elapsed_time_in_seconds",
        "frame_rate_timer",
        "frame_time",
        "frame_time_elapsed",
        "_logged_errors",
        "window"]

    _key_press_handlers = dict()
    _key_release_handlers = dict()
    _mouse_press_handlers = dict()
    _mouse_release_handlers = dict()

    def __init__(self):
        self.window = None
        self.frame_time_elapsed = 0.0
        self.frame_time = 0.0
        self.frame_rate_timer = _PerformanceTimer()
        self.display_timer = _PerformanceTimer()
        self.elapsed_time_in_seconds = 0.0
        self._logged_errors = set()

    # pylint: disable=unsupported-binary-operation
    def start_framework(self, args, title, width, height, posx, posy, fps, *,
                        display_mode=GLUT.GLUT_RGB | GLUT.GLUT_DOUBLE):
        """ start_framework will initialize framework and start the GLUT run\
            loop. It must be called after the GlutFramework class is created\
            to start the application.

        Not expected to return.
        """
        # Sets the instance to this, used in the callback wrapper functions
        self.frame_time = 1.0 / fps * 1000.0

        # Initialize GLUT
        GLUT.glutInit(args)
        GLUT.glutInitDisplayMode(display_mode)
        GLUT.glutInitWindowSize(width, height)
        GLUT.glutInitWindowPosition(posx, posy)
        self.window = GLUT.glutCreateWindow(title)
        try:
            GLUT.glutSetOption(GLUT.GLUT_ACTION_ON_WINDOW_CLOSE,
                               GLUT.GLUT_ACTION_CONTINUE_EXECUTION)
        except OpenGL.error.NullFunctionError:
            pass

        self.init()  # Initialize

        # Function callbacks with wrapper functions
        GLUT.glutDisplayFunc(self.__display_framework)
        GLUT.glutReshapeFunc(self.__reshape_framework)
        GLUT.glutIdleFunc(self.__run)
        GLUT.glutMouseFunc(self.__mouse_button_press)
        GLUT.glutMotionFunc(self.__mouse_move)
        GLUT.glutKeyboardFunc(self.__keyboard_down)
        GLUT.glutKeyboardUpFunc(self.__keyboard_up)
        GLUT.glutSpecialFunc(self.__special_keyboard_down)
        GLUT.glutSpecialUpFunc(self.__special_keyboard_up)
        GLUT.glutMenuStateFunc(self.__menu_state)
        try:
            GLUT.glutCloseFunc(self._terminate)
        except OpenGL.error.NullFunctionError:
            GLUT.glutWMCloseFunc(self._terminate)

        GLUT.glutMainLoop()

    def init(self):
        """ Initialises GLUT and registers any extra callback functions.
        """

    @abstractmethod
    def display(self, dTime):
        """ The display function is called at a specified frames-per-second\
            (FPS). Any animation drawing code can be run in the display method.

        :param dTime: the change in time (seconds)
        """

    def reshape(self, width, height):
        """ Called when the window dimensions change.

        :param width: the width of the window in pixels
        :param height: the height of the window in pixels
        """
        viewport(0, 0, width, height)

    def mouse_button_press(self, button, state, x, y):
        """ Called when the mouse buttons are pressed.

        :param button: the mouse buttons
        :param state: the state of the buttons
        :param x: the x coordinate
        :param y: the y coordinate
        """

    def mouse_move(self, x, y):
        """ Called when the mouse moves on the screen.

        :param x: the x coordinate
        :param y: the y coordinate
        """

    def keyboard_down(self, key, x, y):
        """ The keyboard function is called when a standard key is pressed\
            down.

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """

    def keyboard_up(self, key, x, y):
        """ The keyboard function is called when a standard key is "unpressed".

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """

    def special_keyboard_down(self, key, x, y):
        """ The keyboard function is called when a special key is pressed down\
            (F1 keys, Home, Inser, Delete, Page Up/Down, End, arrow keys).\
            http://www.opengl.org/resources/libraries/glut/spec3/node54.html

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """

    def special_keyboard_up(self, key, x, y):
        """ The keyboard function is called when a special key is "unpressed"\
            (F1 keys, Home, Inser, Delete, Page Up/Down, End, arrow keys).

        :param key: the key press
        :param x: the x coordinate of the mouse
        :param y: the y coordinate of the mouse
        """

    def menu_state(self, menu_open):
        """ Reports if a menu is open.

        :param bool menu_open:
        """

    def run(self):
        """ The run method is called by GLUT and contains the logic to set the\
            frame rate of the application.
        """
        if self.frame_rate_timer.stopped:
            self.frame_rate_timer.start()

        # stop the timer and calculate time since last frame
        self.frame_rate_timer.stop()
        milliseconds = self.frame_rate_timer.elapsed_milliseconds
        self.frame_time_elapsed += milliseconds

        if self.frame_time_elapsed >= self.frame_time:
            # If the time exceeds a certain "frame rate" then show the next
            # frame
            GLUT.glutPostRedisplay()

            # remove a "frame" and start counting up again
            self.frame_time_elapsed -= self.frame_time
        self.frame_rate_timer.start()

    def display_framework(self):
        """ The display_framework() function sets up initial GLUT state and\
            calculates the change in time between each frame. It calls the\
            display(float) function, which can be subclassed.
        """
        if self.display_timer.stopped:
            self.display_timer.start()
        self.display_timer.stop()
        elapsedTimeInSeconds = self.display_timer.elapsed_seconds
        if GLUT.glutGetWindow() == self.window:
            self.display(elapsedTimeInSeconds)
            GLUT.glutSwapBuffers()
        self.display_timer.start()

    def reshape_framework(self, width, height):
        """ Handle resizing of the window.
        """
        if GLUT.glutGetWindow() == self.window:
            self.reshape(width, height)

    def measure_text(self, string, font=Font.TimesRoman24):
        """ Utility function: measure the size of a piece of text in a font.
        """
        return GLUT.glutBitmapLength(font.value, string)

    @staticmethod
    def write_large(x, y, string, font=Font.TimesRoman24):
        """ Utility function: write a string to a given location as a bitmap.
        """
        # pylint: disable=no-member
        raster_position(x, y)
        for ch in string:
            GLUT.glutBitmapCharacter(font.value, ord(ch))

    @staticmethod
    def write_small(x, y, size, rotation, string):
        """ Utility function: write a string to a given location as a strokes.
        """
        # pylint: disable=no-member
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

    def _terminate(self, exit_code=0):
        """
        Because sys.exit() doesn't always work in the ctype-handled callbacks.
        """
        os._exit(exit_code)  # pylint: disable=protected-access

    @staticmethod
    def menu(callback, items):
        if not GLUT.glutGetWindow():
            return None
        the_menu = GLUT.glutCreateMenu(callback)
        for label, code in items:
            GLUT.glutAddMenuEntry(label, code)
        return the_menu

    @staticmethod
    def attach_current_menu(to):
        if GLUT.glutGetWindow():
            GLUT.glutAttachMenu(to)

    @staticmethod
    def destroy_menu(menu):
        if GLUT.glutGetWindow():
            GLUT.glutDestroyMenu(menu)

    def __display_framework(self):
        if not GLUT.glutGetWindow():
            return
        try:
            return self.display_framework()
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __reshape_framework(self, width, height):
        if not GLUT.glutGetWindow():
            return
        try:
            return self.reshape_framework(width, height)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __run(self):
        if not GLUT.glutGetWindow():
            return
        try:
            return self.run()
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __mouse_button_press(self, button, state, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            handler = None
            if state == mouse_down:
                handler = self._mouse_press_handlers.get(button, None)
            elif state == mouse_up:
                handler = self._mouse_release_handlers.get(button, None)
            if handler:
                handler(self, x, y)
                return
            self.mouse_button_press(button, state, x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __mouse_move(self, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            return self.mouse_move(x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __keyboard_down(self, key, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            keychar = key.decode()
            handler = self._key_press_handlers.get(keychar, None)
            if handler:
                return handler(self)
            return self.keyboard_down(keychar, x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __keyboard_up(self, key, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            keychar = key.decode()
            handler = self._key_release_handlers.get(keychar, None)
            if handler:
                return handler(self)
            return self.keyboard_up(key.decode(), x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __special_keyboard_down(self, key, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            handler = self._key_press_handlers.get(key, None)
            if handler:
                handler(self)
            return self.special_keyboard_down(key, x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __special_keyboard_up(self, key, x, y):
        if not GLUT.glutGetWindow():
            return
        try:
            handler = self._key_release_handlers.get(key, None)
            if handler:
                handler(self)
            return self.special_keyboard_up(key, x, y)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __menu_state(self, status):
        if not GLUT.glutGetWindow():
            return
        try:
            self.menu_state(status == GLUT.GLUT_MENU_IN_USE)
        except Exception:
            self.__log_error()
        except SystemExit:
            self._terminate()

    def __log_error(self):
        tb = traceback.format_exc()
        if tb not in self._logged_errors:
            self._logged_errors.add(tb)
            traceback.print_exc()
