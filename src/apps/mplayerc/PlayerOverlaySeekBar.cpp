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
	return TRUE;
}

void CPlayerOverlaySeekBar::Hide()
{
	m_state.Hide();
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

void CPlayerOverlaySeekBar::UpdateLayout(const CRect& view, int dpiX, int dpiY, int transparency)
{
	if (m_scaleX != dpiX || m_scaleY != dpiY || !m_font.GetSafeHandle()) {
		m_scaleX = dpiX;
		m_scaleY = dpiY;
		m_font.DeleteObject();
		m_font.CreateFontW(-ScaleY(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	}
	CRect bounds(view.left + ScaleX(12), view.bottom - ScaleY(48),
		view.right - ScaleX(12), view.bottom - ScaleY(8));
	CRect current;
	GetWindowRect(current);
	if (current != bounds) {
		SetWindowPos(nullptr, bounds.left, bounds.top, bounds.Width(), bounds.Height(),
			SWP_NOZORDER | SWP_NOACTIVATE);
		Invalidate(FALSE);
	}
	transparency = std::clamp(transparency, 0, 90);
	if (m_transparency != transparency) {
		m_transparency = transparency;
		SetLayeredWindowAttributes(0, OverlaySeekBarState::AlphaFromTransparency(transparency), LWA_ALPHA);
	}
}

void CPlayerOverlaySeekBar::UpdateProgress(REFERENCE_TIME position, REFERENCE_TIME duration)
{
	if (m_state.UpdateProgress(position, duration) && GetSafeHwnd() && IsWindowVisible()) {
		Invalidate(FALSE);
	}
}

void CPlayerOverlaySeekBar::UpdateHover(bool inHotZone)
{
	if (m_state.UpdateHover(inHotZone, GetTickCount64())) {
		if (!IsWindowVisible()) {
			ShowWindow(SW_SHOWNOACTIVATE);
		}
	} else if (IsWindowVisible()) {
		Hide();
	}
}

CRect CPlayerOverlaySeekBar::TrackRect() const
{
	CRect rect;
	GetClientRect(rect);
	rect.DeflateRect(ScaleX(12), 0);
	rect.top = ScaleY(26);
	rect.bottom = rect.top + std::max(2, ScaleY(4));
	return rect;
}

void CPlayerOverlaySeekBar::OnPaint()
{
	CPaintDC paint(this);
	CRect rect;
	GetClientRect(rect);
	if (rect.IsRectEmpty()) {
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
	CString text = ReftimeToString2(m_state.position, false) + L" / " + ReftimeToString2(m_state.duration, false);
	CRect textRect(rect);
	textRect.DeflateRect(ScaleX(12), 0);
	textRect.top = ScaleY(3);
	textRect.bottom = ScaleY(21);
	dc.DrawTextW(text, textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

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
	if (m_state.duration > 0) {
		m_lastSeekPosition = -1;
		m_state.dragging = true;
		SetCapture();
		Seek(point);
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
	}
}

void CPlayerOverlaySeekBar::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_state.dragging && (nFlags & MK_LBUTTON)) {
		Seek(point);
	}
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
	::SetCursor(LoadCursorW(nullptr, IDC_HAND));
	return TRUE;
}
