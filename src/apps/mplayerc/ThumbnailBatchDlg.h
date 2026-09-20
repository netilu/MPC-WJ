#pragma once
#include <ExtLib/ui/ResizableLib/ResizableDialog.h>
#include "ThumbnailGenerator.h"
#include "ThumbnailResources.h"
class CMainFrame;
class OpenFileData;

class CThumbnailBatchDlg : public CResizableDialog {
public:
    explicit CThumbnailBatchDlg(CMainFrame* frame);
    ~CThumbnailBatchDlg();
protected:
    BOOL OnInitDialog() override;
    void OnCancel() override;
    void OnOK() override;
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
    afx_msg void OnTimer(UINT_PTR id);
    afx_msg void OnClose();
    DECLARE_MESSAGE_MAP()
private:
    enum class Stage { Idle, Opening, Capturing, Restoring };
    struct Item { CStringW file, output, error; ThumbnailResult result = ThumbnailResult::Pending; };
    CMainFrame* m_frame;
    ThumbnailOptions m_options;
    CListCtrl m_list;
    std::vector<Item> m_items;
    std::unique_ptr<OpenFileData> m_original;
    std::unique_ptr<CThumbnailGenerator> m_generator;
    Stage m_stage = Stage::Idle;
    size_t m_index = 0;
    ULONGLONG m_deadline = 0;
    bool m_cancel = false, m_aborting = false, m_closeAfter = false, m_tick = false;
    REFERENCE_TIME m_position = 0;
    OAFilterState m_playState = State_Stopped;
    int m_volume = 0, m_audio = -1, m_subtitle = -1, m_playlistAudio = -1, m_playlistSubtitle = -1;
    double m_rate = 1.0;
    bool m_subtitlesEnabled = true;
    void AddFiles();
    void AddFile(const CStringW& path);
    void Refresh();
    void EnableSettings(bool enable);
    void FormatChanged();
    bool ReadOptions();
    void ChooseFont(bool timestamp);
    void Next();
    void Restore();
    void Finish(bool restored);
    void AbortOpen();
};
