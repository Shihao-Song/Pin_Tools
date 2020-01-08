#ifndef __PYTHON_HOOKS_HH__
#define __PYTHON_HOOKS_HH__

#include <Python.h>

//Avoid optimizing compilers moving code around this barrier
#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory");}

//These need to be in sync with the simulator
#define ROI_BEGIN         (1025)
#define ROI_END           (1026)

#ifdef __x86_64__
#define HOOKS_STR  "HOOKS"
static inline void magic_op(uint64_t op) {
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
}
#else
#define HOOKS_STR  "NOP-HOOKS"
static inline void magic_op(uint64_t op) {
    //NOP
}
#endif

static PyObject* roi_begin(PyObject* self)
{
    printf("[" HOOKS_STR "] ROI begin\n");
    magic_op(ROI_BEGIN);

    Py_RETURN_NONE;
}

static PyObject* roi_end(PyObject* self)
{
    printf("[" HOOKS_STR "] End ROI\n");
    magic_op(ROI_END);

    Py_RETURN_NONE;
}

static PyMethodDef module_methods[] = {
    {"roi_begin", (PyCFunction) roi_begin, METH_NOARGS, NULL},
    {"roi_end", (PyCFunction) roi_end, METH_NOARGS, NULL},
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef ROI =
{
    PyModuleDef_HEAD_INIT,
    "ROI", /* name of module */
    NULL,
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit_ROI(void)
{
    return PyModule_Create(&ROI);
}

#endif
