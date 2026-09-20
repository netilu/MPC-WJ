#pragma once
#include "ThumbnailLayout.h"

struct ThumbnailOptions {
    int rows = 4, cols = 4, width = 1024, margin = 10, quality = 85, compression = 7;
    bool info = true, time = true, logo = true, subtitles = false, beside = false;
    bool hideVideo = false;
    COLORREF background = RGB(255, 255, 255);
    CStringW directory, extension = L".jpg";
    CStringW infoFont = L"Segoe UI", timeFont = L"Segoe UI";
    int infoSize = 20, timeSize = 16, infoWeight = FW_NORMAL, timeWeight = FW_NORMAL;
    bool infoItalic = false, timeItalic = false;
    void Load();
    void Save() const;
};

enum class ThumbnailResult { Pending, Running, Success, Failed, Cancelled };
