import datetime


def timestamp():
    td = datetime.datetime.now()
    return td.microsecond + (td.second + td.day * 86400) * 10**6
