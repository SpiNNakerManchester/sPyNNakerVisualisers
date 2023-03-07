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

# encoding: utf-8
""" A live plotter for the sPyNNaker Sudoku network.
"""
from argparse import ArgumentParser, REMAINDER
import sys
from threading import Condition, RLock
from spinn_utilities.overrides import overrides
from spinn_front_end_common.utilities.connections import LiveEventConnection
from spynnaker_visualisers.glut_framework import GlutFramework
from spynnaker_visualisers.opengl_support import (
    vertex, draw, lines, color, point_size, points, line_width, clear_color,
    clear, color_buffer_bit, load_identity, viewport, matrix_mode, projection,
    model_view, orthographic_projction, shade_model, smooth)

__all__ = []
__version__ = 1
__date__ = '2017-07-25'

WINDOW_BORDER = 110
INIT_WINDOW_WIDTH = 800
INIT_WINDOW_HEIGHT = 600
INIT_WINDOW_X = 100
INIT_WINDOW_Y = 100
FRAMES_PER_SECOND = 10


class SudokuPlot(GlutFramework):
    """ A live plotter for the sPyNNaker Sudoku network.
    """
    __slots__ = [
        "args",
        "cell_id",
        "cell_labels",
        "cell_size_map",
        "database_read",
        "label_to_cell_map",
        "latest_time",
        "ms_per_bin",
        "n_neurons",
        "n_populations_to_read",
        "neurons_per_number",
        "plot_time_ms",
        "point_mutex",
        "points_to_draw",
        "simulation_started",
        "start_condition",
        "timestep_ms",
        "user_pressed_start",
        "window_height",
        "window_width"]

    def __init__(self, args, neurons_per_number, ms_per_bin, wait_for_start):
        """
        :param args:
            Arguments (relating to the display) to pass through to GLUT
        :param neurons_per_number:
            How many neurons are used per number in the Sudoku cells
        :param ms_per_bin:
            How long does a sampling period last
        :param wait_for_start:
            Whether the system should wait for the SpiNNaker simulation to\
            boot (probably yes!)
        """
        super(SudokuPlot, self).__init__()
        self.window_width = INIT_WINDOW_WIDTH
        self.window_height = INIT_WINDOW_HEIGHT

        self.cell_id = 0
        self.user_pressed_start = not wait_for_start
        self.simulation_started = False
        self.database_read = False
        self.n_neurons = 0
        self.timestep_ms = 0
        self.plot_time_ms = 0
        self.ms_per_bin = float(ms_per_bin)
        self.latest_time = 0.0
        self.neurons_per_number = neurons_per_number

        self.n_populations_to_read = 1

        self.args = args

        self.points_to_draw = [[] for _ in range(81)]
        self.point_mutex = RLock()

        self.label_to_cell_map = dict()
        self.cell_size_map = dict()
        self.cell_labels = dict()

        self.start_condition = Condition()

    @overrides(GlutFramework.init)
    def init(self):
        clear_color(0.0, 0.0, 0.0, 1.0)
        color(1.0, 1.0, 1.0)
        shade_model(smooth)

    def connect_callbacks(self, connection, label):
        """ Arrange so that labels on the given connection report their\
            goings-on to this class.

        :type connection: LiveEventConnection
        :type label: str
        """
        connection.add_init_callback(label, self._init_cb)
        connection.add_receive_callback(label, self._receive_cb)
        connection.add_start_resume_callback(label, self._start_cb)

    def _init_cb(self, label, n_neurons, run_time_ms, machine_time_step_ms):
        self.plot_time_ms = float(run_time_ms)
        self.timestep_ms = float(machine_time_step_ms)

        self.cell_labels[self.cell_id] = label
        self.cell_size_map[self.cell_id] = n_neurons
        self.label_to_cell_map[label] = self.cell_id
        self.n_neurons += n_neurons
        self.cell_id += 1

        with self.start_condition:
            self.n_populations_to_read -= 1
            if self.n_populations_to_read <= 0:
                self.database_read = True
                while not self.user_pressed_start:
                    self.start_condition.wait()

    def _start_cb(self, *args):  # @UnusedVariable
        with self.start_condition:
            self.simulation_started = True

    def _receive_cb(self, label, time, spikes=None):  # @UnusedVariable
        if spikes is None:
            spikes = []
        with self.point_mutex:
            for spike in spikes:
                cell_id, neuron_id = divmod(
                    spike, self.neurons_per_number * 9)
                self.points_to_draw[cell_id].append((time, neuron_id))
            time_ms = time * self.timestep_ms
            if time_ms > self.latest_time:
                self.latest_time = time_ms

    def main_loop(self):
        """ Run the GUI.
        """
        self.start_framework(
            self.args, "Sudoku", self.window_width, self.window_height,
            INIT_WINDOW_X, INIT_WINDOW_Y, FRAMES_PER_SECOND)

    @overrides(GlutFramework.display)
    def display(self, dTime):  # @UnusedVariable
        self._start_display()

        cell_width = (self.window_width - 2 * WINDOW_BORDER) / 9.0
        cell_height = (self.window_height - 2 * WINDOW_BORDER) / 9.0
        end = self.latest_time
        start = end - self.ms_per_bin
        if start < 0.0:
            start = 0.0
            end = start + self.ms_per_bin

        with self.start_condition:
            if not self.database_read:
                prompt = "Waiting for simulation to load..."
            elif not self.user_pressed_start:
                prompt = "Press space bar to start..."
            elif not self.simulation_started:
                prompt = "Waiting for simulation to start..."
            else:
                prompt = "Sudoku"
        self._print_text(prompt)

        self._draw_cells(cell_width, cell_height)

        if self.timestep_ms != 0:
            x_spacing = cell_width / ((end - start) / self.timestep_ms)
            start_tick = int(start / self.timestep_ms)
            with self.point_mutex:
                values, probs = self._find_cell_values(start_tick)
                valid = self._find_cell_correctness(values)
                self._draw_cell_contents(values, valid, probs, start_tick,
                                         x_spacing, cell_width, cell_height)

    @overrides(GlutFramework.reshape)
    def reshape(self, width, height):
        self.window_width = width
        self.window_height = height

        # Viewport dimensions
        viewport(0, 0, width, height)
        matrix_mode(projection)
        load_identity()

        # An orthographic projection. Should probably look into OpenGL
        # perspective projections for 3D if that's your thing
        orthographic_projction(0.0, width, 0.0, height, -50.0, 50.0)
        matrix_mode(model_view)
        load_identity()

    @overrides(GlutFramework.keyboard_down)
    def keyboard_down(self, key, x, y):  # @UnusedVariable
        if key == 32 or key == ' ':
            with self.start_condition:
                if not self.user_pressed_start:
                    print("Starting the simulation")
                    self.user_pressed_start = True
                    self.start_condition.notify_all()

    def _find_cell_values(self, start_tick):
        cell_value = [0] * 81
        cell_prob = [0.0] * 81

        for cell in range(81):
            # Strip off items that are no longer needed
            queue = self.points_to_draw[cell]
            while queue and queue[0][0] < start_tick:
                queue.pop(0)

            # Count the spikes per number
            count, total = self._count_spikes_per_number(queue)

            # Work out the probability of a given number in a given cell
            max_prob_number = 0
            max_prob = 0.0
            for i in range(9):
                if count[i] > 0:
                    prob = count[i] / total
                    if prob > max_prob:
                        max_prob = prob
                        max_prob_number = i + 1
            cell_value[cell] = max_prob_number
            cell_prob[cell] = max_prob
        return cell_value, cell_prob

    def _count_spikes_per_number(self, queue):
        count = [0] * 9
        total = 0
        for (_, n_id) in queue:
            number = n_id // self.neurons_per_number
            if number < 9:
                count[number] += 1
                total += 1
            else:
                sys.stderr.write(f"Neuron id {n_id} out of range\n")
        return count, float(total)

    def _find_cell_correctness(self, values):
        # Work out the correctness of each cell
        cell_valid = [True] * 81
        for cell in range(81):
            y, x = divmod(cell, 9)
            for row in range(9):
                if row != y:
                    self._check_cell(values, cell_valid, x, y, row, x)
            for col in range(9):
                if col != x:
                    self._check_cell(values, cell_valid, x, y, y, col)
            for row in range(3 * (y // 3), 3 * (y // 3 + 1)):
                for col in range(3 * (x // 3), 3 * (x // 3 + 1)):
                    if x != col and y != row:
                        self._check_cell(values, cell_valid, x, y, row, col)
        return cell_valid

    @staticmethod
    def _start_display():
        point_size(1.0)
        clear(color_buffer_bit)
        clear_color(1.0, 1.0, 1.0, 1.0)
        color(0.0, 0.0, 0.0, 1.0)

    # TODO positioning
    # https://github.com/SpiNNakerManchester/sPyNNakerVisualisers/issues/23
    def _print_text(self, prompt):
        # Guesstimate of length of prompt in pixels
        plen = len(prompt) * 4
        self.write_large(
            self.window_width / 2 - plen, self.window_height - 50, prompt)

    def _draw_cells(self, width, height):
        color(0.0, 0.0, 0.0, 1.0)
        for i in range(10):
            line_width(3.0 if i % 3 == 0 else 1.0)
            pos = WINDOW_BORDER + i * height
            self._line(self.window_width - WINDOW_BORDER, pos,
                       WINDOW_BORDER, pos)
            pos = WINDOW_BORDER + i * width
            self._line(pos, self.window_height - WINDOW_BORDER,
                       pos, WINDOW_BORDER)

    def _draw_cell_contents(self, value, valid, prob, start, x_spacing,
                            cell_width, cell_height):
        # Print the spikes
        for cell in range(81):
            cell_y, cell_x = divmod(cell, 9)
            x_start = WINDOW_BORDER + (cell_x * cell_width) + 1
            y_start = WINDOW_BORDER + (cell_y * cell_height) + 1
            y_spacing = cell_height / (self.neurons_per_number * 9.0)

            # Work out how probable the number is and use this for colouring
            cell_sat = 1 - prob[cell]

            point_size(2.0)
            with draw(points):
                if valid[cell]:
                    color(cell_sat, 1.0, cell_sat, 1.0)
                else:
                    color(1.0, cell_sat, cell_sat, 1.0)
                for (time, n_id) in self.points_to_draw[cell]:
                    x_value = (time - start) * x_spacing + x_start
                    y_value = n_id * y_spacing + y_start
                    vertex(x_value, y_value)

            # Print the number
            if value[cell] != 0:
                color(0, 0, 0, 1 - cell_sat)
                size = 0.005 * cell_height
                self.write_small(
                    x_start + (cell_width / 2.0) - (size * 50.0),
                    y_start + (cell_height / 2.0) - (size * 50.0),
                    size, 0, "%d", value[cell])

    @staticmethod
    def _line(x1, y1, x2, y2):
        with draw(lines):
            vertex(x1, y1)
            vertex(x2, y2)

    @staticmethod
    def _check_cell(values, correct, x, y, row, col):
        value = values[y * 9 + x]
        if value == values[row * 9 + col]:
            correct[y * 9 + x] = False


# https://github.com/SpiNNakerManchester/sPyNNakerVisualisers/issues/24
def sudoku_visualiser(args, port=19999, neurons=5, ms=100, database=None):
    """ Make a visualiser, connecting a LiveEventConnection that listens to a\
        population labelled "Cells" to a GLUT GUI.
    """
    # Set up the application
    cells = ["Cells"]
    connection = LiveEventConnection(
        "LiveSpikeReceiver", receive_labels=cells, local_port=port)
    plotter = SudokuPlot(args, neurons, ms, database is None)
    for label in cells:
        plotter.connect_callbacks(connection, label)
    if database is not None:
        # TODO: This concept not present on Python side!
        # connection.set_database(database)
        sys.stderr.write("Database setting not currently supported")
    plotter.main_loop()


def main(argv=None):
    """ The main script.\
        Parses command line arguments and launches the visualiser.
    """
    program_name = "sudoku_visualiser"
    program_version = "v%d" % (__version__)
    program_description = "Visualise the SpiNNaker sudoku solver."
    program_version_string = '%%prog %s (%s)' % (program_version, __date__)

    # setup option parser
    parser = ArgumentParser(prog=program_name,
                            description=program_description)
    parser.add_argument(
        "-d", "--database", dest="database", metavar="FILE",
        help="optional file path to where the database is located, if "
        "needed for manual configuration", default=None)
    parser.add_argument(
        "-m", "--ms_per_bin", dest="ms", metavar="MILLISECONDS",
        help="optional number of milliseconds to show at once",
        type=float, default=100)
    parser.add_argument(
        "-n", "--neurons_per_number", dest="neurons",
        help="the number of neurons that represent each number in a cell",
        metavar="COUNT", type=int, default=5)
    parser.add_argument(
        "-p", "--hand_shake_port", dest="port", default="19999",
        help="optional port which the visualiser will listen to for"
        " database hand shaking", metavar="PORT", type=int)
    parser.add_argument('--version', action='version',
                        version=program_version_string)
    parser.add_argument("args", nargs=REMAINDER)

    # Set up and run the application
    try:
        if argv is None:
            argv = sys.argv[1:]
        sudoku_visualiser(**parser.parse_args(argv).__dict__)
        return 0
    except Exception as e:  # pylint: disable=broad-except
        indent = len(program_name) * " "
        sys.stderr.write(program_name + ": " + repr(e) + "\n")
        sys.stderr.write(indent + "  for help use --help")
        return 2


if __name__ == "__main__":
    sys.exit(main())
