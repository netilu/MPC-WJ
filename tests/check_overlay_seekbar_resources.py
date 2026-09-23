"""Validate the OSD controls in compiled resources without executing the modules.

Usage: python tests/check_overlay_seekbar_resources.py _bin/mpc-be_x64
"""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import struct
import sys

k = c.WinDLL("kernel32", use_last_error=True)
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


def dialog_controls(data):
    version, signature, _, _, style, count, _, _, width, height = struct.unpack_from("<HHIIIHhhhh", data)
    assert (version, signature) == (1, 0xffff), "Expected DIALOGEX"
    offset = 26

    def variable():
        nonlocal offset
        word, = struct.unpack_from("<H", data, offset)
        offset += 2
        if word == 0xffff:
            ordinal, = struct.unpack_from("<H", data, offset)
            offset += 2
            return ordinal
        chars = []
        while word:
            chars.append(word)
            word, = struct.unpack_from("<H", data, offset)
            offset += 2
        return "".join(chr(char) for char in chars)

    for _ in range(3):
        variable()  # Menu, window class, title.
    if style & 0x40:  # DS_SETFONT
        offset += 6
        variable()
    controls = []
    for _ in range(count):
        offset = (offset + 3) & ~3
        _, _, control_style, x, y, cx, cy, identifier = struct.unpack_from("<IIIhhhhI", data, offset)
        offset += 24
        control_class, title = variable(), variable()
        extra, = struct.unpack_from("<H", data, offset)
        offset += 2 + extra
        controls.append((identifier, control_class, title, (x, y, cx, cy), control_style))
    return width, height, controls


def check_module(path):
    module = k.LoadLibraryExW(str(path.resolve()), None, 0x22)
    assert module, (path, c.get_last_error())
    try:
        resource = k.FindResourceW(module, c.cast(10107, w.LPCWSTR), c.cast(5, w.LPCWSTR))
        assert resource, path
        data = c.string_at(k.LockResource(k.LoadResource(module, resource)), k.SizeofResource(module, resource))
        width, height, controls = dialog_controls(data)
        label = {
            "mpcresources.sc.dll": "最小模式进度条透明度:",
            "mpcresources.tc.dll": "最小模式進度列透明度:",
        }.get(path.name, "Minimal mode seek bar transparency:")
        assert sum(control[2] == label for control in controls) == 1, path
        for identifier in (22054, 22055):
            matches = [control for control in controls if control[0] == identifier]
            assert len(matches) == 1, (path, identifier)
            _, control_class, title, (x, y, cx, cy), style = matches[0]
            assert x >= 0 and y >= 0 and x + cx <= width and y + cy <= height
            if identifier == 22054:
                assert control_class == "msctls_trackbar32" and style & 0x10000  # WS_TABSTOP
            else:
                assert title == "30%"
    finally:
        k.FreeLibrary(module)


if __name__ == "__main__":
    build = Path(sys.argv[1])
    modules = [build / "mpc-be64.exe", *sorted((build / "Lang").glob("mpcresources.*.dll"))]
    assert len(modules) == 31, "Build the main program and all 30 language resources."
    for module in modules:
        check_module(module)
    print(f"PASS: {len(modules)} OSD dialogs; slider, percentage, translations and control bounds.")
