# Copyright (c) 2017-2021 The University of Manchester
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

from argparse import ArgumentParser
import re
import sys

from spynnaker_visualisers.heat.gui import GUI
import spynnaker_visualisers._version as ver

__version__ = re.sub(r"^.*!", "", ver.__version__)
__date__ = \
    f'{ver.__version_day__} {ver.__version_month__} {ver.__version_year__}'

# -------------------------------------------------------------------


def parse_arguments(args):
    program_name = "sudoku_visualiser"
    program_version = f"v{__version__}"
    program_description = "Visualise the SpiNNaker heat map."
    program_version_string = f'%(prog)s {program_version} ({__date__})'

    # setup option parser
    parser = ArgumentParser(prog=program_name,
                            description=program_description)
    parser.add_argument(
        "-c", "--config", dest="config", metavar="FILE",
        help="file path to where the configuration JSON file is located",
        default="visparam.json")
    parser.add_argument(
        "-i", "--board-ip", dest="ip", metavar="ADDRESS",
        help="the address of the SpiNNaker board running the Heat Demo; if "
        "omitted, the address of the system that contacts the visualiser "
        "will be used", default=None)
    parser.add_argument(
        "--version", action="version", version=program_version_string)

    if args is None:
        args = sys.argv[1:]
    parsed = parser.parse_args(args)
    print(f"Will load configuration from: {parsed.config}")
    return parsed.config, parsed.ip


def main():
    configfile, ipaddr = parse_arguments(sys.argv[1:])
    gui = GUI(configfile)
    gui.init_history()
    gui.cleardown()
    if ipaddr is not None:
        gui.set_board_ip_address(ipaddr)
        print(f"Waiting for packets only from: {gui.get_board_ip_address()}")
    gui.launch(sys.argv)
    print("goodbye")
    sys.exit(0)


if __name__ == "__main__":
    main()
