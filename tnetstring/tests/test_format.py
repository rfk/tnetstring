
import sys
import unittest
import random
import StringIO


import tnetstring


FORMAT_EXAMPLES = {
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
    '18:3:0.1^3:0.2^3:0.3^]': [0.1, 0.2, 0.3],
    '243:238:233:228:223:218:213:208:203:198:193:188:183:178:173:168:163:158:153:148:143:138:133:128:123:118:113:108:103:99:95:91:87:83:79:75:71:67:63:59:55:51:47:43:39:35:31:27:23:19:15:11:hello-there,]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]': [[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["hello-there"]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
}


def get_random_object(random=random,depth=0,unicode=False):
    """Generate a random serializable object."""
    #  The probability of generating a scalar value increases as the depth increase.
    #  This ensures that we bottom out eventually.
    if random.randint(depth,10) <= 4:
        what = random.randint(0,1)
        if what == 0:
            n = random.randint(0,10)
            l = []
            for _ in xrange(n):
                l.append(get_random_object(random,depth+1,unicode))
            return l
        if what == 1:
            n = random.randint(0,10)
            d = {}
            for _ in xrange(n):
               n = random.randint(0,100)
               k = "".join(chr(random.randint(32,126)) for _ in xrange(n))
               if unicode:
                   k = k.decode("ascii")
               d[k] = get_random_object(random,depth+1,unicode)
            return d
    else:
        what = random.randint(0,4)
        if what == 0:
            return None
        if what == 1:
            return True
        if what == 2:
            return False
        if what == 3:
            if random.randint(0,1) == 0:
                return random.randint(0,sys.maxint)
            else:
                return -1 * random.randint(0,sys.maxint)
        n = random.randint(0,100)
        if unicode:
            return u"".join(chr(random.randint(32,126)) for _ in xrange(n))



class Test_Format(unittest.TestCase):

    def test_roundtrip_format_examples(self):
        for data, expect in FORMAT_EXAMPLES.items():
            self.assertEqual(expect,tnetstring.loads(data))
            self.assertEqual(expect,tnetstring.loads(tnetstring.dumps(expect)))
            self.assertEqual((expect,""),tnetstring.pop(data))

    def test_roundtrip_format_random(self):
        for _ in xrange(500):
            v = get_random_object()
            self.assertEqual(v,tnetstring.loads(tnetstring.dumps(v)))
            self.assertEqual((v,""),tnetstring.pop(tnetstring.dumps(v)))

    def test_unicode_handling(self):
        self.assertRaises(ValueError,tnetstring.dumps,u"hello")
        self.assertEquals(tnetstring.dumps(u"hello","utf8"),"5:hello,")
        self.assertEquals(type(tnetstring.loads("5:hello,")),str)
        self.assertEquals(type(tnetstring.loads("5:hello,","utf8")),unicode)
        ALPHA = u"\N{GREEK CAPITAL LETTER ALPHA}lpha"
        self.assertEquals(tnetstring.dumps(ALPHA,"utf8"),"6:"+ALPHA.encode("utf8")+",")
        self.assertEquals(tnetstring.dumps(ALPHA,"utf16"),"12:"+ALPHA.encode("utf16")+",")
        self.assertEquals(tnetstring.loads("12:\xff\xfe\x91\x03l\x00p\x00h\x00a\x00,","utf16"),ALPHA)

    def test_roundtrip_format_unicode(self):
        for _ in xrange(500):
            v = get_random_object(unicode=True)
            self.assertEqual(v,tnetstring.loads(tnetstring.dumps(v,"utf8"),"utf8"))
            self.assertEqual((v,""),tnetstring.pop(tnetstring.dumps(v,"utf16"),"utf16"))


class Test_FileLoading(unittest.TestCase):

    def test_roundtrip_file_examples(self):
        for data, expect in FORMAT_EXAMPLES.items():
            s = StringIO.StringIO()
            s.write(data)
            s.write("OK")
            s.seek(0)
            self.assertEqual(expect,tnetstring.load(s))
            self.assertEqual("OK",s.read())
            s = StringIO.StringIO()
            tnetstring.dump(expect,s)
            s.write("OK")
            s.seek(0)
            self.assertEqual(expect,tnetstring.load(s))
            self.assertEqual("OK",s.read())

    def test_roundtrip_file_random(self):
        for _ in xrange(500):
            v = get_random_object()
            s = StringIO.StringIO()
            tnetstring.dump(v,s)
            s.write("OK")
            s.seek(0)
            self.assertEqual(v,tnetstring.load(s))
            self.assertEqual("OK",s.read())

    def test_error_on_absurd_lengths(self):
        s = StringIO.StringIO()
        s.write("1000000000:pwned!,")
        s.seek(0)
        self.assertRaises(ValueError,tnetstring.load,s)
        self.assertEquals(s.read(1),":")

