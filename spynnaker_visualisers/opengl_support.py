"""
This file makes the OpenGL interface a little more python-pretty. It's
massively incomplete; feel free to add to it as required.
"""

from contextlib import contextmanager
import OpenGL.GL as GL

# pylint: disable=invalid-name
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
# pylint: enable=invalid-name


def blend_function(sfactor, dfactor):
    """ Set the blending function. """
    GL.glBlendFunc(sfactor, dfactor)


def clear(mask):
    """ Clear the drawing surface. """
    GL.glClear(mask)


def clear_color(red, green, blue, alpha=1.0):
    """ Clear the surface to the given colour. """
    GL.glClearColor(float(red), float(green), float(blue), float(alpha))


def color(*args):
    """ Set the drawing colour. """
    GL.glColor(*args)


def disable(*args):
    """ Disable the listed features. """
    for feature in args:
        GL.glDisable(feature)


def enable(*args):
    """ Enable the listed features. """
    for feature in args:
        GL.glEnable(feature)


def line_width(width):
    """ Set the line width. """
    GL.glLineWidth(float(width))


def load_identity():
    """ Load the identity matrix. """
    GL.glLoadIdentity()


def matrix_mode(mode):
    """ Set the matrix mode. """
    GL.glMatrixMode(mode)


def orthographic_projction(*args):
    """ Set an orthographic (non-perspective) projection. """
    GL.glOrtho(*args)


def point_size(size):
    """ Set the size of points. """
    GL.glPointSize(float(size))


def raster_position(*args):
    """ Set the raster position. """
    GL.glRasterPos(*args)


def rotate(angle, x, y, z):
    """ Rotate the projection about a point. """
    GL.glRotatef(angle, x, y, z)


def scale(x, y, z):
    """ Scale the projection about the origin. """
    GL.glScale(x, y, z)


def shade_model(mode):
    """ Set the shading model. """
    GL.glShadeModel(mode)


def translate(x, y, z):
    """ Translate the projection. """
    GL.glTranslate(x, y, z)


def vertex(*args):
    """ Mark a vertex of a drawing path. """
    GL.glVertex(*args)


def viewport(x, y, width, height):
    """ Set up the view port. """
    GL.glViewport(int(x), int(y), int(width), int(height))


@contextmanager
def draw(drawing_style):
    """ Draw a line, set of points or closed curve (depending on\
        drawing_style). Use as a context manager and specify the vertices of\
        the path in the body of the context.
    """
    GL.glBegin(drawing_style)
    yield
    GL.glEnd()


@contextmanager
def save_matrix():
    """ Manipulate the view matrix in a temporary context; the view matrix is\
        restored once this context is left.
    """
    GL.glPushMatrix()
    yield
    GL.glPopMatrix()
