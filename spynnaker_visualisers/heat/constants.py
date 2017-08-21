from enum import Enum

MAXDATA = 65535.0
MINDATA = -65535.0
NOTDEFINED = -66666.0

XDIMENSIONS = 32
YDIMENSIONS = 32
EACHCHIPX = 4
EACHCHIPY = 4
XCHIPS = XDIMENSIONS / EACHCHIPX
YCHIPS = YDIMENSIONS / EACHCHIPY

TIMEWINDOW = 3.5
HISTORYSIZE = 3500

HIWATER = 10.0
LOWATER = 0.0

WINBORDER = 110
WINHEIGHT = 700
WINWIDTH = 850
KEYWIDTH = 50

CONTROLBOXES = 3
BOXSIZE = 40
GAP = 5

MAXFRAMERATE = 25
SDPPORT = 17894
FIXEDPOINT = 16

MAXBLOCKSIZE = 364
SPINN_HELLO = 0x41
P2P_SPINN_PACKET = 0x3A
STIM_IN_SPINN_PACKET = 0x49
MTU = 1515


class UIColours(Enum):
    BLACK = 0
    WHITE = 1
    RED = 2
    GREEN = 3
    CYAN = 4
    GREY = 5
