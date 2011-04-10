//
//  _tnetstring.c:  python module for fast encode/decode of typed-netstrings
//
//  You get the following functions:
//
//    dumps:  dump a python object to a tnetstring
//    loads:  parse tnetstring into a python object
//    load:   parse tnetstring from a file-like object
//    pop:    parse tnetstring into a python object,
//            return it along with unparsed data.

#include <Python.h>


#define TNS_MAX_LENGTH 999999999
#include "tns_core.c"

static tns_ops _tnetstring_ops;

static PyObject*
_tnetstring_loads(PyObject* self, PyObject *args) 
{
  PyObject *string, *val;
  char *data;
  size_t len;

  if(!PyArg_UnpackTuple(args, "loads", 1, 1, &string)) {
      return NULL;
  }
  if(!PyString_Check(string)) {
      PyErr_SetString(PyExc_TypeError, "arg must be a string");
      return NULL;
  }
  Py_INCREF(string);

  data = PyString_AS_STRING(string);
  len = PyString_GET_SIZE(string);
  val = tns_parse(&_tnetstring_ops, data, len, NULL);
  Py_DECREF(string);
  if(val == NULL) {
      return NULL;
  }

  return val;
}


static PyObject*
_tnetstring_load(PyObject* self, PyObject *args) 
{
  PyObject *val = NULL;
  PyObject *file = NULL;
  PyObject *methnm = NULL;
  PyObject *metharg = NULL;
  PyObject *res = NULL;
  char c, *data;
  size_t datalen = 0;

  //  Grab file-like object as only argument
  if(!PyArg_UnpackTuple(args, "load", 1, 1, &file)) {
      goto error;
  }
  Py_INCREF(file);

  //  We're going to read one char at a time
  if((methnm = PyString_FromString("read")) == NULL) {
      goto error;
  }
  if((metharg = PyInt_FromLong(1)) == NULL) {
      goto error;
  }

  //  Read the length prefix one char at a time
  res = PyObject_CallMethodObjArgs(file, methnm, metharg, NULL);
  if(res == NULL) {
      goto error;
  }
  Py_INCREF(res);
  if(!PyString_Check(res) || !PyString_GET_SIZE(res)) {
      PyErr_SetString(PyExc_ValueError,
                      "Not a tnetstring: invlaid or missing length prefix");
      goto error;
  }
  c = PyString_AS_STRING(res)[0];
  Py_DECREF(res); res = NULL;
  //  Note that the netsring spec explicitly forbids padding zeroes.
  //  If the first char is zero, it must be the only char.
  if(c < '0' || c > '9') {
      PyErr_SetString(PyExc_ValueError,
                      "Not a tnetstring: invlaid or missing length prefix");
      goto error;
  } else if (c == '0') {
      res = PyObject_CallMethodObjArgs(file, methnm, metharg, NULL);
      if(res == NULL) {
          goto error;
      }
      Py_INCREF(res);
      if(!PyString_Check(res) || !PyString_GET_SIZE(res)) {
          PyErr_SetString(PyExc_ValueError,
                      "Not a tnetstring: invlaid or missing length prefix");
          goto error;
      }
      c = PyString_AS_STRING(res)[0];
      Py_DECREF(res); res = NULL;
  } else {
      do {
          datalen = (10 * datalen) + (c - '0');
          check(datalen < TNS_MAX_LENGTH,
                "Not a tnetstring: absurdly large length prefix"); 
          res = PyObject_CallMethodObjArgs(file, methnm, metharg, NULL);
          if(res == NULL) {
              goto error;
          }
          Py_INCREF(res);
          if(!PyString_Check(res) || !PyString_GET_SIZE(res)) {
              PyErr_SetString(PyExc_ValueError,
                        "Not a tnetstring: invlaid or missing length prefix");
              goto error;
          }
          c = PyString_AS_STRING(res)[0];
          Py_DECREF(res); res = NULL;
      } while(c >= '0' && c <= '9');
  }

  //  Validate end-of-length-prefix marker.
  if(c != ':') {
      PyErr_SetString(PyExc_ValueError,
                      "Not a tnetstring: missing length prefix");
      goto error;
  }
  
  //  Read the data plus terminating type tag.
  Py_DECREF(metharg);
  if((metharg = PyInt_FromSize_t(datalen + 1)) == NULL) {
      goto error;
  } 
  res = PyObject_CallMethodObjArgs(file, methnm, metharg, NULL);
  if(res == NULL) {
      goto error;
  }
  Py_INCREF(res);
  Py_DECREF(file); file = NULL;
  Py_DECREF(methnm); methnm = NULL;
  Py_DECREF(metharg); metharg = NULL;
  if(!PyString_Check(res) || PyString_GET_SIZE(res) != datalen + 1) {
      PyErr_SetString(PyExc_ValueError,
                      "Not a tnetstring: invalid length prefix");
      goto error;
  }

  //  Parse out the payload object
  data = PyString_AS_STRING(res);
  val = tns_parse_payload(&_tnetstring_ops, data[datalen], data, datalen);
  Py_DECREF(res); res = NULL;

  return val;

error:
  if(file != NULL) {
      Py_DECREF(file);
  }
  if(methnm != NULL) {
      Py_DECREF(methnm);
  }
  if(metharg != NULL) {
      Py_DECREF(metharg);
  }
  if(res != NULL) {
      Py_DECREF(res);
  }
  if(val != NULL) {
      Py_DECREF(val);
  }
  return NULL;
}


static PyObject*
_tnetstring_pop(PyObject* self, PyObject *args) 
{
  PyObject *string, *val, *rest;
  char *data, *remain;
  size_t len;

  if(!PyArg_UnpackTuple(args, "pop", 1, 1, &string)) {
      return NULL;
  }
  if(!PyString_Check(string)) {
      PyErr_SetString(PyExc_TypeError, "arg must be a string");
      return NULL;
  }
  Py_INCREF(string);

  data = PyString_AS_STRING(string);
  len = PyString_GET_SIZE(string);
  val = tns_parse(&_tnetstring_ops, data, len, &remain);
  Py_DECREF(string);
  if(val == NULL) {
      return NULL;
  }

  rest = PyString_FromStringAndSize(remain, len-(remain-data));
  return PyTuple_Pack(2, val, rest);
}


static PyObject*
_tnetstring_dumps(PyObject* self, PyObject *args, PyObject *kwds) 
{
  PyObject *object, *string;
  tns_outbuf outbuf;

  if(!PyArg_UnpackTuple(args, "dumps", 1, 1, &object)) {
      return NULL;
  }
  Py_INCREF(object);

  
  if(tns_outbuf_init(&outbuf) == -1) {
      Py_DECREF(object);
      return NULL;
  }
  if(tns_render_value(&_tnetstring_ops, object, &outbuf) == -1) {
      Py_DECREF(object);
      return NULL;
  }

  Py_DECREF(object);
  string = PyString_FromStringAndSize(NULL,tns_outbuf_size(&outbuf));
  if(string == NULL) {
      return NULL;
  }

  tns_outbuf_memmove(&outbuf, PyString_AS_STRING(string));
  free(outbuf.buffer);

  return string;
}


static PyMethodDef _tnetstring_methods[] = {
    {"load",
     (PyCFunction)_tnetstring_load,
     METH_VARARGS,
     PyDoc_STR("load(file) -> object\n"
               "This function reads a tnetstring from a file and parses it\n"
               " into a python object.")},

    {"loads",
     (PyCFunction)_tnetstring_loads,
     METH_VARARGS,
     PyDoc_STR("loads(string) -> object\n"
               "This function parses a tnetstring into a python object.")},

    {"pop",
     (PyCFunction)_tnetstring_pop,
     METH_VARARGS,
     PyDoc_STR("pop(string) -> (object, remain)\n"
               "This function parses a tnetstring into a python object.\n"
               "It returns a tuple giving the parsed object and a string\n"
               "containing any unparsed data.")},

    {"dumps",
     (PyCFunction)_tnetstring_dumps,
     METH_VARARGS,
     PyDoc_STR("dumps(object) -> string\n"
               "This function dumps a python object as a tnetstring.")},

    {NULL, NULL}
};


//  Functions to hook the parser core up to python.

static void* tns_parse_string(const char *data, size_t len)
{
  return PyString_FromStringAndSize(data, len);
}


static void* tns_parse_integer(const char *data, size_t len)
{
  long l = 0;
  long long ll = 0;
  int sign = 1;
  char c;
  char *dataend;
  const char *pos, *eod;
  PyObject *v = NULL;

  //  Anything with less than 10 digits, we can fit into a long.
  //  Hand-parsing, as we need tighter error-checking than strtol.
  if (len < 10) {
      pos = data;
      eod = data + len;
      c = *pos++;
      switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          l = c - '0';
          break;
        case '+':
          break;
        case '-':
          sign = -1;
          break;
        default:
          sentinel("invalid integer literal");
      }
      while(pos < eod) {
          c = *pos++;
          check(c >= '0' && c <= '9', "invalid integer literal");
          l = (l * 10) + (c - '0');
      }
      return PyLong_FromLong(l * sign);
  }
  //  Anything with less than 19 digits fits in a long long.
  //  Hand-parsing, as we need tighter error-checking than strtoll.
  else if(len < 19) {
      pos = data;
      eod = data + len;
      c = *pos++;
      switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          ll = c - '0';
          break;
        case '+':
          break;
        case '-':
          sign = -1;
          break;
        default:
          sentinel("invalid integer literal");
      }
      while(pos < eod) {
          c = *pos++;
          check(c >= '0' && c <= '9', "invalid integer literal");
          ll = (ll * 10) + (c - '0');
      }
      return PyLong_FromLongLong(ll * sign);
  }
  //  Really big numbers must be parsed by python.
  //  Technically this allows whitespace around the number, which
  //  isn't valid in a tnetstring.  But I don't want to waste the
  //  time checking and I am *not* reimplementing arbitrary-precision
  //  strtod for python.
  else { 
      v = PyLong_FromString((char *)data, &dataend, 10);
      check(dataend == data + len, "invalid integer literal");
      return v;
  }
  sentinel("invalid code branch, check your compiler...");

error:
  return NULL;
}


static void* tns_parse_float(const char *data, size_t len)
{
  double d = 0;
  char *dataend;

  //  Technically this allows whitespace around the float, which
  //  isn't valid in a tnetstring.  But I don't want to waste the
  //  time checking and I am *not* reimplementing strtod.
  d = strtod(data, &dataend);
  if(dataend != data + len) {
      return NULL;
  }
  return PyFloat_FromDouble(d);
}


static void* tns_get_null(void)
{
  Py_INCREF(Py_None);
  return Py_None;
}


static void* tns_get_true(void)
{
  Py_INCREF(Py_True);
  return Py_True;
}


static void* tns_get_false(void)
{
  Py_INCREF(Py_False);
  return Py_False;
}


static void* tns_new_dict(void)
{
  return PyDict_New();
}


static void* tns_new_list(void)
{
  return PyList_New(0);
}


static void tns_free_value(void *value)
{
  Py_XDECREF(value);
}


static int tns_add_to_dict(void *dict, void *key, void *item)
{
  int res;
  res = PyDict_SetItem(dict, key, item);
  Py_DECREF(key);
  Py_DECREF(item);
  if(res == -1) {
      return -1;
  }
  return 0;
}


static int tns_add_to_list(void *list, void *item)
{
  int res;
  res = PyList_Append(list, item);
  Py_DECREF(item);
  if(res == -1) {
      return -1;
  }
  return 0;
}


static int tns_render_string(void *val, tns_outbuf *outbuf)
{
    return tns_outbuf_puts(outbuf, PyString_AS_STRING(val),
                                   PyString_GET_SIZE(val));
}


static int tns_render_integer(void *val, tns_outbuf *outbuf)
{
  PyObject *string = NULL;
  int res = 0;

  string = PyObject_Str(val);
  if(string == NULL) {
      return -1;
  }

  res = tns_render_string(string, outbuf);
  Py_DECREF(string);
  return res;
}


static int tns_render_float(void *val, tns_outbuf *outbuf)
{
  PyObject *string;
  int res = 0;

  string = PyObject_Repr(val);
  if(string == NULL) {
      return -1;
  }

  res = tns_render_string(string, outbuf);
  Py_DECREF(string);
  return res;
}


static int tns_render_bool(void *val, tns_outbuf *outbuf)
{
  if(val == Py_True) {
      return tns_outbuf_puts(outbuf, "true", 4);
  } else {
      return tns_outbuf_puts(outbuf, "false", 5);
  }
}


static int tns_render_dict(const tns_ops *ops, void *val, tns_outbuf *outbuf)
{
  PyObject *key, *item;
  Py_ssize_t pos = 0;

  while(PyDict_Next(val, &pos, &key, &item)) {
      if(tns_render_value(ops, item, outbuf) == -1) {
          return -1;
      }
      if(tns_render_value(ops, key, outbuf) == -1) {
          return -1;
      }
  }
  return 0;
}


static int tns_render_list(const tns_ops *ops, void *val, tns_outbuf *outbuf)
{
  PyObject *item;
  Py_ssize_t idx;

  //  Remember, all output is in reverse.
  //  So we must write the last element first.
  idx = PyList_GET_SIZE(val) - 1;
  while(idx >= 0) {
      item = PyList_GET_ITEM(val, idx);
      if(tns_render_value(ops, item, outbuf) == -1) {
          return -1;
      }
      idx--;
  }
  return 0;
}


static tns_type_tag tns_get_type(void *val)
{
  if(val == Py_True || val == Py_False) {
    return tns_tag_bool;
  }
  if(val == Py_None) {
    return tns_tag_null;
  }
  if(PyInt_Check((PyObject*)val) || PyLong_Check((PyObject*)val)) {
    return tns_tag_integer;
  }
  if(PyFloat_Check((PyObject*)val)) {
    return tns_tag_float;
  }
  if(PyString_Check((PyObject*)val)) {
    return tns_tag_string;
  }
  if(PyList_Check((PyObject*)val)) {
    return tns_tag_list;
  }
  if(PyDict_Check((PyObject*)val)) {
    return tns_tag_dict;
  }
  return 0;
}


PyDoc_STRVAR(module_doc,
"Fast encoding/decoding of typed-netstrings."
);


PyMODINIT_FUNC
init_tnetstring(void)
{
  Py_InitModule3("_tnetstring", _tnetstring_methods, module_doc);

  _tnetstring_ops.get_type = &tns_get_type;
  _tnetstring_ops.free_value = &tns_free_value;

  _tnetstring_ops.parse_string = tns_parse_string;
  _tnetstring_ops.parse_integer = tns_parse_integer;
  _tnetstring_ops.parse_float = tns_parse_float;
  _tnetstring_ops.get_null = tns_get_null;
  _tnetstring_ops.get_true = tns_get_true;
  _tnetstring_ops.get_false = tns_get_false;

  _tnetstring_ops.render_string = tns_render_string;
  _tnetstring_ops.render_integer = tns_render_integer;
  _tnetstring_ops.render_float = tns_render_float;
  _tnetstring_ops.render_bool = tns_render_bool;

  _tnetstring_ops.new_dict = tns_new_dict;
  _tnetstring_ops.add_to_dict = tns_add_to_dict;
  _tnetstring_ops.render_dict = tns_render_dict;

  _tnetstring_ops.new_list = tns_new_list;
  _tnetstring_ops.add_to_list = tns_add_to_list;
  _tnetstring_ops.render_list = tns_render_list;
}

