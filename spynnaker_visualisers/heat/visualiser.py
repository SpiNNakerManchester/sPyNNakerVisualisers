from argparse import ArgumentParser
import sys
import threading

from spynnaker_visualisers.heat.constants import NOTDEFINED
from spynnaker_visualisers.heat import events, sdp, state, utils

__version__ = 18
__date__ = '2017-08-23'


# -------------------------------------------------------------------


def parse_arguments(args):
    program_name = "sudoku_visualiser"
    program_version = "v%d" % (__version__)
    program_description = "Visualise the SpiNNaker heat map."
    program_version_string = '%%prog %s (%s)' % (program_version, __date__)

    # setup option parser
    parser = ArgumentParser(prog=program_name,
                            version=program_version_string,
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

    if args is None:
        args = sys.argv[1:]
    parsed = parser.parse_args(args)
    print("Will load configuration from: %s", parsed.config)
    if parsed.ip is not None:
        sdp.set_board_ip_address(parsed.ip)
        print("Waiting for packets only from: %s" % sdp.get_board_ip_address())
    return parsed.config


def main():
    configfile = parse_arguments(sys.argv[1:])
    state.param_load(configfile)
    state.cleardown()
    state.starttime = utils.timestamp()

    state.history_data = [
        [NOTDEFINED for _ in range(state.xdim * state.ydim)]
        for _ in range(state.history_size)]

    sdp.init_listening()
    threading.Thread(target=sdp.input_thread)

    events.run_GUI(sys.argv)
    print("goodbye")
    sys.exit(0)


if __name__ == "__main__":
    main()
