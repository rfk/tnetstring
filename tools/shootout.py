
import random

import cjson
import yajl
import ujson
import tnetstring
import marshal

from tnetstring.tests.test_format import FORMAT_EXAMPLES, get_random_object

TESTS = []
def add_test(v):
    #  These modules have a few round-tripping problems...
    try:
        assert cjson.decode(cjson.encode(v)) == v
        assert yajl.loads(yajl.dumps(v)) == v
        assert ujson.loads(ujson.dumps(v)) == v
    except Exception:
        pass
    else:
        TESTS.append((v,tnetstring.dumps(v),cjson.encode(v),marshal.dumps(v)))

for (k,v) in FORMAT_EXAMPLES.iteritems():
    add_test(v)
for _ in xrange(20):
    v = get_random_object(random.Random(0),jsonsafe=True)
    add_test(v)

def thrash_tnetstring():
    for obj, tns, json, msh in TESTS:
#        tnetstring.dumps(obj)
        assert tnetstring.loads(tns) == obj
#        assert tnetstring.loads(tnetstring.dumps(obj)) == obj

def thrash_cjson():
    for obj, tns, json, msh in TESTS:
#        cjson.encode(obj)
        assert cjson.decode(json) == obj
#        assert cjson.decode(cjson.encode(obj)) == obj

def thrash_yajl():
    for obj, tns, json, msh in TESTS:
#        yajl.dumps(obj)
        assert yajl.loads(json) == obj
#        assert yajl.loads(yajl.dumps(obj)) == obj

def thrash_ujson():
    for obj, tns, json, msh in TESTS:
#        ujson.dumps(obj)
        assert ujson.loads(json) == obj
#        assert ujson.loads(ujson.dumps(obj)) == obj

def thrash_marshal():
    for obj, tns, json, msh in TESTS:
#        marshal.dumps(obj)
        assert marshal.loads(msh) == obj
#        assert marshal.loads(marshal.dumps(obj)) == obj


if __name__ == "__main__":
    import timeit
    t1 = timeit.Timer("thrash_tnetstring()",
                      "from shootout import thrash_tnetstring")
    t1 = min(t1.repeat(number=10000))
    print "tnetstring", t1

    t2 = timeit.Timer("thrash_cjson()",
                      "from shootout import thrash_cjson")
    t2 = min(t2.repeat(number=10000))
    print "cjson:", t2
    print "speedup: ", round((t2 - t1) / (t2) * 100,2), "%"

    t3 = timeit.Timer("thrash_yajl()",
                      "from shootout import thrash_yajl")
    t3 = min(t3.repeat(number=10000))
    print "yajl:", t3
    print "speedup: ", round((t3 - t1) / (t3) * 100,2), "%"

    t4 = timeit.Timer("thrash_ujson()",
                      "from shootout import thrash_ujson")
    t4 = min(t4.repeat(number=10000))
    print "ujson:", t4
    print "speedup: ", round((t4 - t1) / (t4) * 100,2), "%"

    t5 = timeit.Timer("thrash_marshal()",
                      "from shootout import thrash_marshal")
    t5 = min(t5.repeat(number=10000))
    print "marshal:", t5
    print "speedup: ", round((t5 - t1) / (t5) * 100,2), "%"


