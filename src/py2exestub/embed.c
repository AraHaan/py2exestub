/* Minimal main program -- everything is loaded from the library */

#include <Python.h>
#ifdef MS_WINDOWS
#include <Windows.h>
#else
#error "Non-Windows systems are not supported at this time."
#endif

#define DECLARE_MODULE(mod) \
PyObject *PyInit_##mod(void)

#define APPEND_MODULE(mod) \
PyImport_AppendInittab(Py_STRINGIFY(mod), PyInit_##mod)

DECLARE_MODULE(_memimporter);
#if (PY_VERSION_HEX >= 0x030D0000)
DECLARE_MODULE(audioop);
#endif

#if (PY_VERSION_HEX < 0x030A0000)
static int Py_IsNone(PyObject *x) {
    return x == Py_None;
}
#endif

static int __cdecl AddZipExtImportHook() {
  // inject zipextimporter here to allow loading of pyd files from a zip file.
  PyObject *zipextimporter = PyImport_ImportModule("zipextimporter");
  if (zipextimporter == NULL || Py_IsNone(zipextimporter)) {
    Py_XDECREF(zipextimporter);
    return -1;
  }

  PyObject *install_func = PyObject_GetAttrString(zipextimporter, "install");
  if (install_func == NULL || Py_IsNone(install_func)) {
    Py_XDECREF(install_func);
    Py_DECREF(zipextimporter);
    return -1;
  }

  PyObject *result = PyObject_CallNoArgs(install_func);
  if (result == NULL) {
    Py_DECREF(install_func);
    Py_DECREF(zipextimporter);
    return -1;
  }

  Py_DECREF(install_func);
  Py_DECREF(zipextimporter);
  Py_DECREF(result);
  return 0;
}

typedef struct _embed_state {
  wchar_t *argv_copy[5];
  int py_args;
} embed_state;

int
wmain(int argc, wchar_t **argv) {
  int result;
  embed_state *state = (embed_state *)calloc(1, sizeof(embed_state));;
  if (state == NULL) {
    return 1;
  }

  state->argv_copy[0] = argv[0];
  // run in isolated mode
  state->argv_copy[1] = L"-I";
  state->py_args = 2;
  /* to allow importing of pyd extensions from a zip file in memory. */
  APPEND_MODULE(_memimporter);
#if (PY_VERSION_HEX >= 0x030D0000)
  APPEND_MODULE(audioop);
#endif
  Py_Initialize();
  result = AddZipExtImportHook();
  if (result < 0) {
    result = FALSE;
  }

  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      // process each argument for possible values.
      if (wcsicmp(argv[i], L"--console-title") == 0) {
        // console title passed in; set it.
        SetConsoleTitleW(argv[i + 1]);
        // skip next as we used it to set the console title.
        // i++;
      } else if (wcsicmp(argv[i], L"-m") == 0) {
        // the main module to run.
        // If this argument is not provided then the program will run the
        // interactive interpreter.
        state->argv_copy[2] = argv[i];
        state->argv_copy[3] = argv[i + 1];
        state->py_args += 2;
        // skip next as we used it to set the main module to run.
        // i++;
      }
    }
  }

  result = Py_Main(state->py_args, state->argv_copy);
  Py_Finalize();
  free(state);
  return result;
}
