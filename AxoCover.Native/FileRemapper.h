// AxoCover.Runner.Native.h

#pragma once

#pragma unmanaged
LPCWSTR* _mappedPaths;
LPCWSTR _root;
BOOL _excludeNonexistentDirectories = false;
BOOL _excludeNonexistentFiles = false;
BOOL _includeBaseDirectory = false;

BOOL FileExists(LPCWSTR filePath);
BOOL DirectoryExists(LPCWSTR dirPath);

void MapFile(LPCWSTR &filePath)
{
  if (filePath)
  {
    WCHAR dir[MAX_PATH];
    memset(dir, 0, sizeof(WCHAR) * MAX_PATH);
    auto fileName = wcsrchr(filePath, '\\');
    if (!fileName) return;

    memcpy(dir, filePath, (BYTE*)fileName - (BYTE*)filePath);

    if ((_excludeNonexistentDirectories && !DirectoryExists(dir)) ||
      (
        (_excludeNonexistentFiles && !FileExists(filePath)) &&
        (!_includeBaseDirectory || _wcsicmp(dir, _root))
      ))
      return;

    for (auto mappedPath = &_mappedPaths[0]; *mappedPath != nullptr; mappedPath++)
    {
      auto mappedName = wcsrchr(*mappedPath, '\\');
      if (_wcsicmp(fileName, mappedName) == 0 && _wcsicmp(filePath, *mappedPath) != 0)
      {
        filePath = *mappedPath;
        break;
      }
    }
  }
}

HANDLE WINAPI OnCreateFileW(
  LPCWSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile)
{
  MapFile(lpFileName);

  auto result = CreateFileW(
    lpFileName,
    dwDesiredAccess,
    dwShareMode,
    lpSecurityAttributes,
    dwCreationDisposition,
    dwFlagsAndAttributes,
    hTemplateFile);

  return result;
}

DWORD WINAPI OnGetFileAttributesW(
  LPCWSTR lpFileName)
{
  MapFile(lpFileName);

  auto result = GetFileAttributesW(
    lpFileName);

  return result;
}

BOOL WINAPI OnGetFileAttributesExW(
  LPCWSTR lpFileName,
  GET_FILEEX_INFO_LEVELS fInfoLevelId,
  LPVOID lpFileInformation)
{
  MapFile(lpFileName);

  auto result = GetFileAttributesExW(
    lpFileName,
    fInfoLevelId,
    lpFileInformation);

  return result;
}

BOOL FileExists(LPCWSTR filePath)
{
  DWORD attribs = GetFileAttributesW(filePath);
  if (attribs == INVALID_FILE_ATTRIBUTES)
  {
    return false;
  }
  else
  {
    return !(attribs & FILE_ATTRIBUTE_DIRECTORY);
  }
}

BOOL DirectoryExists(LPCWSTR dirPath)
{
  auto attribs = GetFileAttributesW(dirPath);
  if (attribs == INVALID_FILE_ATTRIBUTES)
  {
    return false;
  }
  else
  {
    return (attribs & FILE_ATTRIBUTE_DIRECTORY);
  }
}

#pragma managed
using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Collections::Concurrent;
using namespace System::Runtime::InteropServices;
using namespace System::Reflection;

namespace AxoCover
{
  namespace Native
  {
    public ref class FileRemapper
    {
    private:
      static bool _isHooking = false;
      static bool _isSucceded = true;
    public:
      static property Boolean ExcludeNonexistentDirectories
      {
        Boolean get()
        {
          return !!_excludeNonexistentDirectories;
        }
        void set(Boolean value)
        {
          _excludeNonexistentDirectories = value;
        }
      }

      static property Boolean ExcludeNonexistentFiles
      {
        Boolean get()
        {
          return !!_excludeNonexistentFiles;
        }
        void set(Boolean value)
        {
          _excludeNonexistentFiles = value;
        }
      }

      static property Boolean IncludeBaseDirectory
      {
        Boolean get()
        {
          return !!_includeBaseDirectory;
        }
        void set(Boolean value)
        {
          _includeBaseDirectory = value;
        }
      }

      static Boolean TryRedirectFiles(IList<String^>^ mappedFiles)
      {
        auto root = Path::GetDirectoryName(Assembly::GetEntryAssembly()->Location);
        _root = (LPCWSTR)Marshal::StringToHGlobalUni(root).ToPointer();

        auto mappedPaths = new LPCWSTR[mappedFiles->Count + 1];
        auto mappedPath = &mappedPaths[0];
        for each (String^ mapping in mappedFiles)
        {
          *mappedPath++ = (LPCWSTR)Marshal::StringToHGlobalUni(mapping).ToPointer();
        }
        *mappedPath++ = nullptr;
        _mappedPaths = mappedPaths;
        
        if (!_isHooking)
        {
          _isHooking = true;
          _isSucceded &= TryHook(L"Kernel32.dll", "CreateFileW", OnCreateFileW);
          _isSucceded &= TryHook(L"Kernel32.dll", "GetFileAttributesW", OnGetFileAttributesW);
          _isSucceded &= TryHook(L"Kernel32.dll", "GetFileAttributesExW", OnGetFileAttributesExW);
        }

        return _isSucceded;
      }

      template <typename TCallback>
      static Boolean TryHook(LPCWSTR moduleName, LPCSTR procName, TCallback hook)
      {
        auto moduleHandle = GetModuleHandle(moduleName);
        auto procAddress = GetProcAddress(moduleHandle, procName);

        HOOK_TRACE_INFO hookHandle = { 0 };
        NTSTATUS result = LhInstallHook(procAddress,
          hook,
          NULL,
          &hookHandle);
        if (FAILED(result))
        {
          return false;
        }

        ULONG threads[] = { 0 };
        result = LhSetExclusiveACL(threads, 0, &hookHandle);
        if (FAILED(result))
        {
          return false;
        }

        return true;
      }
    };
  }
}
