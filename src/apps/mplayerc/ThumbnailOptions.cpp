#include "stdafx.h"
#include "mplayerc.h"
#include "ThumbnailOptions.h"

void ThumbnailOptions::Load()
{
    auto& s = AfxGetAppSettings();
    rows = s.iThumbRows; cols = s.iThumbCols; width = s.iThumbWidth;
    quality = s.iThumbQuality; compression = s.iThumbLevelPNG;
    directory = s.strSnapShotPath; extension = s.strSnapShotExt; subtitles = s.bSnapShotSubtitles;
    auto& p = AfxGetProfile();
    constexpr auto section = L"ThumbnailSheet";
    p.ReadInt(section, L"Rows", rows, 1, 20);
    p.ReadInt(section, L"Columns", cols, 1, 10);
    p.ReadInt(section, L"Width", width, 256, 5120);
    p.ReadInt(section, L"Margin", margin, 0, 100);
    p.ReadInt(section, L"Quality", quality, 70, 100);
    p.ReadInt(section, L"Compression", compression, 1, 9);
    p.ReadBool(section, L"Information", info); p.ReadBool(section, L"Time", time);
    p.ReadBool(section, L"Logo", logo); p.ReadBool(section, L"Subtitles", subtitles);
    p.ReadBool(section, L"BesideVideo", beside);
    p.ReadBool(section, L"HideVideo", hideVideo);
    unsigned color = background;
    p.ReadHex32(section, L"Background", color); background = color & 0xffffff;
    p.ReadString(section, L"Directory", directory); p.ReadString(section, L"Extension", extension);
    p.ReadString(section, L"InfoFont", infoFont); p.ReadString(section, L"TimeFont", timeFont);
    p.ReadInt(section, L"InfoSize", infoSize, 8, 96); p.ReadInt(section, L"TimeSize", timeSize, 8, 96);
    p.ReadInt(section, L"InfoWeight", infoWeight, 100, 900); p.ReadInt(section, L"TimeWeight", timeWeight, 100, 900);
    p.ReadBool(section, L"InfoItalic", infoItalic); p.ReadBool(section, L"TimeItalic", timeItalic);
    if (extension != L".jpg" && extension != L".png" && extension != L".bmp") extension = L".jpg";
    if (directory.IsEmpty() || !PathIsDirectoryW(directory)) {
        PWSTR pictures = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &pictures))) {
            directory = pictures; CoTaskMemFree(pictures);
        }
    }
}

void ThumbnailOptions::Save() const
{
    auto& p = AfxGetProfile();
    constexpr auto section = L"ThumbnailSheet";
    p.WriteInt(section, L"Rows", rows); p.WriteInt(section, L"Columns", cols);
    p.WriteInt(section, L"Width", width); p.WriteInt(section, L"Margin", margin);
    p.WriteInt(section, L"Quality", quality); p.WriteInt(section, L"Compression", compression);
    p.WriteBool(section, L"Information", info); p.WriteBool(section, L"Time", time);
    p.WriteBool(section, L"Logo", logo); p.WriteBool(section, L"Subtitles", subtitles);
    p.WriteBool(section, L"BesideVideo", beside); p.WriteHex32(section, L"Background", background);
    p.WriteBool(section, L"HideVideo", hideVideo);
    p.WriteString(section, L"Directory", directory); p.WriteString(section, L"Extension", extension);
    p.WriteString(section, L"InfoFont", infoFont); p.WriteString(section, L"TimeFont", timeFont);
    p.WriteInt(section, L"InfoSize", infoSize); p.WriteInt(section, L"TimeSize", timeSize);
    p.WriteInt(section, L"InfoWeight", infoWeight); p.WriteInt(section, L"TimeWeight", timeWeight);
    p.WriteBool(section, L"InfoItalic", infoItalic); p.WriteBool(section, L"TimeItalic", timeItalic);
}
