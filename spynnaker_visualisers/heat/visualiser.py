import sys
import threading

from .constants import HISTORYSIZE, NOTDEFINED
import spynnaker_visualisers.heat.config as config
import spynnaker_visualisers.heat.events as events
import spynnaker_visualisers.heat.sdp as sdp
import spynnaker_visualisers.heat.state as state
import spynnaker_visualisers.heat.utils as utils


# -------------------------------------------------------------------


def main(argv):
    configfile = config.parse_arguments(argv)
    if configfile is None:
        configfile = "visparam.json"
    config.param_load(configfile)

    state.cleardown()
    state.starttimez = utils.timestamp()

    state.history_data = [
        [NOTDEFINED for _ in xrange(state.xdim * state.ydim)]
        for _ in xrange(HISTORYSIZE)]
    
    sdp.init_listening()
    threading.Thread(target=sdp.input_thread)

    events.run_GUI(argv)
    print("goodbye")
    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv)
