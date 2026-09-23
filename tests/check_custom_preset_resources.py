"""Validate the built preset menus/strings without executing the EXE or DLLs.

Usage: python tests/check_custom_preset_resources.py _bin/mpc-be_x64
"""

import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import sys


kernel = c.WinDLL("kernel32", use_last_error=True)
user = c.WinDLL("user32", use_last_error=True)
kernel.LoadLibraryExW.argtypes = [w.LPCWSTR, w.HANDLE, w.DWORD]
kernel.LoadLibraryExW.restype = w.HMODULE
kernel.FreeLibrary.argtypes = [w.HMODULE]
user.LoadMenuW.argtypes = [w.HINSTANCE, w.LPCWSTR]
user.LoadMenuW.restype = w.HMENU
user.GetMenuItemCount.argtypes = [w.HMENU]
user.GetSubMenu.argtypes = [w.HMENU, c.c_int]
user.GetSubMenu.restype = w.HMENU
user.GetMenuItemID.argtypes = [w.HMENU, c.c_int]
user.GetMenuItemID.restype = w.UINT
user.GetMenuState.argtypes = [w.HMENU, w.UINT, w.UINT]
user.GetMenuState.restype = w.UINT
user.GetMenuStringW.argtypes = [w.HMENU, w.UINT, w.LPWSTR, c.c_int, w.UINT]
user.LoadStringW.argtypes = [w.HINSTANCE, w.UINT, w.LPWSTR, c.c_int]
user.DestroyMenu.argtypes = [w.HMENU]

MF_BYPOSITION = 0x400
MF_SEPARATOR = 0x800


def check_menu(menu, expected_labels):
    ids = [user.GetMenuItemID(menu, i) for i in range(user.GetMenuItemCount(menu))]
    found = 0
    if 827 in ids:
        assert ids == [827, 828, 829, 1204, 0, 1205], ids
        assert user.GetMenuState(menu, 4, MF_BYPOSITION) & MF_SEPARATOR
        for position, expected in zip((3, 5), expected_labels):
            label = c.create_unicode_buffer(256)
            user.GetMenuStringW(menu, position, label, len(label), MF_BYPOSITION)
            assert label.value == expected, (label.value, expected)
        found += 1
    for i in range(len(ids)):
        submenu = user.GetSubMenu(menu, i)
        if submenu:
            found += check_menu(submenu, expected_labels)
    return found


def check_module(path):
    labels = ("C&ustom", "&Save current as custom")
    strings = ("View Custom", "Save current as custom preset", "Custom preset saved")
    if path.name == "mpcresources.sc.dll":
        labels = ("自定义(&U)", "保存当前为自定义(&S)")
        strings = ("自定义视图", "保存当前为自定义预设", "已保存自定义预设")
    elif path.name == "mpcresources.tc.dll":
        labels = ("自訂(&U)", "將目前配置儲存為自訂(&S)")
        strings = ("自訂視窗", "將目前配置儲存為自訂預設", "已儲存自訂預設")
    module = kernel.LoadLibraryExW(str(path.resolve()), None, 0x22)
    assert module, (path, c.get_last_error())
    try:
        for menu_id in (128, 130, 133):
            menu = user.LoadMenuW(module, c.cast(menu_id, w.LPCWSTR))
            assert menu, (path, menu_id)
            try:
                assert check_menu(menu, labels) == 1, (path, menu_id)
            finally:
                user.DestroyMenu(menu)
        for string_id, expected in zip((44105, 44106, 44107), strings):
            value = c.create_unicode_buffer(256)
            assert user.LoadStringW(module, string_id, value, len(value))
            assert value.value == expected, (path, string_id, value.value)
    finally:
        kernel.FreeLibrary(module)


if __name__ == "__main__":
    build = Path(sys.argv[1])
    modules = [build / "mpc-be64.exe", *sorted((build / "Lang").glob("mpcresources.*.dll"))]
    assert len(modules) > 1, "Build the language resources before running this check."
    for module in modules:
        check_module(module)
    print(f"PASS: {len(modules)} modules, {len(modules) * 3} preset menus, all new strings.")
