"""

tnetstring:  data serialization using typed netstrings
======================================================


This is a data serialization library. It's a lot like JSON but it uses a
new syntax called "typed netstrings" that Zed has proposed for use in the
Mongrel2 webserver.  It's designed to be simpler and easier to implement
than JSON, with a happy consequence of also being faster in many cases.

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
functions.  They're only here so you can use load() to read precisely one
item from a file or socket without consuming any extra data.

"""

__ver_major__ = 0
__ver_minor__ = 1
__ver_patch__ = 0
__ver_sub__ = ""
__version__ = "%d.%d.%d%s" % (__ver_major__,__ver_minor__,__ver_patch__,__ver_sub__)


from collections import deque


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
    #  This uses a deque to collect output fragments in reverse order,
    #  then joins them together at the end.  It's measurably faster
    #  than creating all the intermediate strings.
    #  If you're reading this to get a handle on the tnetstring format,
    #  consider the _gdumps() function instead; it's a standard top-down
    #  generator that's simpler to understand but much less efficient.
    q = deque()
    _rdumpq(q,0,value)
    return "".join(q)


def dump(value, file):
    """dump(object, file)

    This function dumps a python object as a tnetstring and writes it to
    the given file.
    """
    file.write(dumps(value))


def _rdumpq(q,size,value):
    """Dump value as a tnetstring, to a deque instance, last chunks first.

    This function generates the tnetstring representation of the given value,
    pushing chunks of the output onto the given deque instance.  It pushes
    the last chunk first, then recursively generates more chunks.

    When passed in the current size of the string in the queue, it will return
    the new size of the string in the queue.

    Operating last-chunk-first makes it easy to calculate the size written
    for recursive structures without having to build their representation as
    a string.  This is measurably faster than generating the intermediate
    strings, especially on deeply nested structures.
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
        #  Use repr() for float rather than str().
        #  It round-trips more accurately.
        #  Probably unnecessary in later python versions that
        #  use David Gay's ftoa routines.
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
        init_size = size = size + 1
        for item in reversed(value):
            size = _rdumpq(q,size,item)
        span = str(size - init_size)
        write(":")
        write(span)
        return size + 1 + len(span)
    elif isinstance(value,(dict,)):
        write("}")
        init_size = size = size + 1
        for (k,v) in value.iteritems():
            size = _rdumpq(q,size,v)
            size = _rdumpq(q,size,k)
        span = str(size - init_size)
        write(":")
        write(span)
        return size + 1 + len(span)
    else:
        raise DumpError("unserializable object")


def _gdumps(value):
    """Generate fragments of value dumped as a tnetstring.

    This is the naive dumping algorithm, implemented as a generator so that
    it's easy to pass to "".join() without building a new list.

    This is mainly here for comparison purposes; the _rdumpq version is
    measurably faster as it doesn't have to build intermediate strins.
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


def load(file):
    """load(file) -> object

    This function reads a tnetstring from a file and parses it into a
    python object.  The file must support the read() method, and this
    function promises not to read more data than necessary.
    """
    #  Read the length prefix one char at a time.
    c = file.read(1)
    if not c.isdigit():
        raise LoadError("not a tnetstring: missing or invalid length prefix")
    datalen = ord(c) - ord("0")
    c = file.read(1)
    while c.isdigit():
        datalen = (10 * datalen) + (ord(c) - ord("0"))
        c = file.read(1)
    if c != ":":
        raise LoadError("not a tnetstring: missing or invalid length prefix")
    #  Now we can read and parse the payload.
    #  This repeats the dispatch logic of pop() so we can avoid
    #  re-constructing the outermost tnetstring.
    data = file.read(datalen)
    if len(data) != datalen:
        raise LoadError("not a tnetstring: length prefix too big")
    type = file.read(1)
    if type == ",":
        return data
    if type == "#":
        if "." in data or "e" in data or "E" in data:
            try:
                return float(data)
            except ValueError:
                raise LoadError("not a tnetstring: invalid float literal")
        else:
            try:
                return int(data)
            except ValueError:
                raise LoadError("not a tnetstring: invalid integer literal")
    if type == "!":
        if data == "true":
            return True
        elif data == "false":
            return False
        else:
            raise LoadError("not a tnetstring: invalid boolean literal")
    if type == "~":
        if data:
            raise LoadError("not a tnetstring: invalid null literal")
        return None
    if type == "]":
        l = []
        while data:
            (item,data) = pop(data)
            l.append(item)
        return l
    if type == "}":
        d = {}
        while data:
            (key,data) = pop(data)
            (val,data) = pop(data)
            d[key] = val
        return d
    raise LoadError("unknown type tag")
    


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
        #  to further validate that data is the right length.
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
    dumps = _tnetstring.dumps
    load = _tnetstring.load
    loads = _tnetstring.loads
    pop = _tnetstring.pop


