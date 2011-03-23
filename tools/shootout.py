
import cjson
import tnetstring

from tnetstring.tests.test_format import FORMAT_EXAMPLES, get_random_object
for _ in xrange(20):
    v = get_random_object(jsonsafe=True)
    FORMAT_EXAMPLES[tnetstring.dumps(v)] = v

JSON_EXAMPLES = {}
for k,v in FORMAT_EXAMPLES.items():
    JSON_EXAMPLES[cjson.encode(v)] = v

def thrash_tnetstring():
    for data, expect in FORMAT_EXAMPLES.items():
        assert tnetstring.loads(data) == expect
        assert tnetstring.loads(tnetstring.dumps(expect)) == expect

def thrash_cjson():
    for data, expect in JSON_EXAMPLES.items():
        assert cjson.decode(data) == expect
        assert cjson.decode(cjson.encode(expect)) == expect


if __name__ == "__main__":
    import timeit
    t1 = timeit.Timer("thrash_cjson()",
                      "from shootout import thrash_cjson")
    t1 = min(t1.repeat(number=10000))
    print "cjson:", t1
    t2 = timeit.Timer("thrash_tnetstring()",
                      "from shootout import thrash_tnetstring")
    t2 = min(t2.repeat(number=10000))
    print "tnetstring", t2
    print "speedup: ", round((t1 - t2) / (t1) * 100,2), "%"


