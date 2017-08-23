import json
from spynnaker_visualisers.heat import state
from spynnaker_visualisers.heat.constants \
    import YDIMENSIONS, XDIMENSIONS, EACHCHIPX, EACHCHIPY, HISTORYSIZE,\
    MAXFRAMERATE, SDPPORT, FIXEDPOINT, ALTERSTEPSIZE, WINBORDER, WINHEIGHT,\
    KEYWIDTH, WINWIDTH, CONTROLBOXES, BOXSIZE, GAP


def param_load(filename):
    with open(filename) as f:
        data = json.load(f)
    state.title = data.get("title", "NO SIMULATION TITLE SUPPLIED")
    state.xdim, state.ydim = data.get("dimensions", [XDIMENSIONS, YDIMENSIONS])
    state.each_x, state.each_y = data.get("chip_size", [EACHCHIPX, EACHCHIPY])
    state.x_chips, state.y_chips = data.get("num_chips", [
        state.xdim / state.each_x, state.ydim / state.each_y])
    state.history_size = data.get("history_size", HISTORYSIZE)
    state.max_frame_rate = data.get("max_frame_rate", MAXFRAMERATE)
    state.our_port = data.get("sdp_port", SDPPORT)
    state.fixed_point_factor = 0.5 ** data.get("fixed_point", FIXEDPOINT)
    state.alter_step = data.get("alter_step_size", ALTERSTEPSIZE)

    state.windowBorder = WINBORDER
    state.windowHeight = WINHEIGHT
    state.windowWidth = WINWIDTH + KEYWIDTH
    state.plotwidth = state.windowWidth - 2 * state.windowBorder - KEYWIDTH
    state.printlabels = (state.windowBorder >= 100)

    state.xorigin = (state.windowWidth + KEYWIDTH -
                     CONTROLBOXES * (BOXSIZE + GAP))

    n_elems = state.xdim * state.ydim
    state.history_data = [[0.0 for _ in xrange(n_elems)]
                          for _ in xrange(state.history_size)]
    state.immediate_data = [0.0 for _ in xrange(n_elems)]


def parse_arguments(args):
    pass
