#include "stdafx.h"
#include "MainFrm.h"
#include "ThumbnailGenerator.h"
#include "ThumbnailResources.h"
#include "DSUtil/ResampleRGB32.h"
#include "DIB.h"

// libpng errors (for example a full output drive) must return to the batch controller.
// The row buffer exists before setjmp so error recovery cannot skip its construction.
static bool SaveThumbnailPNG(LPCWSTR filename, BYTE* dib, int compression)
{
    const auto* header = reinterpret_cast<BITMAPINFOHEADER*>(dib);
    const size_t pitch = size_t(header->biWidth) * 4;
    std::unique_ptr<BYTE[]> row(new(std::nothrow) BYTE[size_t(header->biWidth)*3]);
    if (!row) return false;
    FILE* file = nullptr;
    if (_wfopen_s(&file, filename, L"wb") || !file) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info) {
        if (png) png_destroy_write_struct(&png, nullptr);
        fclose(file); return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info); fclose(file); return false;
    }
    png_init_io(png, file); png_set_bgr(png); png_set_compression_level(png, compression);
    png_set_IHDR(png, info, header->biWidth, header->biHeight, 8, PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = header->biHeight-1; y >= 0; --y) {
        const BYTE* source = dib + sizeof(*header) + size_t(y)*pitch;
        for (int x = 0; x < header->biWidth; ++x) memcpy(row.get()+size_t(x)*3, source+size_t(x)*4, 3);
        png_write_row(png, row.get());
    }
    png_write_end(png, info); png_destroy_write_struct(&png, &info);
    return fclose(file) == 0;
}

CThumbnailGenerator::~CThumbnailGenerator() { Release(); }
void CThumbnailGenerator::Release()
{
    if (m_dc && m_previous) SelectObject(m_dc, m_previous);
    if (m_bitmap) DeleteObject(m_bitmap);
    if (m_dc) DeleteDC(m_dc);
    m_bitmap = nullptr; m_dc = nullptr; m_previous = nullptr; m_pixels = nullptr;
}
void CThumbnailGenerator::Fail(UINT message)
{
    if (m_frame && m_waiting) {
        if (m_frame->m_pFS) m_frame->m_pFS->CancelStep();
        m_frame->m_bFrameSteppingActive = false;
    }
    m_waiting = false; m_result = ThumbnailResult::Failed; m_error = ResStr(message);
}
void CThumbnailGenerator::Cancel()
{
    if (m_result == ThumbnailResult::Running) { Fail(TH_CANCELLED); m_result = ThumbnailResult::Cancelled; }
}
void CThumbnailGenerator::Start(CMainFrame* frame, const CStringW& source, const CStringW& directory, const ThumbnailOptions& options)
{
    Release(); m_frame = frame; m_source = source; m_directory = directory; m_options = options;
    m_index = 0; m_waiting = false; m_error.Empty(); m_result = ThumbnailResult::Running;
    m_size = m_aspect = CSize(0, 0);
    if (!frame->m_pMS || !frame->m_pFS || frame->m_bAudioOnly) { Fail(TH_OPENERROR); return; }
    if (frame->m_pCAP) {
        m_size = frame->m_pCAP->GetVideoSize(); m_aspect = frame->m_pCAP->GetVideoSizeAR();
    } else if (frame->m_pMFVDC) {
        frame->m_pMFVDC->GetNativeVideoSize(&m_size, &m_aspect);
    } else if (frame->m_pBV) {
        frame->m_pBV->GetVideoSize(&m_size.cx, &m_size.cy);
        CComQIPtr<IBasicVideo2> bv = frame->m_pBV;
        if (bv) bv->GetPreferredAspectRatio(&m_aspect.cx, &m_aspect.cy);
    }
    if (m_aspect.cx <= 0 || m_aspect.cy <= 0) m_aspect = m_size;
    frame->m_pMS->GetDuration(&m_duration);
    if (m_duration <= 0 || m_size.cx <= 0 || m_size.cy <= 0) { Fail(TH_OPENERROR); return; }
    if (!m_layout.Calculate(options.width, options.rows, options.cols, options.margin, m_aspect.cx, m_aspect.cy,
        options.info, options.logo, options.time, options.infoSize, options.timeSize)) { Fail(TH_INVALID); return; }
    m_bi = {};
    auto& h = m_bi.bmiHeader;
    h.biSize = sizeof(h); h.biWidth = options.width; h.biHeight = m_layout.height;
    h.biPlanes = 1; h.biBitCount = 32; h.biSizeImage = DWORD(m_layout.bytes);
    m_dc = CreateCompatibleDC(nullptr);
    m_bitmap = CreateDIBSection(m_dc, &m_bi, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_pixels), nullptr, 0);
    if (!m_dc || !m_bitmap) { Fail(IDS_MAINFRM_56); return; }
    m_previous = SelectObject(m_dc, m_bitmap);
    RECT whole = {0, 0, options.width, m_layout.height};
    HBRUSH bg = CreateSolidBrush(options.background); FillRect(m_dc, &whole, bg); DeleteObject(bg); GdiFlush();
    if (options.info) {
        WIN32_FILE_ATTRIBUTE_DATA attr = {}; CStringW size;
        if (GetFileAttributesExW(source, GetFileExInfoStandard, &attr)) {
            WCHAR formatted[80] = {};
            StrFormatByteSizeW((int64_t(attr.nFileSizeHigh) << 32) | attr.nFileSizeLow, formatted, 80); size = formatted;
        }
        CStringW resolution; resolution.Format(L"%d x %d", m_size.cx, m_size.cy);
        const CStringW lines[] = { GetFileName(source), size, resolution, ReftimeToString2(m_duration) };
        for (int i = 0; i < 4; ++i) {
            RECT r = { options.margin, options.margin + i * (options.infoSize + 4),
                       options.width - options.margin, options.margin + (i + 1) * (options.infoSize + 4) };
            Text(lines[i], r, false);
        }
    }
    if (options.logo) {
        RECT r = { options.margin, options.margin + m_layout.headerHeight - 28,
                   options.width - options.margin, options.margin + m_layout.headerHeight };
        const int old = m_options.infoSize; m_options.infoSize = 20; Text(L"MPC-WJ", r, false); m_options.infoSize = old;
    }
    frame->Pause();
    if (frame->m_pBA) frame->m_pBA->put_Volume(-10000);
    frame->m_nVolumeBeforeFrameStepping = -10000;
}
void CThumbnailGenerator::Text(const CStringW& value, RECT rect, bool timestamp)
{
    const auto& o = m_options;
    HFONT font = CreateFontW(-(timestamp ? o.timeSize : o.infoSize), 0, 0, 0,
        timestamp ? o.timeWeight : o.infoWeight, timestamp ? o.timeItalic : o.infoItalic, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,
        timestamp ? o.timeFont.GetString() : o.infoFont.GetString());
    HGDIOBJ previous = SelectObject(m_dc, font);
    SetBkMode(m_dc, timestamp ? OPAQUE : TRANSPARENT); SetBkColor(m_dc, RGB(0,0,0));
    const int luminance = GetRValue(o.background)*299 + GetGValue(o.background)*587 + GetBValue(o.background)*114;
    SetTextColor(m_dc, timestamp || luminance < 128000 ? RGB(255,255,255) : RGB(0,0,0));
    DrawTextW(m_dc, value, value.GetLength(), &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | (timestamp ? DT_RIGHT : DT_LEFT));
    SelectObject(m_dc, previous); DeleteObject(font); GdiFlush();
}
bool CThumbnailGenerator::Capture()
{
    CSimpleBlock<BYTE> dib; CStringW error;
    if (m_frame->GetOriginalFrame(dib, error) != S_OK || dib.Size() < sizeof(BITMAPINFOHEADER)) return false;
    auto* bi = reinterpret_cast<BITMAPINFOHEADER*>(dib.Data());
    if (bi->biBitCount != 32 || bi->biWidth <= 0 || !bi->biHeight || bi->biHeight == INT_MIN) return false;
    if (uint64_t(bi->biWidth) * abs(bi->biHeight) * 4 + sizeof(*bi) > dib.Size()) return false;
    if (m_options.subtitles) m_frame->RenderCurrentSubtitles(dib.Data(), true);
    const int w = m_layout.cellWidth, h = m_layout.cellHeight;
    std::unique_ptr<BYTE[]> scaled(new(std::nothrow) BYTE[size_t(w)*h*4]); if (!scaled) return false;
    CResampleRGB32 resample;
    if (FAILED(resample.SetParameters(w, h, bi->biWidth, abs(bi->biHeight), CResampleRGB32::FILTER_HAMMING, false))
        || FAILED(resample.Process(scaled.get(), reinterpret_cast<BYTE*>(bi+1)))) return false;
    const int x = m_options.margin + (m_index % m_options.cols) * (w + m_options.margin);
    const int y = m_layout.headerHeight + m_options.margin + (m_index / m_options.cols) * (h + m_options.margin);
    for (int row = 0; row < h; ++row) {
        const int srcY = bi->biHeight > 0 ? h-row-1 : row;
        memcpy(m_pixels + (size_t(m_layout.height-y-row-1)*m_options.width+x)*4, scaled.get()+size_t(srcY)*w*4, size_t(w)*4);
    }
    if (m_options.time) {
        REFERENCE_TIME position = 0; m_frame->m_pMS->GetCurrentPosition(&position);
        RECT r = {x+4, y+h-m_options.timeSize-6, x+w-4, y+h-2}; Text(ReftimeToString2(position), r, true);
    }
    return true;
}
bool CThumbnailGenerator::Save()
{
    std::unique_ptr<BYTE[]> dib(new(std::nothrow) BYTE[sizeof(BITMAPINFOHEADER)+size_t(m_layout.bytes)]);
    if (!dib) return false;
    GdiFlush(); memcpy(dib.get(), &m_bi.bmiHeader, sizeof(BITMAPINFOHEADER));
    memcpy(dib.get()+sizeof(BITMAPINFOHEADER), m_pixels, size_t(m_layout.bytes));
    GUID guid; if (FAILED(CoCreateGuid(&guid))) return false;
    WCHAR id[40]; StringFromGUID2(guid, id, 40);
    CStringW temporary = GetCombineFilePath(m_directory, CStringW(L".mpc-thumbs-")+id+m_options.extension);
    HANDLE f = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    CloseHandle(f);
    size_t length = 0;
    const bool encoded = m_options.extension == L".png" ? SaveThumbnailPNG(temporary, dib.get(), m_options.compression)
        : SaveDIB_WIC(temporary, dib.get(), m_options.quality, nullptr, length);
    bool published = false;
    if (encoded) for (unsigned n = 0; n < 100000; ++n) {
        auto name = ThumbnailName(m_source.GetString(), m_options.extension.GetString(), n);
        m_output = GetCombineFilePath(m_directory, name.c_str());
        if (MoveFileExW(temporary, m_output, MOVEFILE_WRITE_THROUGH)) { published = true; break; }
        const auto error = GetLastError(); if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) break;
    }
    if (!published) DeleteFileW(temporary);
    return published;
}
void CThumbnailGenerator::Tick()
{
    if (m_result != ThumbnailResult::Running) return;
    if (m_waiting) {
        if (m_frame->m_bFrameSteppingActive) {
            if (GetTickCount64() >= m_deadline) Fail(TH_FRAMEERROR);
            return;
        }
        m_waiting = false;
        if (!Capture()) { Fail(TH_FRAMEERROR); return; }
        ++m_index;
        // Start the next seek immediately after copying the completed frame.
        // Still yield to the message pump while waiting for EC_STEP_COMPLETE.
    }
    if (!m_waiting) {
        if (m_index == m_options.rows*m_options.cols) {
            if (Save()) m_result = ThumbnailResult::Success; else Fail(TH_WRITEERROR);
            Release(); return;
        }
        REFERENCE_TIME target = (m_duration / (m_options.rows*m_options.cols+1)) * (m_index+1);
        // Batch graphs do not change the playlist/seek bar. Seek against their own duration.
        if (FAILED(m_frame->m_pMS->SetPositions(&target, AM_SEEKING_AbsolutePositioning, nullptr, AM_SEEKING_NoPositioning))) {
            Fail(TH_FRAMEERROR); return;
        }
        m_frame->m_bEndOfStream = false; m_frame->m_bGraphEventComplete = false;
        m_frame->m_bFrameSteppingActive = true;
        m_waiting = true; m_deadline = GetTickCount64()+15000;
        if (FAILED(m_frame->m_pFS->Step(2, nullptr))) Fail(TH_FRAMEERROR);
    }
}
