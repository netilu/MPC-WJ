#pragma once
#include "ThumbnailOptions.h"
class CMainFrame;

// Advanced on the UI thread, leaving the message pump free to deliver frame events.
class CThumbnailGenerator {
public:
    ~CThumbnailGenerator();
    void Start(CMainFrame* frame, const CStringW& source, const CStringW& directory, const ThumbnailOptions& options);
    void Tick();
    void Cancel();
    ThumbnailResult Result() const { return m_result; }
    const CStringW& Error() const { return m_error; }
    const CStringW& Output() const { return m_output; }
    int Progress() const { return m_index; }
private:
    CMainFrame* m_frame = nullptr;
    ThumbnailOptions m_options;
    ThumbnailLayout m_layout;
    ThumbnailResult m_result = ThumbnailResult::Pending;
    CStringW m_source, m_directory, m_output, m_error;
    CSize m_size, m_aspect;
    REFERENCE_TIME m_duration = 0;
    int m_index = 0;
    bool m_waiting = false;
    ULONGLONG m_deadline = 0;
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_previous = nullptr;
    BYTE* m_pixels = nullptr;
    BITMAPINFO m_bi = {};
    void Fail(UINT message);
    void Release();
    void Text(const CStringW& value, RECT rect, bool timestamp);
    bool Capture();
    bool Save();
};
