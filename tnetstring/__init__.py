"""

tnetstring:  data serialization using typed netstrings
======================================================


This is a data serialization library. It's a lot like JSON but it uses a
new syntax called "typed netstrings" that Zed has proposed for use in the
the Mongrel2 webserver.  It's designed to be simpler and easier to implement
than JSON, with a happy consequence of also being faster.

An ordinary netstring is a blob of data prefixed with its length and postfixed
with a sanity-checking comma.  The string "hello world" encodes like this::

    11:hello world,

Typed netstings add other datatypes by replacing the comma with a type tag.
Here's the integer 12345 encoded as a tnetstring::

    5:12345#

And here's a list mixing integers and bools::

    19:5:12345#4:true!1:0#]

Simple enough?  This module gives you the following functions:

    :dumps:   dump an object as a tnetstring to a string
    :loads:   load a tnetstring-encoded object from a string
    :pop:     pop a tnetstring-encoded object from the front of a string


When I get around to it, I will also add the following:

    :dump:    dump an object as a tnetstring to a file
    :load:    load a tnetstring-encoded object from a file

Note that since parsing a tnetstring requires reading all the data into memory
at once, there's no efficiency gain from using the file-based versions of these
functions; I'm only planning to add them for API compatability with other
serialization modules e.g. pickle and json.

"""

__ver_major__ = 0
__ver_minor__ = 1
__ver_patch__ = 0
__ver_sub__ = ""
__version__ = "%d.%d.%d%s" % (__ver_major__,__ver_minor__,__ver_patch__,__ver_sub__)


class Error(Exception):
    pass

class LoadError(Error):
    pass

class DumpError(Error):
    pass


def dumps(v):
    d = _dump_value(v)
    return "%d:%s" % (len(d)-1,d)

def _dump_value(v):
    if v is None:
        return "~"
    if v is True:
        return "true!"
    if v is False:
        return "false!"
    if isinstance(v,(int,long)):
        return str(v) + "#"
    if isinstance(v,(float,)):
        return repr(v) + "#"
    if isinstance(v,(str,)):
        return v + ","
    if isinstance(v,(list,tuple,)):
        return "".join(dumps(item) for item in v) + "]"
    if isinstance(v,(dict,)):
        def getitems():
            for (k,i) in v.iteritems():
                yield dumps(k)
                yield dumps(i)
        return "".join(getitems()) + "}"
    raise DumpError("unserializable object")


def loads(string):
    return pop(string)[0]


def pop(string):
    (data,type,rest) = _pop_netstring(string)
    if type == ",":
        return (data,rest)
    if type == "#":
        if "." in data or "e" in data or "E" in data:
            return (float(data),rest)
        else:
            return (int(data),rest)
    if type == "!":
        if data == "true":
            return (True,rest)
        else:
            return (False,rest)
    if type == "~":
        if data:
            raise LoadError("null must be zero length")
        return (None,rest)
    if type == "]":
        l = []
        while data:
            (item,data) = pop(data)
            l.append(item)
        return (l,rest)
    if type == "}":
        d = {}
        while data:
            (key,data) = pop(data)
            (val,data) = pop(data)
            d[key] = val
        return (d,rest)
    raise LoadError("unknown type tag")


def _pop_netstring(string):
    if not string:
        raise LoadError("empty string")
    (dlen,rest) = string.split(":",1)
    dlen = int(dlen)
    (data,type,rest) = (rest[:dlen],rest[dlen],rest[dlen+1:])
    if len(data) != dlen or not type:
        raise LoadError("badly formatted")
    return (data,type,rest)


#  Use the c-extension version if available
try:
    import _tnetstring
except ImportError:
    pass
else:
    Error = _tnetstring.Error
    LoadError = _tnetstring.LoadError
    DumpError = _tnetstring.DumpError
    #dump = _tnetstring.dump
    dumps = _tnetstring.dumps
    #load = _tnetstring.load
    loads = _tnetstring.loads
    pop = _tnetstring.pop


