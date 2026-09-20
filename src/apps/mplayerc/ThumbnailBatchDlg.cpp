#include "stdafx.h"
#include "MainFrm.h"
#include "ThumbnailBatchDlg.h"
#include "OpenMediaData.h"
#include <afxdlgs.h>

BEGIN_MESSAGE_MAP(CThumbnailBatchDlg, CResizableDialog)
    ON_WM_TIMER()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

CThumbnailBatchDlg::CThumbnailBatchDlg(CMainFrame* frame) : CResizableDialog(TH_DIALOG, frame), m_frame(frame) {}
CThumbnailBatchDlg::~CThumbnailBatchDlg() = default;

BOOL CThumbnailBatchDlg::OnInitDialog()
{
    __super::OnInitDialog();
    m_options.Load(); m_list.SubclassDlgItem(TH_FILES, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    CRect r; m_list.GetClientRect(r);
    m_list.InsertColumn(0, ResStr(TH_FILECOL), LVCFMT_LEFT, r.Width()*3/4);
    m_list.InsertColumn(1, ResStr(TH_STATUSCOL), LVCFMT_LEFT, r.Width()/4-4);
    // Give all extra vertical space to the file queue; keep settings together below it.
    AddAnchor(57100, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(TH_FILES, TOP_LEFT, BOTTOM_RIGHT);
    for (int id : {57101,57102,57107}) AddAnchor(id, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(TH_DIRECTORY, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(TH_BROWSE, BOTTOM_RIGHT); AddAnchor(TH_BESIDE, BOTTOM_RIGHT);
    AddAnchor(TH_START, BOTTOM_RIGHT); AddAnchor(TH_CANCEL, BOTTOM_RIGHT); AddAnchor(IDCANCEL, BOTTOM_RIGHT);
    AddAnchor(TH_OPENOUTPUT, BOTTOM_LEFT); AddAnchor(TH_SUMMARY, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(TH_TOTAL, BOTTOM_LEFT); AddAnchor(TH_CURRENT, BOTTOM_RIGHT);
    AddAnchor(57121, BOTTOM_RIGHT);
    AddAllOtherAnchors(BOTTOM_LEFT);
    auto* format = static_cast<CComboBox*>(GetDlgItem(TH_FORMAT));
    format->AddString(L"JPG"); format->AddString(L"PNG"); format->AddString(L"BMP");
    format->SetCurSel(m_options.extension == L".png" ? 1 : m_options.extension == L".bmp" ? 2 : 0);
    SetDlgItemTextW(TH_DIRECTORY, m_options.directory);
    SetDlgItemInt(TH_ROWS, m_options.rows); SetDlgItemInt(TH_COLS, m_options.cols);
    SetDlgItemInt(TH_WIDTH, m_options.width); SetDlgItemInt(TH_MARGIN, m_options.margin);
    SetDlgItemInt(TH_QUALITY, m_options.quality); SetDlgItemInt(TH_COMPRESSION, m_options.compression);
    SetDlgItemInt(TH_INFOSIZE, m_options.infoSize); SetDlgItemInt(TH_TIMESIZE, m_options.timeSize);
    SetDlgItemTextW(TH_INFOFONT, m_options.infoFont); SetDlgItemTextW(TH_TIMEFONT, m_options.timeFont);
    CheckDlgButton(TH_INFO, m_options.info); CheckDlgButton(TH_TIME, m_options.time);
    CheckDlgButton(TH_LOGO, m_options.logo); CheckDlgButton(TH_SUBTITLES, m_options.subtitles); CheckDlgButton(TH_BESIDE, m_options.beside);
    CheckDlgButton(TH_HIDEVIDEO, m_options.hideVideo);
    if (m_frame->m_eMediaLoadState == MLS_LOADED && !m_frame->m_bAudioOnly && m_frame->GetPlaybackMode() == PM_FILE) AddFile(m_frame->GetCurFileName());
    EnableSettings(true); Refresh();
    return TRUE;
}
void CThumbnailBatchDlg::AddFile(const CStringW& path)
{
    if (path.IsEmpty() || PathIsURLW(path) || PathIsDirectoryW(path)) return;
    for (const auto& item : m_items) if (!item.file.CompareNoCase(path)) return;
    m_items.push_back({path});
}
void CThumbnailBatchDlg::AddFiles()
{
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR,
        L"Video files|*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.webm;*.ts;*.m2ts;*.mpg;*.mpeg;*.m4v;*.flv;*.vob|All files|*.*||", this);
    std::vector<wchar_t> names(65536);
    dlg.GetOFN().lpstrFile = names.data(); dlg.GetOFN().nMaxFile = DWORD(names.size());
    if (dlg.DoModal() == IDOK) {
        POSITION pos = dlg.GetStartPosition(); while (pos) AddFile(dlg.GetNextPathName(pos));
        Refresh();
    }
}
void CThumbnailBatchDlg::Refresh()
{
    if (m_list.GetItemCount() != int(m_items.size())) {
        m_list.DeleteAllItems();
        for (size_t i = 0; i < m_items.size(); ++i) m_list.InsertItem(int(i), m_items[i].file);
    }
    unsigned succeeded = 0, failed = 0, cancelled = 0;
    for (size_t i = 0; i < m_items.size(); ++i) {
        auto& item = m_items[i]; UINT text = TH_READY;
        switch (item.result) {
        case ThumbnailResult::Success: text = TH_DONE; ++succeeded; break;
        case ThumbnailResult::Failed: text = TH_FAILED; ++failed; break;
        case ThumbnailResult::Cancelled: text = TH_CANCELLED; ++cancelled; break;
        case ThumbnailResult::Running: text = m_stage == Stage::Opening ? TH_WAITING : TH_WORKING; break;
        default: break;
        }
        CStringW status = ResStr(text); if (!item.error.IsEmpty()) status += L": " + item.error;
        m_list.SetItemText(int(i), 1, status);
    }
    CStringW summary; summary.Format(ResStr(TH_SUMMARYTEXT), succeeded, failed, cancelled, unsigned(m_items.size()));
    SetDlgItemTextW(TH_SUMMARY, summary);
    auto* progress = static_cast<CProgressCtrl*>(GetDlgItem(TH_TOTAL));
    progress->SetRange32(0, std::max(1, int(m_items.size()))); progress->SetPos(succeeded+failed+cancelled);
}
void CThumbnailBatchDlg::FormatChanged()
{
    int index = static_cast<CComboBox*>(GetDlgItem(TH_FORMAT))->GetCurSel();
    for (int id : {TH_QUALITY, TH_Q_LABEL}) GetDlgItem(id)->ShowWindow(index == 0 ? SW_SHOW : SW_HIDE);
    for (int id : {TH_COMPRESSION, TH_P_LABEL}) GetDlgItem(id)->ShowWindow(index == 1 ? SW_SHOW : SW_HIDE);
    bool directory = !IsDlgButtonChecked(TH_BESIDE) && m_stage == Stage::Idle;
    GetDlgItem(TH_DIRECTORY)->EnableWindow(directory); GetDlgItem(TH_BROWSE)->EnableWindow(directory);
}
void CThumbnailBatchDlg::EnableSettings(bool enable)
{
    for (int id : {TH_ADD,TH_REMOVE,TH_CLEAR,TH_DIRECTORY,TH_BROWSE,TH_BESIDE,TH_FORMAT,TH_QUALITY,TH_COMPRESSION,
        TH_ROWS,TH_COLS,TH_WIDTH,TH_MARGIN,TH_COLOR,TH_INFO,TH_TIME,TH_LOGO,TH_SUBTITLES,TH_INFOFONT,TH_TIMEFONT,
        TH_INFOSIZE,TH_TIMESIZE,TH_HIDEVIDEO,TH_START}) GetDlgItem(id)->EnableWindow(enable);
    GetDlgItem(TH_CANCEL)->EnableWindow(!enable); FormatChanged();
}
bool CThumbnailBatchDlg::ReadOptions()
{
    auto read = [this](int id, int& value, int low, int high) {
        BOOL valid = FALSE; const UINT n = GetDlgItemInt(id, &valid, FALSE);
        if (!valid || n < UINT(low) || n > UINT(high)) { GetDlgItem(id)->SetFocus(); return false; }
        value = int(n); return true;
    };
    if (!read(TH_ROWS,m_options.rows,1,20) || !read(TH_COLS,m_options.cols,1,10) || !read(TH_WIDTH,m_options.width,256,5120)
        || !read(TH_MARGIN,m_options.margin,0,100) || !read(TH_INFOSIZE,m_options.infoSize,8,96)
        || !read(TH_TIMESIZE,m_options.timeSize,8,96) || !read(TH_QUALITY,m_options.quality,70,100)
        || !read(TH_COMPRESSION,m_options.compression,1,9)) { AfxMessageBox(ResStr(TH_INVALID)); return false; }
    m_options.info = !!IsDlgButtonChecked(TH_INFO); m_options.time = !!IsDlgButtonChecked(TH_TIME);
    m_options.logo = !!IsDlgButtonChecked(TH_LOGO); m_options.subtitles = !!IsDlgButtonChecked(TH_SUBTITLES);
    m_options.hideVideo = !!IsDlgButtonChecked(TH_HIDEVIDEO);
    m_options.beside = !!IsDlgButtonChecked(TH_BESIDE); GetDlgItemTextW(TH_DIRECTORY, m_options.directory);
    const int format = static_cast<CComboBox*>(GetDlgItem(TH_FORMAT))->GetCurSel();
    m_options.extension = format == 1 ? L".png" : format == 2 ? L".bmp" : L".jpg";
    if (m_items.empty() || (!m_options.beside && !PathIsDirectoryW(m_options.directory))) {
        AfxMessageBox(ResStr(TH_LOCALONLY)); return false;
    }
    if (m_options.width <= (m_options.cols+1)*m_options.margin) { AfxMessageBox(ResStr(TH_INVALID)); return false; }
    return true;
}
void CThumbnailBatchDlg::ChooseFont(bool timestamp)
{
    LOGFONTW lf = {};
    auto& name = timestamp ? m_options.timeFont : m_options.infoFont;
    auto& size = timestamp ? m_options.timeSize : m_options.infoSize;
    auto& weight = timestamp ? m_options.timeWeight : m_options.infoWeight;
    auto& italic = timestamp ? m_options.timeItalic : m_options.infoItalic;
    size = int(GetDlgItemInt(timestamp ? TH_TIMESIZE : TH_INFOSIZE));
    lf.lfHeight = -std::clamp(size,8,96); lf.lfWeight = weight; lf.lfItalic = italic; wcscpy_s(lf.lfFaceName, name);
    CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST | CF_SCALABLEONLY, nullptr, this);
    if (dlg.DoModal() == IDOK) {
        name = lf.lfFaceName; size = std::clamp(int(abs(lf.lfHeight)),8,96); weight = lf.lfWeight; italic = !!lf.lfItalic;
        SetDlgItemTextW(timestamp ? TH_TIMEFONT : TH_INFOFONT, name);
        SetDlgItemInt(timestamp ? TH_TIMESIZE : TH_INFOSIZE, size);
    }
}
BOOL CThumbnailBatchDlg::OnCommand(WPARAM wp, LPARAM lp)
{
    const int id = LOWORD(wp);
    if (id == TH_CANCEL) { OnCancel(); return TRUE; }
    if (id == TH_OPENOUTPUT) {
        CStringW path; GetDlgItemTextW(TH_DIRECTORY,path);
        int selected = m_list.GetNextItem(-1, LVNI_SELECTED);
        if (selected >= 0 && !m_items[selected].output.IsEmpty()) path = GetFolderPath(m_items[selected].output);
        else if (IsDlgButtonChecked(TH_BESIDE) && !m_items.empty()) path = GetFolderPath(m_items[selected >= 0 ? selected : 0].file);
        if (PathIsDirectoryW(path)) ShellExecuteW(m_hWnd,L"open",path,nullptr,nullptr,SW_SHOWNORMAL);
        return TRUE;
    }
    if (m_stage != Stage::Idle) return __super::OnCommand(wp,lp);
    switch (id) {
    case TH_START: OnOK(); return TRUE;
    case TH_ADD: AddFiles(); return TRUE;
    case TH_REMOVE:
        for (int i = int(m_items.size())-1; i >= 0; --i) if (m_list.GetItemState(i, LVIS_SELECTED)) m_items.erase(m_items.begin()+i);
        Refresh(); return TRUE;
    case TH_CLEAR: m_items.clear(); Refresh(); return TRUE;
    case TH_FORMAT: case TH_BESIDE: FormatChanged(); return TRUE;
    case TH_BROWSE: {
        CStringW path; GetDlgItemTextW(TH_DIRECTORY,path);
        CFolderPickerDialog dlg(path, OFN_PATHMUSTEXIST, this);
        if (dlg.DoModal() == IDOK) SetDlgItemTextW(TH_DIRECTORY,dlg.GetPathName()); return TRUE;
    }
    case TH_COLOR: {
        CColorDialog dlg(m_options.background, CC_FULLOPEN, this);
        if (dlg.DoModal() == IDOK) m_options.background = dlg.GetColor(); return TRUE;
    }
    case TH_INFOFONT: ChooseFont(false); return TRUE;
    case TH_TIMEFONT: ChooseFont(true); return TRUE;
    }
    return __super::OnCommand(wp,lp);
}
void CThumbnailBatchDlg::OnOK()
{
    if (m_stage != Stage::Idle || !ReadOptions()) return;
    m_options.Save(); m_original.reset();
    m_subtitlesEnabled = AfxGetAppSettings().fEnableSubtitles;
    if (m_frame->m_eMediaLoadState == MLS_LOADED) {
        auto* original = dynamic_cast<OpenFileData*>(m_frame->m_lastOMD.get());
        if (!original) { AfxMessageBox(ResStr(TH_LOCALONLY)); return; }
        m_original = std::make_unique<OpenFileData>(*original);
        CPlaylistItem playlistItem;
        if (m_frame->m_wndPlaylistBar.GetCur(playlistItem)
            && !playlistItem.m_fi.GetPath().CompareNoCase(original->fi.GetPath())) {
            m_original->subs = playlistItem.m_subs;
            m_original->auds = playlistItem.m_auds;
        }
        m_position = m_frame->GetPos(); m_playState = m_frame->GetMediaState();
        m_audio = m_frame->GetAudioTrackIdx(); m_subtitle = m_frame->GetSubtitleTrackIdx(); m_rate = m_frame->m_PlaybackRate;
    }
    m_volume = m_frame->m_wndToolBar.Volume;
    if (m_frame->m_pBA) { long actual = m_volume; if (SUCCEEDED(m_frame->m_pBA->get_Volume(&actual))) m_volume = actual; }
    m_playlistAudio = m_frame->m_wndPlaylistBar.curPlayList.m_nSelectedAudioTrack;
    m_playlistSubtitle = m_frame->m_wndPlaylistBar.curPlayList.m_nSelectedSubtitleTrack;
    // Only hidden tasks use a separate render target. Otherwise retain the normal player view.
    // The target must exist before graph creation and outlive every batch renderer.
    if (m_options.hideVideo && !m_frame->m_wndThumbnailRender.GetSafeHwnd()
        && !m_frame->m_wndThumbnailRender.CreateEx(0, AfxRegisterWndClass(0), L"Thumbnail renderer",
            WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CRect(0, 0, 640, 360), &m_frame->m_wndView, 0)) {
        AfxMessageBox(ResStr(TH_OPENERROR)); return;
    }
    m_frame->m_bThumbnailBatch = true;
    AfxGetAppSettings().fEnableSubtitles = m_options.subtitles;
    for (auto& item : m_items) { item.result = ThumbnailResult::Pending; item.error.Empty(); item.output.Empty(); }
    m_cancel = m_aborting = false; m_index = 0; m_stage = Stage::Opening;
    EnableSettings(false); SetTimer(1,15,nullptr); Next();
}
void CThumbnailBatchDlg::Next()
{
    if (m_cancel || m_index == m_items.size()) { Restore(); return; }
    m_generator.reset(); m_stage = Stage::Opening; m_aborting = false;
    // Subtitle selection during graph setup may enable subtitles; restore the task choice per file.
    AfxGetAppSettings().fEnableSubtitles = m_options.subtitles;
    auto& item = m_items[m_index]; item.result = ThumbnailResult::Running;
    m_frame->m_thumbnailOpenResult = 0;
    m_frame->m_thumbnailOpenError.Empty();
    auto media = std::make_unique<OpenFileData>(); media->fi = item.file; media->title = item.file; media->bAddRecent = FALSE;
    // Reuse normal sidecar discovery on a temporary item without touching the real playlist.
    CPlaylistItem sidecars; sidecars.m_fi = item.file; sidecars.AutoLoadFiles();
    media->subs = sidecars.m_subs;
    media->auds = sidecars.m_auds;
    m_deadline = GetTickCount64()+60000;
    m_frame->m_bThumbnailInternalOpen = true; m_frame->OpenMedia(std::move(media)); m_frame->m_bThumbnailInternalOpen = false;
    static_cast<CProgressCtrl*>(GetDlgItem(TH_CURRENT))->SetRange32(0,m_options.rows*m_options.cols);
    static_cast<CProgressCtrl*>(GetDlgItem(TH_CURRENT))->SetPos(0);
    Refresh(); m_list.EnsureVisible(int(m_index), FALSE);
}
void CThumbnailBatchDlg::AbortOpen()
{
    if (m_aborting) return;
    m_aborting = true; m_frame->m_fOpeningAborted = true;
    SetDlgItemTextW(TH_SUMMARY, ResStr(TH_CANCELLING));
    if (m_frame->m_pGB) {
        CComQIPtr<IAMOpenProgress> progress = m_frame->m_pAMOP;
        if (!progress) progress = m_frame->m_pGB;
        if (progress) progress->AbortOperation();
        m_frame->m_pGB->Abort();
    }
}
void CThumbnailBatchDlg::Restore()
{
    m_generator.reset();
    if (m_cancel) for (size_t i=m_index; i<m_items.size(); ++i) if (m_items[i].result == ThumbnailResult::Pending || m_items[i].result == ThumbnailResult::Running) m_items[i].result = ThumbnailResult::Cancelled;
    Refresh();
    m_frame->CloseMedia();
    AfxGetAppSettings().fEnableSubtitles = m_subtitlesEnabled;
    if (!m_original) { Finish(true); return; }
    m_stage = Stage::Restoring; m_aborting = false;
    SetDlgItemTextW(TH_SUMMARY,ResStr(TH_RESTORING)); GetDlgItem(TH_CANCEL)->EnableWindow(FALSE);
    m_frame->m_bThumbnailRestoring = true; m_frame->m_thumbnailOpenResult = 0;
    m_original->rtStart = m_position; m_original->bAddRecent = FALSE; m_deadline = GetTickCount64()+60000;
    m_frame->m_bThumbnailInternalOpen = true; m_frame->OpenMedia(std::move(m_original)); m_frame->m_bThumbnailInternalOpen = false;
}
void CThumbnailBatchDlg::Finish(bool restored)
{
    KillTimer(1);
    if (!restored) m_frame->CloseMedia();
    AfxGetAppSettings().fEnableSubtitles = m_subtitlesEnabled;
    m_frame->m_wndPlaylistBar.curPlayList.m_nSelectedAudioTrack = m_playlistAudio;
    m_frame->m_wndPlaylistBar.curPlayList.m_nSelectedSubtitleTrack = m_playlistSubtitle;
    if (restored && m_frame->m_eMediaLoadState == MLS_LOADED) {
        m_frame->SetAudioTrackIdx(m_audio); m_frame->SetSubtitleTrackIdx(m_subtitle);
        AfxGetAppSettings().fEnableSubtitles = m_subtitlesEnabled;
        m_frame->UpdateSubtitle(false, false);
        // Restore the visible renderer's output before seeking a paused graph.
        // Batch opening bypasses the playback commands that normally position it.
        m_frame->MoveVideoWindow(false, true);
        m_frame->SeekTo(m_position,false); m_frame->SetPlayingRate(m_rate);
        if (m_frame->m_pBA) m_frame->m_pBA->put_Volume(m_volume);
        m_frame->m_nVolumeBeforeFrameStepping = m_volume;
    }
    m_frame->m_bThumbnailBatch = false; m_frame->m_bThumbnailRestoring = false;
    if (m_frame->m_pVideoWnd == &m_frame->m_wndThumbnailRender) m_frame->m_pVideoWnd = &m_frame->m_wndView;
    if (m_frame->m_wndThumbnailRender.GetSafeHwnd()) m_frame->m_wndThumbnailRender.DestroyWindow();
    if (restored && m_frame->m_eMediaLoadState == MLS_LOADED) {
        if (m_playState == State_Running) m_frame->Run(); else if (m_playState == State_Stopped) m_frame->Stop(); else m_frame->Pause();
    }
    m_stage = Stage::Idle; EnableSettings(true); Refresh();
    if (!restored) AfxMessageBox(ResStr(TH_RESTOREERROR));
    if (m_closeAfter) __super::OnCancel();
}
void CThumbnailBatchDlg::OnTimer(UINT_PTR id)
{
    if (id != 1 || m_tick || m_stage == Stage::Idle) return;
    m_tick = true;
    if (m_stage == Stage::Opening || m_stage == Stage::Restoring) {
        if (!m_frame->m_thumbnailOpenResult && (GetTickCount64() >= m_deadline || (m_cancel && m_stage == Stage::Opening))) AbortOpen();
        if (m_frame->m_thumbnailOpenResult) {
            bool ok = m_frame->m_thumbnailOpenResult > 0 && !m_aborting;
            if (m_stage == Stage::Restoring) Finish(ok);
            else if (m_cancel) Restore();
            else if (!ok) {
                m_items[m_index].result = ThumbnailResult::Failed; m_items[m_index].error = ResStr(TH_OPENERROR);
                if (m_aborting) m_items[m_index].error = ResStr(TH_OPENTIMEOUT);
                if (!m_aborting && !m_frame->m_thumbnailOpenError.IsEmpty()) m_items[m_index].error = m_frame->m_thumbnailOpenError;
                ++m_index; Next();
            } else {
                m_stage = Stage::Capturing; m_generator = std::make_unique<CThumbnailGenerator>();
                auto& item = m_items[m_index];
                m_generator->Start(m_frame,item.file,m_options.beside ? GetFolderPath(item.file) : m_options.directory,m_options); Refresh();
            }
        }
    } else if (m_stage == Stage::Capturing) {
        if (m_cancel) m_generator->Cancel(); else m_generator->Tick();
        static_cast<CProgressCtrl*>(GetDlgItem(TH_CURRENT))->SetPos(m_generator->Progress());
        if (m_generator->Result() != ThumbnailResult::Running) {
            auto& item = m_items[m_index]; item.result = m_generator->Result(); item.error = m_generator->Error(); item.output = m_generator->Output();
            ++m_index; Next();
        }
    }
    m_tick = false;
}
void CThumbnailBatchDlg::OnCancel()
{
    if (m_stage == Stage::Idle) { __super::OnCancel(); return; }
    if (m_stage == Stage::Restoring) return;
    m_cancel = true; GetDlgItem(TH_CANCEL)->EnableWindow(FALSE);
    SetDlgItemTextW(TH_SUMMARY, ResStr(TH_CANCELLING));
}
void CThumbnailBatchDlg::OnClose()
{
    if (m_stage != Stage::Idle) m_closeAfter = true;
    OnCancel();
}
