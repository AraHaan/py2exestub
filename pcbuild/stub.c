/* stub program for running embedded python from a zip file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <miniz.h>
#include <Windows.h>
#include "resource.h"

#define MAX_LONG_PATH 32767

typedef struct _stub_state {
  wchar_t temp_folder[MAX_LONG_PATH];
  wchar_t exe_path[MAX_LONG_PATH];
  wchar_t exe_args[MAX_PATH];
  wchar_t current_directory[MAX_LONG_PATH];
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  wchar_t file_path[MAX_LONG_PATH];
  LPWSTR console_title;
  LPWSTR program_name;
  wchar_t err_msg[50];
} stub_state;

static int set_folder(stub_state *state) {
  memcpy(state->temp_folder, state->current_directory, MAX_LONG_PATH * sizeof(wchar_t));
  wcsncat(state->temp_folder, state->program_name, __min(wcslen(state->program_name), MAX_LONG_PATH - wcslen(state->current_directory)));
  return EXIT_SUCCESS;
}

static int create_folder(stub_state const *state) {
  if (!CreateDirectoryW(state->temp_folder, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

// Function to extract ZIP file
static int extract_zip(_In_ HINSTANCE hInstance, const void *pMem, DWORD size, const wchar_t *extract_to, stub_state *state) {
  int result = EXIT_SUCCESS;
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));
  if (!mz_zip_reader_init_mem(&zip_archive, pMem, size, 0)) {
    return EXIT_FAILURE;
  }

  size_t num_files = mz_zip_reader_get_num_files(&zip_archive);
  if (num_files == 0) {
    mz_zip_reader_end(&zip_archive);
    // if there are no files, return a failure result.
    return EXIT_FAILURE;
  }

  for (size_t i = 0; i < num_files; i++) {
    if (mz_zip_reader_is_file_a_directory(&zip_archive, (int)i)) {
      continue;
    }

    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, (int)i, &file_stat)) {
      result = EXIT_FAILURE;
      break;
    }

    memset(state->file_path, 0, sizeof(state->file_path));
    _snwprintf_s(state->file_path, MAX_LONG_PATH - 1, MAX_LONG_PATH, L"%s\\%hs", extract_to, file_stat.m_filename);
    FILE *f = _wfopen(state->file_path, L"wbx");
    if (f == NULL) {
      DWORD fileAttr = GetFileAttributesW(state->file_path);
      if (fileAttr == INVALID_FILE_ATTRIBUTES) {
        result = EXIT_FAILURE;
        break;
      }

      // file exists.
      continue;
    }

    if (!mz_zip_reader_extract_to_cfile(&zip_archive, (int)i, f, 0)) {
      fclose(f);
      result = EXIT_FAILURE;
      break;
    }

    fclose(f);
  }

  mz_zip_reader_end(&zip_archive);
  return result;
}

_Success_(return != NULL) static LPVOID __cdecl InternalLoadResource(_In_opt_ HMODULE hModule, _In_ LPCWSTR lpName, _In_ LPCWSTR lpType, _Out_ PDWORD sizeOfResource) {
  HRSRC hRes = FindResourceW(hModule, lpName, lpType);
  if (hRes == NULL) {
    return NULL;
  }

  HGLOBAL hMem = LoadResource(hModule, hRes);
  if (hMem == NULL) {
    return NULL;
  }

  LPVOID lpResource = LockResource(hMem);
  *sizeOfResource = SizeofResource(hModule, hRes);
  if (*sizeOfResource == 0) {
    return NULL;
  }

  return lpResource;
}

_Success_(return != NULL) static LPWSTR __cdecl InternalLoadStringW(_In_opt_ HINSTANCE hInstance, _In_ UINT uID) {
  wchar_t *buf = malloc(100 * sizeof(wchar_t));
  if (buf == NULL) {
    return NULL;
  }

  if (FAILED(LoadStringW(hInstance, uID, buf, 100))) {
    return NULL;
  }

  return buf;
}

static int ErrorMsgBox(_In_ LPCWSTR msg) {
  return MessageBoxW(NULL, msg, L"Error!", MB_ICONERROR | MB_OK);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {
  int result = EXIT_SUCCESS;
  stub_state *state = malloc(sizeof(stub_state));
  if (state == NULL) {
    return EXIT_FAILURE;
  }

  DWORD nChars = GetCurrentDirectoryW(MAX_LONG_PATH, state->current_directory);
  if (nChars >= MAX_LONG_PATH) {
    result = EXIT_FAILURE;
    goto end;
  }

  state->current_directory[nChars] = L'\\';

  // Path to the executable in the temp folder
  state->console_title = InternalLoadStringW(hInstance, IDS_STRING1);
  state->program_name = InternalLoadStringW(hInstance, IDS_STRING2);
  state->si.lpTitle = state->console_title;
  state->si.wShowWindow = SW_SHOW;
  state->si.dwFlags = STARTF_PREVENTPINNING | STARTF_USESHOWWINDOW;
  set_folder(state);
  _snwprintf_s(state->exe_path, MAX_LONG_PATH - 1, MAX_LONG_PATH, L"%s\\%s.exe", state->temp_folder, state->program_name);
  _snwprintf_s(state->exe_args, MAX_PATH - 1, MAX_PATH, L"--console-title \"%s\" -m %s", state->console_title, state->program_name);
  state->si.cb = sizeof(state->si);

  // Create temporary folder
  if (create_folder(state) > 0) {
    result = EXIT_FAILURE;
    goto end;
  }

  DWORD zipSize = 0;
  LPCVOID zip = InternalLoadResource(hInstance, MAKEINTRESOURCEW(IDR_ZIP1), RT_RCDATA, &zipSize);

  // Extract ZIP file to the temporary folder
  if (extract_zip(hInstance, zip, zipSize, state->temp_folder, state) > 0) {
    result = EXIT_FAILURE;
    goto end;
  }

  // Run the executable
  if (!CreateProcessW(state->exe_path, state->exe_args, NULL, NULL, FALSE, 0, NULL, state->current_directory, &state->si, &state->pi)) {
    _snwprintf_s(state->err_msg, (sizeof(state->err_msg) / sizeof(wchar_t)) - 1, sizeof(state->err_msg) / sizeof(wchar_t), L"'CreateProcessW' failed with error code: %u", GetLastError());
    ErrorMsgBox(state->err_msg);
    result = EXIT_FAILURE;
    goto end;
  }

  // Wait for the process to finish
  WaitForSingleObject(state->pi.hProcess, INFINITE);

  // Close process and thread handles
  CloseHandle(state->pi.hProcess);
  CloseHandle(state->pi.hThread);

end:
  free(state->console_title);
  free(state->program_name);
  free(state);
  return result;
}
