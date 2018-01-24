# This file makes the OpenGL interface a little more python-pretty. It's
# massively incomplete; feel free to add to it as required.

import OpenGL.GL as GL  # pylint: disable=import-error

blend = GL.GL_BLEND
color_buffer_bit = GL.GL_COLOR_BUFFER_BIT
line_smooth = GL.GL_LINE_SMOOTH
lines = GL.GL_LINES
model_view = GL.GL_MODELVIEW
one_minus_src_alpha = GL.GL_ONE_MINUS_SRC_ALPHA
points = GL.GL_POINTS
projection = GL.GL_PROJECTION
smooth = GL.GL_SMOOTH
src_alpha = GL.GL_SRC_ALPHA


def blend_function(sfactor, dfactor):
    GL.glBlendFunc(sfactor, dfactor)


def clear(mask):
    GL.glClear(mask)


def clear_color(red, green, blue, alpha=1.0):
    GL.glClearColor(float(red), float(green), float(blue), float(alpha))


def color(*args):
    GL.glColor(*args)


def disable(*args):
    for feature in args:
        GL.glDisable(feature)


def enable(*args):
    for feature in args:
        GL.glEnable(feature)


def line_width(width):
    GL.glLineWidth(float(width))


def load_identity():
    GL.glLoadIdentity()


def matrix_mode(mode):
    GL.glMatrixMode(mode)


def orthographic_projction(*args):
    GL.glOrtho(*args)


def point_size(size):
    GL.glPointSize(float(size))


def raster_position(*args):
    GL.glRasterPos(*args)


def rotate(angle, x, y, z):
    GL.glRotatef(angle, x, y, z)


def scale(x, y, z):
    GL.glScale(x, y, z)


def shade_model(mode):
    GL.glShadeModel(mode)


def translate(x, y, z):
    GL.glTranslate(x, y, z)


def vertex(*args):
    GL.glVertex(*args)


def viewport(x, y, width, height):
    GL.glViewport(int(x), int(y), int(width), int(height))


class _context(object):
    def __enter__(self):
        self._enter()

    def __exit__(self, exc_type, exc_value, traceback):  # @UnusedVariable
        # pylint: disable=unused-argument
        self._leave()
        return False


class draw(_context):
    def __init__(self, drawing_style):
        self.style = drawing_style

    def _enter(self):
        GL.glBegin(self.style)

    def _leave(self):
        GL.glEnd()


class save_matrix(_context):
    def _enter(self):
        GL.glPushMatrix()

    def _leave(self):
        GL.glPopMatrix()
