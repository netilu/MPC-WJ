#pragma once

#include <cstdint>
#include <string>
#include <algorithm>

// Independent of MFC so the allocation and naming rules can be regression tested.
struct ThumbnailLayout {
    int cellWidth = 0, cellHeight = 0, headerHeight = 0, height = 0;
    uint64_t bytes = 0;

    bool Calculate(int width, int rows, int cols, int margin, int arx, int ary,
                   bool info, bool logo, bool time, int infoSize, int timeSize) {
        *this = {};
        if (width < 256 || width > 5120 || rows < 1 || rows > 20 || cols < 1 || cols > 10
            || margin < 0 || margin > 100 || arx <= 0 || ary <= 0
            || infoSize < 8 || infoSize > 96 || timeSize < 8 || timeSize > 96) return false;
        cellWidth = (width - (cols + 1) * margin) / cols;
        if (cellWidth <= 0) return false;
        const int64_t ch = int64_t(cellWidth) * ary / arx;
        if (ch <= 0 || ch > INT32_MAX) return false;
        cellHeight = int(ch);
        if (time && (cellWidth < timeSize * 6 + 8 || cellHeight < timeSize + 8)) return false;
        if (info && width - 2*margin < infoSize * 5) return false;
        headerHeight = (info ? (infoSize + 4) * 4 : 0) + (logo ? 28 : 0);
        const int64_t h = headerHeight + int64_t(margin) * (rows + 1) + ch * rows;
        bytes = uint64_t(width) * h * 4;
        if (h > INT32_MAX || bytes > 512ull * 1024 * 1024) return false;
        height = int(h);
        return true;
    }
};

inline std::wstring ThumbnailName(const std::wstring& file, const std::wstring& ext, unsigned suffix = 0) {
    std::wstring stem = file.substr(file.find_last_of(L"/\\") + 1) + L"_thumbs";
    if (suffix) stem += L" (" + std::to_wstring(suffix) + L")";
    return stem + ext;
}
