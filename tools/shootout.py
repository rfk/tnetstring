
import cjson
import _tnetstring


TESTS = {
    '0:}': {},
    '0:]': [],
    '51:5:hello,39:11:12345678901#4:this,4:true!0:~4:\x00\x00\x00\x00,]}':
            {'hello': [12345678901, 'this', True, None, '\x00\x00\x00\x00']},
    '5:12345#': 12345,
    '12:this is cool,': "this is cool",
    '0:,': "",
    '0:~': None,
    '4:true!': True,
    '5:false!': False,
    '10:\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00,': "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
    '24:5:12345#5:67890#5:xxxxx,]': [12345, 67890, 'xxxxx'],
    '243:238:233:228:223:218:213:208:203:198:193:188:183:178:173:168:163:158:153:148:143:138:133:128:123:118:113:108:103:99:95:91:87:83:79:75:71:67:63:59:55:51:47:43:39:35:31:27:23:19:15:11:hello-there,]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]': [[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["hello-there"]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
}

JSON_TESTS = {}

for k,v in TESTS.items():
    JSON_TESTS[cjson.encode(v)] = v

def thrash_tnetstring():
    for data, expect in TESTS.items():
        assert _tnetstring.parse_tnetstring(data) == expect
        assert _tnetstring.parse_tnetstring(_tnetstring.dump_tnetstring(expect)) == expect

def thrash_cjson():
    for data, expect in JSON_TESTS.items():
        assert cjson.decode(data) == expect
        assert cjson.decode(cjson.encode(expect)) == expect

if __name__ == "__main__":
    import timeit
    t1 = timeit.Timer("thrash_cjson()",
                      "from shootout import thrash_cjson")
    t1 = min(t1.repeat(number=100000))
    print "cjson:", t1
    t2 = timeit.Timer("thrash_tnetstring()",
                      "from shootout import thrash_tnetstring")
    t2 = min(t2.repeat(number=100000))
    print "_tnetstring", t2
    print "speedup: ", round((t1 - t2) / (t1) * 100,2), "%"


