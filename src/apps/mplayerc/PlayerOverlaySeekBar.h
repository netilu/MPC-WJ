/*
 * (C) 2026 see Authors.txt
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "OverlaySeekBarState.h"

class CMainFrame;

// An owned, non-activating window; never participates in the docked bar layout.
class CPlayerOverlaySeekBar : public CWnd
{
	CMainFrame* m_pMainFrame = nullptr;
	OverlaySeekBarState m_state;
	REFERENCE_TIME m_lastSeekPosition = -1;
	int m_scaleX = 96;
	int m_scaleY = 96;
	int m_transparency = -1;
	CFont m_font;

	int ScaleX(int value) const { return MulDiv(value, m_scaleX, 96); }
	int ScaleY(int value) const { return MulDiv(value, m_scaleY, 96); }
	CRect TrackRect() const;
	void Seek(CPoint point);

public:
	BOOL Create(CMainFrame* pMainFrame);
	void Hide();
	void UpdateLayout(const CRect& view, int dpiX, int dpiY, int transparency);
	void UpdateProgress(REFERENCE_TIME position, REFERENCE_TIME duration);
	void UpdateHover(bool inHotZone);

protected:
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
	afx_msg int OnMouseActivate(CWnd*, UINT, UINT) { return MA_NOACTIVATE; }
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnCaptureChanged(CWnd* pWnd);
	afx_msg void OnCancelMode();
	afx_msg BOOL OnSetCursor(CWnd*, UINT, UINT);
	DECLARE_MESSAGE_MAP()
};
