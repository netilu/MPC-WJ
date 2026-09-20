"""Read thumbnail dialog resources from an EXE/DLL without executing its code."""
import ctypes as c
from ctypes import wintypes as w
import sys
k = c.WinDLL('kernel32', use_last_error=True)
k.LoadLibraryExW.argtypes = [w.LPCWSTR, w.HANDLE, w.DWORD]
k.LoadLibraryExW.restype = w.HMODULE
k.FindResourceW.argtypes = [w.HMODULE, w.LPCWSTR, w.LPCWSTR]
k.FindResourceW.restype = w.HRSRC
k.LoadResource.argtypes = [w.HMODULE, w.HRSRC]
k.LoadResource.restype = w.HGLOBAL
k.LockResource.argtypes = [w.HGLOBAL]
k.LockResource.restype = c.c_void_p
k.SizeofResource.argtypes = [w.HMODULE, w.HRSRC]
k.SizeofResource.restype = w.DWORD
k.FreeLibrary.argtypes = [w.HMODULE]
u = c.WinDLL('user32', use_last_error=True)
u.LoadStringW.argtypes = [w.HINSTANCE,w.UINT,w.LPWSTR,c.c_int]
u.LoadStringW.restype = c.c_int
for path in sys.argv[1:]:
    module = k.LoadLibraryExW(path, None, 0x22)
    if not module: raise c.WinError(c.get_last_error())
    resource = k.FindResourceW(module, c.cast(57000,w.LPCWSTR),c.cast(5,w.LPCWSTR))
    print(path, 'dialog:', bool(resource))
    version = c.create_unicode_buffer(16)
    u.LoadStringW(module, 57051, version, len(version))
    print('  dialog contract:', version.value or '(missing: use built-in English)')
    if resource:
        data = c.string_at(k.LockResource(k.LoadResource(module,resource)),k.SizeofResource(module,resource))
        print('  HideVideo control:', (57050).to_bytes(4,'little') in data, 'bytes:',len(data))
    k.FreeLibrary(module)
