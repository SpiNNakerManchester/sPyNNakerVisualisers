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

"""
A wrapper round OpenGL for doing simple GUIs.
"""

from .glut_framework import (
    key_up, key_down, key_left, key_right, display_mode_double,
    left_button, right_button, scroll_up, scroll_down, mouse_down,
    Font, GlutFramework, key_press_handler, key_release_handler,
    mouse_down_handler, mouse_up_handler)
from .opengl_support import (
    blend, color_buffer_bit, depth_buffer_bit, line_smooth, lines, model_view,
    one_minus_src_alpha, points, projection, smooth, src_alpha, depth_test,
    rgb, unsigned_byte, quads, line_loop, triangles,
    blend_function, clear, clear_color, color, disable, enable,
    line_width, load_identity, matrix_mode, orthographic_projction,
    point_size, raster_position, rotate, scale, shade_model, translate,
    vertex, viewport, draw_pixels, draw, save_matrix)

__all__ = (
    'key_up', 'key_down', 'key_left', 'key_right', 'display_mode_double',
    'left_button', 'right_button', 'scroll_up', 'scroll_down', 'mouse_down',
    'Font', 'GlutFramework', 'key_press_handler', 'key_release_handler',
    'mouse_down_handler', 'mouse_up_handler',
    'blend', 'color_buffer_bit', 'depth_buffer_bit', 'line_smooth', 'lines',
    'model_view', 'one_minus_src_alpha', 'points', 'projection', 'smooth',
    'src_alpha', 'depth_test', 'rgb', 'unsigned_byte', 'quads', 'line_loop',
    'triangles',
    'blend_function', 'clear', 'clear_color', 'color', 'disable', 'enable',
    'line_width', 'load_identity', 'matrix_mode', 'orthographic_projction',
    'point_size', 'raster_position', 'rotate', 'scale', 'shade_model',
    'translate', 'vertex', 'viewport', 'draw_pixels', 'draw', 'save_matrix')
