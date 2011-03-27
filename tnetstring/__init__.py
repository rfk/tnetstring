"""

tnetstring:  data serialization using typed netstrings
======================================================


This is a data serialization library. It's a lot like JSON but it uses a
new syntax called "typed netstrings" that Zed has proposed for use in the
Mongrel2 webserver.  It's designed to be simpler and easier to implement
than JSON, with a happy consequence of also being faster.

An ordinary netstring is a blob of data prefixed with its length and postfixed
with a sanity-checking comma.  The string "hello world" encodes like this::

    11:hello world,

Typed netstrings add other datatypes by replacing the comma with a type tag.
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


from collections import deque


try:
    from cStringIO import StringIO
except ImportError:
    from StringIO import StringIO


class Error(Exception):
    """Base error class for the tnetstring module."""
    pass

class LoadError(Error):
    """Error class raised when there's a problem loading a tnetstring."""
    pass

class DumpError(Error):
    """Error class raised when there's a problem dumping a tnetstring."""
    pass



def dumps(value):
    """dumps(object) -> string

    This function dumps a python object as a tnetstring.
    """
    #  This uses the naive bottom-up generator, it's quite slow.
    #return "".join(_gdumps(value))
    #  This uses the write-in-reverse trick from the C version.
    #  It's faster, but loses a lot in reversing the python strings.
    #s = StringIO()
    #_rdumps(s,value)
    #return s.getvalue()[::-1]
    #  This uses a deque to collect output fragments in reverse order.
    #  It's slightly faster than the _rdumps version as the strings
    #  don't need to be reversed when outputting them.
    q = deque()
    _rdumpq(q,0,value)
    return "".join(q)


def _rdumps(s,value):
    """Dump value as a tnetstring, to a StringIO instance, in reverse.

    This function writes out the tnetstring representation of the given value
    to the given StringIO instance, in reverse.  Yes, in reverse.

    Writing in reverse makes it easier to calculate all the length prefixes
    without building every little intermediate string.  Unfortunately it means
    we have to reverse the string for each literal, but it pays off compared
    to the naive version.
    """
    write = s.write
    if value is None:
        write("~:0")
    elif value is True:
        write("!eurt:4")
    elif value is False:
        write("!eslaf:5")
    elif isinstance(value,(int,long)):
        data = str(value) 
        write("#")
        write(data[::-1])
        write(":")
        write(str(len(data))[::-1])
    elif isinstance(value,(float,)):
        data = repr(value) 
        write("#")
        write(data[::-1])
        write(":")
        write(str(len(data))[::-1])
    elif isinstance(value,(str,)):
        write(",")
        write(value[::-1])
        write(":")
        write(str(len(value))[::-1])
    elif isinstance(value,(list,tuple,)):
        write("]")
        i = s.tell()
        for item in reversed(value):
            _rdumps(s,item)
        i = s.tell() - i
        write(":")
        write(str(i)[::-1])
    elif isinstance(value,(dict,)):
        write("}")
        i = s.tell()
        for (k,v) in value.iteritems():
            _rdumps(s,v)
            _rdumps(s,k)
        i = s.tell() - i
        write(":")
        write(str(i)[::-1])
    else:
        raise DumpError("unserializable object")


def _rdumpq(q,size,value):
    """Dump value as a tnetstring, to a deque instance, last chunks first.

    This function generates the tnetstring representation of the given value,
    pushing chunks of the output onto the given deque instance.  It pushes
    the last chunk first, then recursively generates more chunks.

    When passed in the current size of the string in the queue, it will return
    the new size of the string in the queue.

    Operating last-chunk-first makes it easy to calculate the size written
    for recursive structures without having to build their representation as
    a string.  This is measurably faster than the _rdumps version because
    it avoid having to reverse lots of strings.
    """
    write = q.appendleft
    if value is None:
        write("0:~")
        return size + 3
    elif value is True:
        write("4:true!")
        return size + 7
    elif value is False:
        write("5:false!")
        return size + 8
    elif isinstance(value,(int,long)):
        data = str(value) 
        ldata = len(data)
        span = str(ldata)
        write("#")
        write(data)
        write(":")
        write(span)
        return size + 2 + len(span) + ldata
    elif isinstance(value,(float,)):
        data = repr(value) 
        ldata = len(data)
        span = str(ldata)
        write("#")
        write(data)
        write(":")
        write(span)
        return size + 2 + len(span) + ldata
    elif isinstance(value,(str,)):
        lvalue = len(value)
        span = str(lvalue)
        write(",")
        write(value)
        write(":")
        write(span)
        return size + 2 + len(span) + lvalue
    elif isinstance(value,(list,tuple,)):
        write("]")
        i = size = size + 1
        for item in reversed(value):
            size = _rdumpq(q,size,item)
        span = str(size - i)
        write(":")
        write(span)
        return size + 1 + len(span)
    elif isinstance(value,(dict,)):
        write("}")
        i = size = size + 1
        for (k,v) in value.iteritems():
            size = _rdumpq(q,size,v)
            size = _rdumpq(q,size,k)
        span = str(size - i)
        write(":")
        write(span)
        return size + 1 + len(span)
    else:
        raise DumpError("unserializable object")


def _gdumps(value):
    """Generate fragments of value dumped as a tnetstring.

    This is the naive dumping algorithm, implemented as a generator so that
    it's easy to pass to "".join() without building a new list.

    This is mainly here for experimentation purposes; the _rdumps and _rdumpq
    versions are measurably faster.
    """
    if value is None:
        yield "0:~"
    elif value is True:
        yield "4:true!"
    elif value is False:
        yield "5:false!"
    elif isinstance(value,(int,long)):
        data = str(value) 
        yield str(len(data))
        yield ":"
        yield data
        yield "#"
    elif isinstance(value,(float,)):
        data = repr(value) 
        yield str(len(data))
        yield ":"
        yield data
        yield "#"
    elif isinstance(value,(str,)):
        yield str(len(value))
        yield ":"
        yield value
        yield ","
    elif isinstance(value,(list,tuple,)):
        sub = []
        for item in value:
            sub.extend(_gdumps(item))
        sub = "".join(sub)
        yield str(len(sub))
        yield ":"
        yield sub
        yield "]"
    elif isinstance(value,(dict,)):
        sub = []
        for (k,v) in value.iteritems():
            sub.extend(_gdumps(k))
            sub.extend(_gdumps(v))
        sub = "".join(sub)
        yield str(len(sub))
        yield ":"
        yield sub
        yield "}"
    else:
        raise DumpError("unserializable object")


def loads(string):
    """loads(string) -> object

    This function parses a tnetstring into a python object.
    """
    #  No point duplicating effort here.  In the C-extension version,
    #  loads() is measurably faster then pop() since it can avoid
    #  the overhead of building a second string.
    return pop(string)[0]


def pop(string):
    """pop(string) -> (object, remain)

    This function parses a tnetstring into a python object.
    It returns a tuple giving the parsed object and a string
    containing any unparsed data from the end of the string.
    """
    #  Parse out data length, type and remaining string.
    try:
        (dlen,rest) = string.split(":",1)
        dlen = int(dlen)
    except ValueError:
        raise LoadError("not a tnetstring: missing or invalid length prefix")
    try:
        (data,type,remain) = (rest[:dlen],rest[dlen],rest[dlen+1:])
    except IndexError:
        #  This fires if len(rest) < dlen, meaning we don't need
        #  to validate that data is the right length.
        raise LoadError("not a tnetstring: invalid length prefix")
    #  Parse the data based on the type tag.
    if type == ",":
        return (data,remain)
    if type == "#":
        if "." in data or "e" in data or "E" in data:
            try:
                return (float(data),remain)
            except ValueError:
                raise LoadError("not a tnetstring: invalid float literal")
        else:
            try:
                return (int(data),remain)
            except ValueError:
                raise LoadError("not a tnetstring: invalid integer literal")
    if type == "!":
        if data == "true":
            return (True,remain)
        elif data == "false":
            return (False,remain)
        else:
            raise LoadError("not a tnetstring: invalid boolean literal")
    if type == "~":
        if data:
            raise LoadError("not a tnetstring: invalid null literal")
        return (None,remain)
    if type == "]":
        l = []
        while data:
            (item,data) = pop(data)
            l.append(item)
        return (l,remain)
    if type == "}":
        d = {}
        while data:
            (key,data) = pop(data)
            (val,data) = pop(data)
            d[key] = val
        return (d,remain)
    raise LoadError("unknown type tag")



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


