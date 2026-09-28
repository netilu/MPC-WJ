/*
 * (C) 2026 see Authors.txt
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "stdafx.h"
#include "MainFrm.h"
#include "PlayerOverlaySeekBar.h"

BEGIN_MESSAGE_MAP(CPlayerOverlaySeekBar, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEACTIVATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_CAPTURECHANGED()
	ON_WM_CANCELMODE()
	ON_WM_SETCURSOR()
	ON_WM_NCHITTEST()
END_MESSAGE_MAP()

BOOL CPlayerOverlaySeekBar::Create(CMainFrame* pMainFrame)
{
	m_pMainFrame = pMainFrame;
	if (!CreateEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			AfxRegisterWndClass(0), L"Minimal mode seek bar", WS_POPUP,
			CRect(0, 0, 0, 0), pMainFrame, 0)) {
		return FALSE;
	}
	ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);
	if (m_tooltip.Create(this, TTS_NOPREFIX | TTS_ALWAYSTIP)) {
		m_tooltip.SetMaxTipWidth(SHRT_MAX);
		m_toolInfo.hwnd = m_hWnd;
		m_toolInfo.hinst = AfxGetInstanceHandle();
		m_toolInfo.uId = (UINT_PTR)m_hWnd;
		m_tooltip.SendMessageW(TTM_ADDTOOLW, 0, (LPARAM)&m_toolInfo);
	}
	return TRUE;
}

void CPlayerOverlaySeekBar::Hide()
{
	HidePointerFeedback();
	m_state.Hide();
	m_passiveAllowed = false;
	m_lastSeekPosition = -1;
	if (!GetSafeHwnd()) {
		return;
	}
	if (::GetCapture() == m_hWnd) {
		ReleaseCapture();
	}
	if (IsWindowVisible()) {
		ShowWindow(SW_HIDE);
	}
}

void CPlayerOverlaySeekBar::UpdateLayout(const CRect& view, int dpiX, int dpiY, int transparency, bool passiveAllowed)
{
	m_viewRect = view;
	m_passiveAllowed = passiveAllowed;
	if (m_scaleX != dpiX || m_scaleY != dpiY || !m_font.GetSafeHandle()) {
		m_scaleX = dpiX;
		m_scaleY = dpiY;
		m_font.DeleteObject();
		m_font.CreateFontW(-ScaleY(11), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
		UpdateTimeLabelWidth();
	}
	m_transparency = std::clamp(transparency, 0, 90);
	ApplyPresentation();
}

void CPlayerOverlaySeekBar::ApplyPresentation()
{
	const bool passive = m_passiveAllowed && !m_state.visible;
	const CRect bounds = passive
		? CRect(m_viewRect.left, m_viewRect.bottom - std::max(1, ScaleY(2)), m_viewRect.right, m_viewRect.bottom)
		: CRect(m_viewRect.left + ScaleX(4), m_viewRect.bottom - ScaleY(29),
			m_viewRect.right - ScaleX(4), m_viewRect.bottom - ScaleY(5));
	const bool presentationChanged = m_passive != passive;
	if (presentationChanged) {
		m_passive = passive;
		ModifyStyleEx(passive ? 0 : WS_EX_TRANSPARENT, passive ? WS_EX_TRANSPARENT : 0,
			SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
		Invalidate(FALSE);
	}
	CRect current;
	GetWindowRect(current);
	if (current != bounds) {
		SetWindowPos(nullptr, bounds.left, bounds.top, bounds.Width(), bounds.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
		Invalidate(FALSE);
	}
	const BYTE alpha = OverlaySeekBarState::AlphaFromTransparency(m_transparency);
	const BYTE targetAlpha = alpha;
	if (presentationChanged || m_displayAlpha != targetAlpha) {
		m_displayAlpha = targetAlpha;
		SetLayeredWindowAttributes(passive ? RGB(1, 2, 3) : 0, targetAlpha,
			passive ? LWA_ALPHA | LWA_COLORKEY : LWA_ALPHA);
	}
}

void CPlayerOverlaySeekBar::UpdateProgress(REFERENCE_TIME position, REFERENCE_TIME duration)
{
	const auto oldDuration = m_state.duration;
	const bool changed = m_state.UpdateProgress(position, duration);
	if (m_state.duration != oldDuration) {
		UpdateTimeLabelWidth();
	}
	if (changed && GetSafeHwnd() && IsWindowVisible()) {
		Invalidate(FALSE);
	}
}

void CPlayerOverlaySeekBar::UpdateHover(bool inHotZone)
{
	const bool active = m_state.UpdateHover(inHotZone, GetTickCount64());
	if (!active && !m_passiveAllowed) {
		Hide();
		return;
	}
	ApplyPresentation();
	if (!IsWindowVisible()) {
		ShowWindow(SW_SHOWNOACTIVATE);
	}
	if (active) {
		UpdatePointerFeedback();
	} else {
		HidePointerFeedback();
	}
}

void CPlayerOverlaySeekBar::UpdateTimeLabelWidth()
{
	if (!GetSafeHwnd() || !m_font.GetSafeHandle()) {
		return;
	}
	CClientDC dc(this);
	const auto oldFont = dc.SelectObject(&m_font);
	m_timeLabelWidth = dc.GetTextExtent(ReftimeToString2(m_state.duration, false)).cx;
	dc.SelectObject(oldFont);
}

bool CPlayerOverlaySeekBar::ShowTimeLabels() const
{
	CRect rect;
	GetClientRect(rect);
	const int innerWidth = rect.Width() - 2 * ScaleX(4);
	return OverlaySeekBarState::TimeLabelsFit(innerWidth, m_timeLabelWidth, ScaleX(4), ScaleX(64));
}

CRect CPlayerOverlaySeekBar::TrackRect() const
{
	CRect rect;
	GetClientRect(rect);
	rect.DeflateRect(ScaleX(4), 0);
	if (ShowTimeLabels()) {
		rect.left += m_timeLabelWidth + ScaleX(4);
		rect.right -= m_timeLabelWidth + ScaleX(4);
	}
	rect.top = (rect.Height() - std::max(2, ScaleY(4))) / 2;
	rect.bottom = rect.top + std::max(2, ScaleY(4));
	return rect;
}

void CPlayerOverlaySeekBar::HidePointerFeedback()
{
	if (m_tooltipVisible && m_tooltip.GetSafeHwnd()) {
		m_tooltip.SendMessageW(TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_toolInfo);
	}
	if (m_previewOwned) {
		m_pMainFrame->PreviewWindowHide();
	}
	m_tooltipVisible = m_previewOwned = false;
	m_hoverPosition = m_lastPreviewPosition = -1;
	m_hoverStart = m_lastPreviewUpdate = 0;
}

void CPlayerOverlaySeekBar::ShowTimeTooltip(CPoint screenPoint, REFERENCE_TIME position)
{
	if (!m_tooltip.GetSafeHwnd()) {
		return;
	}
	m_hoverText = ReftimeToString2(position, false);
	m_toolInfo.lpszText = (LPWSTR)(LPCWSTR)m_hoverText;
	m_tooltip.SendMessageW(TTM_SETTOOLINFOW, 0, (LPARAM)&m_toolInfo);
	const CSize size = m_tooltip.GetBubbleSize(&m_toolInfo);
	MONITORINFO monitor = { sizeof(monitor) };
	GetMonitorInfoW(MonitorFromPoint(screenPoint, MONITOR_DEFAULTTONEAREST), &monitor);
	const int minX = monitor.rcWork.left + 4;
	const int maxX = std::max<int>(minX, monitor.rcWork.right - size.cx - 4);
	const int x = std::clamp<int>(screenPoint.x - size.cx / 2, minX, maxX);
	int y = screenPoint.y - size.cy - ScaleY(12);
	if (y < monitor.rcWork.top + 4) {
		y = screenPoint.y + ScaleY(16);
	}
	m_tooltip.SendMessageW(TTM_TRACKPOSITION, 0, MAKELPARAM(x, y));
	if (!m_tooltipVisible) {
		m_tooltip.SendMessageW(TTM_TRACKACTIVATE, TRUE, (LPARAM)&m_toolInfo);
		m_tooltipVisible = true;
	}
}

bool CPlayerOverlaySeekBar::ShowPreview(CPoint screenPoint, REFERENCE_TIME position)
{
	if (!m_pMainFrame->CanPreviewUse()) {
		return false;
	}
	auto& preview = m_pMainFrame->m_wndPreView;
	preview.SetWindowSize();
	CRect bounds;
	preview.GetWindowRect(bounds);
	MONITORINFO monitor = { sizeof(monitor) };
	GetMonitorInfoW(MonitorFromPoint(screenPoint, MONITOR_DEFAULTTONEAREST), &monitor);
	const int minX = monitor.rcWork.left + 4;
	const int maxX = std::max(minX, (int)monitor.rcWork.right - bounds.Width() - 4);
	const int x = std::clamp<int>(screenPoint.x - bounds.Width() / 2, minX, maxX);
	CRect bar;
	GetWindowRect(bar);
	const int y = std::max<int>(monitor.rcWork.top + 4, bar.top - bounds.Height() - ScaleY(10));
	preview.SetWindowPos(nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	const ULONGLONG now = GetTickCount64();
	if (m_previewOwned && (position == m_lastPreviewPosition || now - m_lastPreviewUpdate < 50)) {
		return true;
	}
	preview.SetWindowTextW(ReftimeToString2(position, false));
	if (FAILED(m_pMainFrame->PreviewWindowShow(position))) {
		return false;
	}
	m_previewOwned = true;
	m_lastPreviewPosition = position;
	m_lastPreviewUpdate = now;
	return true;
}

void CPlayerOverlaySeekBar::UpdatePointerFeedback()
{
	if (!GetSafeHwnd() || !IsWindowVisible() || !m_state.visible || m_state.duration <= 0) {
		HidePointerFeedback();
		return;
	}
	CPoint screenPoint;
	GetCursorPos(&screenPoint);
	CPoint point(screenPoint);
	ScreenToClient(&point);
	CRect client;
	GetClientRect(client);
	const CRect track = TrackRect();
	const bool overTrack = client.PtInRect(point) && point.x >= track.left && point.x <= track.right
		&& ::WindowFromPoint(screenPoint) == m_hWnd;
	if (!overTrack && !m_state.dragging) {
		HidePointerFeedback();
		return;
	}
	const REFERENCE_TIME position = OverlaySeekBarState::PositionAtPixel(
		(std::int64_t)point.x - track.left, track.Width(), m_state.duration);
	if (m_hoverPosition < 0) {
		m_hoverStart = GetTickCount64();
	}
	m_hoverPosition = position;
	if (m_pMainFrame->CanPreviewUse() && GetTickCount64() - m_hoverStart >= 200
			&& ShowPreview(screenPoint, position)) {
		if (m_tooltipVisible) {
			m_tooltip.SendMessageW(TTM_TRACKACTIVATE, FALSE, (LPARAM)&m_toolInfo);
			m_tooltipVisible = false;
		}
	} else {
		if (m_previewOwned) {
			m_pMainFrame->PreviewWindowHide();
			m_previewOwned = false;
		}
		ShowTimeTooltip(screenPoint, position);
	}
}

void CPlayerOverlaySeekBar::OnPaint()
{
	CPaintDC paint(this);
	CRect rect;
	GetClientRect(rect);
	if (rect.IsRectEmpty()) {
		return;
	}
	if (m_passive) {
		const int progressX = rect.left + (m_state.duration > 0
			? (int)(rect.Width() * (double)m_state.position / m_state.duration) : 0);
		paint.FillSolidRect(rect, RGB(1, 2, 3));
		paint.FillSolidRect(CRect(rect.left, rect.top, progressX, rect.bottom), RGB(90, 190, 245));
		return;
	}
	CDC dc;
	dc.CreateCompatibleDC(&paint);
	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(&paint, rect.Width(), rect.Height());
	const auto oldBitmap = dc.SelectObject(&bitmap);
	dc.FillSolidRect(rect, RGB(24, 28, 32));

	const auto oldFont = dc.SelectObject(&m_font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(245, 245, 245));
	if (ShowTimeLabels()) {
		CRect left(rect.left + ScaleX(4), rect.top, rect.left + ScaleX(4) + m_timeLabelWidth, rect.bottom);
		CRect right(rect.right - ScaleX(4) - m_timeLabelWidth, rect.top, rect.right - ScaleX(4), rect.bottom);
		dc.DrawTextW(ReftimeToString2(m_state.position, false), left, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.DrawTextW(ReftimeToString2(m_state.duration, false), right, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}

	const CRect track = TrackRect();
	const int width = std::max(0, track.Width());
	const int x = track.left + (m_state.duration > 0 ? (int)(width * (double)m_state.position / m_state.duration) : 0);
	dc.FillSolidRect(track, RGB(95, 102, 109));
	dc.FillSolidRect(CRect(track.left, track.top, x, track.bottom), RGB(90, 190, 245));
	CBrush thumb(RGB(245, 245, 245));
	const auto oldBrush = dc.SelectObject(&thumb);
	const auto oldPen = dc.SelectStockObject(NULL_PEN);
	const int radius = std::max(3, ScaleY(5));
	const int y = (track.top + track.bottom) / 2;
	dc.Ellipse(x - radius, y - radius, x + radius + 1, y + radius + 1);
	dc.SelectObject(oldPen);
	dc.SelectObject(oldBrush);
	dc.SelectObject(oldFont);
	paint.BitBlt(0, 0, rect.Width(), rect.Height(), &dc, 0, 0, SRCCOPY);
	dc.SelectObject(oldBitmap);
}

void CPlayerOverlaySeekBar::Seek(CPoint point)
{
	if (!m_state.visible) {
		return;
	}
	const CRect track = TrackRect();
	const int width = track.Width();
	if (width <= 0 || m_state.duration <= 0) {
		return;
	}
	REFERENCE_TIME position = OverlaySeekBarState::PositionAtPixel(
		(std::int64_t)point.x - track.left, width, m_state.duration);
	if (AfxGetAppSettings().fFastSeek ^ (GetKeyState(VK_SHIFT) < 0)) {
		position = m_pMainFrame->GetClosestKeyFrame(position);
	}
	position = std::clamp(position, 0LL, m_state.duration);
	// Mouse-up and repeated moves within one keyframe must not seek again.
	// Re-seeking the same frame flushes playback and makes a click jump twice.
	if (position == m_lastSeekPosition) {
		return;
	}
	if (m_pMainFrame->SeekFromOverlay(position)) {
		m_lastSeekPosition = position;
		m_state.position = position;
		Invalidate(FALSE);
	}
}

void CPlayerOverlaySeekBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (m_state.visible && m_state.duration > 0) {
		m_lastSeekPosition = -1;
		m_state.dragging = true;
		SetCapture();
		Seek(point);
		UpdatePointerFeedback();
	}
}

void CPlayerOverlaySeekBar::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_state.dragging) {
		Seek(point);
		m_state.dragging = false;
		if (::GetCapture() == m_hWnd) {
			ReleaseCapture();
		}
		UpdatePointerFeedback();
	}
}

void CPlayerOverlaySeekBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_state.dragging && (nFlags & MK_LBUTTON)) {
		Seek(point);
	}
	UpdatePointerFeedback();
}

void CPlayerOverlaySeekBar::OnCaptureChanged(CWnd* pWnd)
{
	m_state.dragging = false;
	m_lastSeekPosition = -1;
	__super::OnCaptureChanged(pWnd);
}

void CPlayerOverlaySeekBar::OnCancelMode()
{
	Hide();
	__super::OnCancelMode();
}

BOOL CPlayerOverlaySeekBar::OnSetCursor(CWnd*, UINT, UINT)
{
	if (m_passive) {
		return FALSE;
	}
	::SetCursor(LoadCursorW(nullptr, IDC_HAND));
	return TRUE;
}

LRESULT CPlayerOverlaySeekBar::OnNcHitTest(CPoint point)
{
	return m_passive ? HTTRANSPARENT : __super::OnNcHitTest(point);
}
