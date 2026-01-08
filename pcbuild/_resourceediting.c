#undef UNICODE
#undef _UNICODE
#include <stdio.h>
#include <Windows.h>
#include <winuser.h>
#include <Python.h>
#include "resource.h"

#pragma pack(push, 2)
typedef struct _ICONDIR {
  WORD idReserved; // must be 0
  WORD idType; // 1 for icon
  WORD idCount; // number of images
} ICONDIR;

typedef struct _ICONDIRENTRY {
  BYTE bWidth;
  BYTE bHeight;
  BYTE bColorCount;
  BYTE bReserved;
  WORD wPlanes;
  WORD wBitCount;
  DWORD dwBytesInRes;
  DWORD dwImageOffset;
} ICONDIRENTRY;

typedef struct _GRPICONDIR {
  WORD idReserved;
  WORD idType; // 1 for icon
  WORD idCount;
} GRPICONDIR;

typedef struct _GRPICONDIRENTRY {
  BYTE bWidth;
  BYTE bHeight;
  BYTE bColorCount;
  BYTE bReserved;
  WORD wPlanes;
  WORD wBitCount;
  DWORD dwBytesInRes;
  WORD nID; // resource ID of the RT_ICON entry
} GRPICONDIRENTRY;
#pragma pack(pop)

_Success_(return != FALSE) static BOOL ValidateIco(_In_ const BYTE *ico, _In_ size_t size, _In_ WORD firstIconId, _Inout_ const ICONDIR **outDir, _Inout_ const ICONDIRENTRY **outEntries, _Inout_ GRPICONDIR **groupIconDir, _Inout_ size_t *groupIconSize) {
  if (size < sizeof(ICONDIR)) {
    return FALSE;
  }
  
  const ICONDIR *dir = (const ICONDIR *)ico;
  if (dir->idReserved != 0 || dir->idType != 1 || dir->idCount == 0) {
    return FALSE;
  }
  
  size_t entriesSize = dir->idCount * sizeof(ICONDIRENTRY);
  if (size < sizeof(ICONDIR) + entriesSize) {
    return FALSE;
  }
  
  const ICONDIRENTRY *entries = (const ICONDIRENTRY *)(ico + sizeof(ICONDIR));
  // Basic bounds check for each entry’s data
  for (WORD i = 0; i < dir->idCount; ++i) {
    DWORD off = entries[i].dwImageOffset;
    DWORD len = entries[i].dwBytesInRes;
    if (off > size || len > size || off + len > size) {
      return FALSE;
    }
  }

  // Build RT_GROUP_ICON payload
  size_t grpSize = sizeof(GRPICONDIR) + dir->idCount * sizeof(GRPICONDIRENTRY);
  GRPICONDIR *grp = (GRPICONDIR *)malloc(grpSize);
  if (!grp) {
    // EndUpdateResourceW(h, TRUE);
    SetLastError(ERROR_OUTOFMEMORY);
    return FALSE;
  }

  grp->idReserved = 0;
  grp->idType = 1;
  grp->idCount = dir->idCount;
  GRPICONDIRENTRY *gentries = (GRPICONDIRENTRY *)((BYTE *)grp + sizeof(GRPICONDIR));
  for (WORD i = 0; i < dir->idCount; ++i) {
    gentries[i].bWidth = entries[i].bWidth;
    gentries[i].bHeight = entries[i].bHeight;
    gentries[i].bColorCount = entries[i].bColorCount;
    gentries[i].bReserved = entries[i].bReserved;
    gentries[i].wPlanes = entries[i].wPlanes;
    gentries[i].wBitCount = entries[i].wBitCount;
    gentries[i].dwBytesInRes = entries[i].dwBytesInRes;
    gentries[i].nID = (WORD)(firstIconId + i);
  }

  *outDir = dir;
  *outEntries = entries;
  *groupIconDir = grp;
  *groupIconSize = grpSize;
  return TRUE;
}

_Success_(return != 0) static WORD GetCurrentIconCount(_In_ LPCSTR exe_path, _In_ WORD groupId) {
  WORD count = 0;
  HMODULE hMod = LoadLibraryExA(exe_path, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  if (hMod == NULL) {
    return count;
  }

  HRSRC hRsrc = FindResourceA(hMod, MAKEINTRESOURCEA(groupId), RT_GROUP_ICON);
  if (hRsrc == NULL) {
    FreeLibrary(hMod);
    return count;
  }

  HGLOBAL hGlobal = LoadResource(hMod, hRsrc);
  if (hGlobal == NULL) {
    FreeLibrary(hMod);
    return count;
  }
  GRPICONDIR *lpGrpIconDir = (GRPICONDIR *)LockResource(hGlobal);
  if (lpGrpIconDir == NULL) {
    FreeLibrary(hMod);
    return count;
  }

  count = lpGrpIconDir->idCount;
  FreeLibrary(hMod);
  return count;
}

_Success_(return != FALSE) static BOOL ReplaceIconFromIco(
  _In_ LPCSTR exe_path,
  _In_ const BYTE *icoBuf,
  _In_ size_t icoSize,
  _In_ WORD groupId, // e.g., 1
  _In_ WORD firstIconId, // e.g., 2000 (each image will use firstIconId + i)
  _In_ WORD langId // e.g., MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)
) {
  const ICONDIR *dir = NULL;
  const ICONDIRENTRY *entries = NULL;
  GRPICONDIR *grp = NULL;
  size_t grpSize = 0;
  if (!ValidateIco(icoBuf, icoSize, firstIconId, &dir, &entries, &grp, &grpSize)) {
    free(grp);
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }
  
  HANDLE h = BeginUpdateResourceA(exe_path, FALSE);
  if (!h) {
    free(grp);
    return FALSE;
  }
  
  // Remove existing RT_ICON images if the new icon has fewer images
  WORD currentCount = GetCurrentIconCount(exe_path, groupId);
  if (dir->idCount < currentCount) {
    for (WORD i = dir->idCount; i < currentCount; i++) {
      if (!UpdateResourceA(h, RT_ICON, MAKEINTRESOURCEA(firstIconId + i), langId, NULL, 0)) {
        free(grp);
        EndUpdateResourceA(h, TRUE);
        return FALSE;
      }
    }
  }

  // Write each image blob as RT_ICON
  for (WORD i = 0; i < dir->idCount; ++i) {
    const BYTE *img = icoBuf + entries[i].dwImageOffset;
    DWORD imgSize = entries[i].dwBytesInRes;
    if (!UpdateResourceA(h, RT_ICON, MAKEINTRESOURCEA(firstIconId + i), langId, (void *)img, imgSize)) {
      free(grp);
      EndUpdateResourceA(h, TRUE);
      return FALSE;
    }
  }
  
  BOOL ok = UpdateResourceA(h, RT_GROUP_ICON, MAKEINTRESOURCEA(groupId), langId, grp, (DWORD)grpSize);
  free(grp);
  if (!ok) {
    EndUpdateResourceA(h, TRUE);
    return FALSE;
  }

  if (!EndUpdateResourceA(h, FALSE)) {
    return FALSE;
  }

  return TRUE;
}

_Success_(return != FALSE) static BOOL ReplaceResource(_In_ LPCSTR exe_path, _In_ LPCSTR resourceType, _In_ WORD resourceID, _In_ LPVOID data, _In_ DWORD size) {
  HANDLE h = BeginUpdateResourceA(exe_path, FALSE);
  if (!h) {
    printf("BeginUpdateResourceA failed: %lu\n", GetLastError());
    return FALSE;
  }

  if (!UpdateResourceA(h, resourceType, MAKEINTRESOURCEA(resourceID), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), data, size)) {
    printf("UpdateResourceA failed: %lu\n", GetLastError());
    EndUpdateResourceA(h, TRUE);
    return FALSE;
  }

  if (!EndUpdateResourceA(h, FALSE)) {
    printf("EndUpdateResourceA failed: %lu\n", GetLastError());
    return FALSE;
  }

  return TRUE;
}

_Success_(return != 0) static size_t GetStringResourceBufferSize(_In_ WCHAR **strings) {
  size_t len = (size_t)0;
  if (strings == NULL) {
    return len;
  }

  for (int i = 0; i < 16; i++) {
    // Each string starts with a WORD length
    len += sizeof(WORD);
    if (strings[i]) {
      // Each string is stored as: WORD length + WCHAR[length]
      // So total size is sizeof(WORD) + len * sizeof(WCHAR)
      len += wcslen(strings[i]) * sizeof(WCHAR);
    }
  }

  return len;
}

_Success_(return != FALSE) static BOOL ReplaceResources(_In_ LPSTR exe_path, _In_ LPSTR zipFilePath, _In_ LPSTR iconFilePath, _In_ LPSTR consoleTitle, _In_ LPSTR exeArgs, BOOL isStubExe) {
  if (isStubExe == TRUE) {
    if (zipFilePath != NULL && zipFilePath != "") {
      FILE *zipFile = fopen(zipFilePath, "rb");
      if (zipFile == NULL) {
        printf("Failed to open zip file: %s.\n", strerror(errno));
        return FALSE;
      }

      fseek(zipFile, 0, SEEK_END);
      long zipSize = ftell(zipFile);
      fseek(zipFile, 0, SEEK_SET);
      BYTE *zipData = (BYTE *)malloc(zipSize);
      if (zipData == NULL) {
        printf("Failed to allocate memory for ZIP data.\n");
        fclose(zipFile);
        return FALSE;
      }

      fread(zipData, 1, zipSize, zipFile);
      fclose(zipFile);
      if (!ReplaceResource(exe_path, RT_RCDATA, IDR_ZIP1, zipData, (DWORD)zipSize)) {
        free(zipData);
        return FALSE;
      }

      free(zipData);
    }

    // Build block 1 (IDs 0–15)
    WCHAR *strings[16] = { 0 };
    strings[IDS_STRING1] = (WCHAR *)malloc((strlen(consoleTitle) + 1) * sizeof(WCHAR));
    strings[IDS_STRING2] = (WCHAR *)malloc((strlen(exeArgs) + 1) * sizeof(WCHAR));
    // ID 1
    MultiByteToWideChar(CP_UTF8, 0, consoleTitle, -1, strings[IDS_STRING1], (int)(strlen(exeArgs) + 1));
    // ID 2
    MultiByteToWideChar(CP_UTF8, 0, exeArgs, -1, strings[IDS_STRING2], (int)(strlen(exeArgs) + 1));
    BYTE *buffer = (BYTE *)malloc(GetStringResourceBufferSize(strings));
    BYTE *p = buffer;
    for (int i = 0; i < 16; i++) {
      if (strings[i]) {
        WORD len = (WORD)wcslen(strings[i]);
        memcpy(p, &len, sizeof(len));
        p += sizeof(len);
        memcpy(p, strings[i], len * sizeof(WCHAR));
        p += len * sizeof(WCHAR);
      }
      else {
        WORD len = 0;
        memcpy(p, &len, sizeof(len));
        p += sizeof(len);
      }
    }

    DWORD blockSize = (DWORD)(p - buffer);
    BOOL result = ReplaceResource(exe_path, RT_STRING, IDS_STRING1, buffer, blockSize);
    free(strings[IDS_STRING1]);
    free(strings[IDS_STRING2]);
    free(buffer);
    if (!result) {
      return FALSE;
    }
  }

  FILE *iconFile = fopen(iconFilePath, "rb");
  if (iconFile == NULL) {
    printf("Failed to open icon file.\n");
    return FALSE;
  }

  fseek(iconFile, 0, SEEK_END);
  long iconSize = ftell(iconFile);
  fseek(iconFile, 0, SEEK_SET);
  BYTE *iconData = (BYTE *)malloc(iconSize);
  if (iconData == NULL) {
    printf("Failed to allocate memory for icon data.\n");
    fclose(iconFile);
    return FALSE;
  }

  fread(iconData, 1, iconSize, iconFile);
  fclose(iconFile);
  if (!ReplaceIconFromIco(exe_path, iconData, (size_t)iconSize, IDI_ICON1, 1, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))) {
    free(iconData);
    return FALSE;
  }

  free(iconData);
  return TRUE;
}

static PyObject *replace_resources(PyObject *self, PyObject *args)
{
  char *exe_path;
  char *zipFilePath;
  char *iconFilePath;
  char *consoleTitle;
  char *exeArgs;
  BOOL isStubExe;
  if (!PyArg_ParseTuple(args, "zzzzzi:replace_resources",
                        &exe_path,
                        &zipFilePath,
                        &iconFilePath,
                        &consoleTitle,
                        &exeArgs,
                        &isStubExe)) {
    return NULL;
  }

  BOOL result = ReplaceResources(exe_path, zipFilePath, iconFilePath, consoleTitle, exeArgs, isStubExe);
  if (result) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

static PyMethodDef methods[] = {
  { "replace_resources", replace_resources, METH_VARARGS,
    "replace_resources(exe_path: str, zipFilePath: str, iconFilePath: str, consoleTitle: str, exeArgs: str, isStubExe: bool) -> bool" },
  { NULL, NULL },		/* Sentinel */
};

static struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "_resourceediting", /* m_name */
  NULL, /* m_doc */
  -1, /* m_size */
  methods, /* m_methods */
  NULL, /* m_reload */
  NULL, /* m_traverse */
  NULL, /* m_clear */
  NULL, /* m_free */
};

PyMODINIT_FUNC PyInit__resourceediting(void)
{
  return PyModule_Create(&moduledef);
}
