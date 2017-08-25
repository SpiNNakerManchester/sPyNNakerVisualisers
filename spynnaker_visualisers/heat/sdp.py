import socket
import struct
import sys

from spynnaker_visualisers.heat import state
from spynnaker_visualisers.heat.constants \
    import MTU, SPINN_HELLO, TIMEWINDOW, NOTDEFINED
from spynnaker_visualisers.heat.utils import timestamp


_board_ip = None
_board_ip_set = False
_board_port = None
_board_address = None
_sock_input = None
_sock_output = None
_last_history_line_updated = 0


def send_to_chip(id, port, command, *args):  # @ReservedAssignment
    x, y = divmod(id, state.x_chips)
    dest = 256 * x + y
    sender(dest, port, command, *args)


def all_desired_chips():
    # for i in xrange(state.x_chips * state.y_chips):
    #     yield i
    yield 1


def set_board_ip_address(address):
    global _board_ip, _board_ip_set
    _board_ip = address
    _board_ip_set = True


def get_board_ip_address():
    return socket.gethostbyname(_board_ip)


def is_board_address_set():
    return _board_ip_set


def is_board_port_set():
    return _board_port is not None


def init_listening():
    for family, socktype, proto, _, sockaddr in socket.getaddrinfo(
            None, state.our_port, socket.AF_INET, socket.SOCK_DGRAM,
            socket.IPPROTO_UDP, socket.AI_PASSIVE):
        try:
            _sock_input = socket.socket(family, socktype, proto)
            _sock_input.bind(sockaddr)
            return _sock_input
        except:
            print("falling over to next possible address")
    print("failed to bind socket for listening")
    sys.exit(1)


def input_thread():
    global _board_port
    sdp_header_len = 26
    while True:
        try:
            string, address = _sock_input.recvfrom(MTU)
            host, port = address
        except:
            return

        buf = memoryview(string)
        num_additional_bytes = len(buf) - sdp_header_len
        if struct.unpack_from("!I", buf, 2)[0] == SPINN_HELLO:
            continue

        if not is_board_address_set():
            _board_port = port
            set_board_ip_address(host)
            init_sender()
            print("packet received from %s on port %d" % (str(host), port))
        elif not is_board_port_set():
            _board_port = port
            init_sender()
            print("packet received from %s on port %d" % (str(host), port))

        nowtime = timestamp()
        if state.firstreceivetime == 0:
            state.firstreceivetime = nowtime

        if not state.freezedisplay:
            update_history_data(nowtime)

        timeperindex = TIMEWINDOW / float(state.plotwidth)
        updateline = ((nowtime - state.starttime) / timeperindex / 1000000) \
            % state.history_size
        process_heatmap_packet(buf, num_additional_bytes, updateline)


def update_history_data(updateline):
    global _last_history_line_updated

    linestoclear = updateline - _last_history_line_updated
    if linestoclear < 0 and updateline + 500 > _last_history_line_updated:
        linestoclear = 0
    if linestoclear < 0:
        linestoclear = (updateline + state.history_size -
                        _last_history_line_updated)

    num_pts = state.xdim * state.ydim
    for i in xrange(linestoclear):
        rowid = (1 + i + _last_history_line_updated) % state.history_size
        for j in xrange(num_pts):
            state.history_data[rowid][j] = NOTDEFINED

    _last_history_line_updated = updateline


def process_heatmap_packet(buf, n_bytes, updateline):
    if state.freezedisplay:
        return

    # take the chip ID and works out the chip X,Y coords
    src_addr = struct.unpack_from("<H", buf, 6)[0]
    xsrc = src_addr / 256
    ysrc = src_addr % 256

    # for all extra data (assuming regular array of 4 byte words)
    for i in xrange(n_bytes / 4):
        arrayindex = (state.x_chips * state.y_chips) \
            * (xsrc * state.x_chips + ysrc) + i
        if arrayindex > state.xdim * state.ydim:
            continue
        data = struct.unpack_from("<I", buf, 26 + i * 4)[0] \
            * state.fixed_point_factor
        state.immediate_data[arrayindex] = data
        state.history_data[updateline][arrayindex] = data
        state.somethingtoplot = True


def init_sender():
    global _sock_output, _board_address
    for family, socktype, proto, _, sockaddr in socket.getaddrinfo(
            _board_ip, str(_board_port), socket.AF_INET, socket.SOCK_DGRAM):
        try:
            _sock_output = socket.socket(family, socktype, proto)
            _board_address = sockaddr
            return _sock_output
        except:
            print("falling over to next possible address")
    print("failed to bind socket for listening")
    sys.exit(1)


def sender(dest_addr, dest_port, command, arg1, arg2, arg3, *args):
    # FIXME!
    ip_time_out = 0
    flags = 7
    tag = 255
    src_port = 0xFF
    src_addr = 0
    seq = 0
    msg = memoryview(struct.pack("<BxBBBBHHHHIII", ip_time_out, flags, tag,
                                 dest_port, src_port, 0, 0,
                                 command, seq, arg1, arg2, arg3))
    struct.pack_into("!HH", msg, 6, dest_addr, src_addr)
    msg = msg.tobytes()
    for a in args:
        msg += struct.pack("<I", a)

    if is_board_address_set():
        _sock_output.sendto(msg, _board_address)
    state.pktgone = timestamp()
