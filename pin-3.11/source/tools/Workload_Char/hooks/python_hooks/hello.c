#include <Python.h>

static PyObject* Hello(PyObject* self)
{
    return Py_BuildValue("s", "hello, world!");
}

static char Hello_docs[] =
    "usage: toy...\n";

/* deprecated: 
static PyMethodDef uniqueCombinations_funcs[] = {
    {"uniqueCombinations", (PyCFunction)uniqueCombinations, 
     METH_NOARGS, uniqueCombinations_docs},
    {NULL}
};
use instead of the above: */

static PyMethodDef module_methods[] = {
    {"Hello", (PyCFunction) Hello, 
     METH_NOARGS, Hello_docs},
    {NULL}
};


/* deprecated : 
PyMODINIT_FUNC init_uniqueCombinations(void)
{
    Py_InitModule3("uniqueCombinations", uniqueCombinations_funcs,
                   "Extension module uniqueCombinations v. 0.01");
}
*/

static struct PyModuleDef HelloWorld =
{
    PyModuleDef_HEAD_INIT,
    "HelloWorld", /* name of module */
    "Toy",
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit_HelloWorld(void)
{
    return PyModule_Create(&HelloWorld);
}

