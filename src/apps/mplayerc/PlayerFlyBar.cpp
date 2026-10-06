/*
 * (C) 2012-2025 see Authors.txt
 *
 * This file is part of MPC-WJ.
 *
 * MPC-WJ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-WJ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "MainFrm.h"
#include "DSUtil/FileHandle.h"
#include "WicUtils.h"
#include "PlayerFlyBar.h"

// CPrevView

CFlyBar::CFlyBar(CMainFrame* pMainFrame)
	: m_pMainFrame(pMainFrame)
{
}

CFlyBar::~CFlyBar()
{
	if (m_pButtonImages) {
		delete m_pButtonImages;
	}
}

HRESULT CFlyBar::Create(CWnd* pWnd)
{
	if (!CreateEx(WS_EX_TOPMOST | WS_EX_LAYERED, AfxRegisterWndClass(0), nullptr, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, RECT{0,0,0,0}, pWnd, 0, nullptr)) {
		DLog(L"Failed to create Flybar Window");
		return E_FAIL;
	}
	ModifyStyleEx(WS_EX_LAYOUTRTL, WS_EX_NOINHERITLAYOUT);
	SetLayeredWindowAttributes(RGB(255, 0, 255), 150, LWA_ALPHA | LWA_COLORKEY);

	if (AfxGetAppSettings().fFlybarOnTop) {
		ShowWindow(SW_SHOWNOACTIVATE);
	}

	return S_OK;
}

IMPLEMENT_DYNAMIC(CFlyBar, CWnd)

BEGIN_MESSAGE_MAP(CFlyBar, CWnd)
	ON_WM_CREATE()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// CFlyBar message handlers

BOOL CFlyBar::PreTranslateMessage(MSG* pMsg)
{
	m_tooltip.RelayEvent(pMsg);

	switch (pMsg->message) {
		case WM_LBUTTONDOWN :
		case WM_LBUTTONDBLCLK :
		case WM_MBUTTONDOWN :
		case WM_MBUTTONUP :
		case WM_MBUTTONDBLCLK :
		case WM_RBUTTONDOWN :
		case WM_RBUTTONUP :
		case WM_RBUTTONDBLCLK :
			GetParentFrame()->SetFocus();
			break;
	}

	return CWnd::PreTranslateMessage(pMsg);
}

BOOL CFlyBar::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	cs.style &= ~WS_BORDER;

	return TRUE;
}

int CFlyBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1) {
		return -1;
	}

	m_tooltip.Create(this, TTS_ALWAYSTIP);
	EnableToolTips(true);

	m_tooltip.AddTool(this, L"");
	m_tooltip.Activate(TRUE);

	m_tooltip.SetDelayTime(TTDT_AUTOPOP, -1);
	m_tooltip.SetDelayTime(TTDT_INITIAL, 500);
	m_tooltip.SetDelayTime(TTDT_RESHOW, 0);

	return 0;
}

void CFlyBar::CalcButtonsRect()
{
	CRect rcBar;
	GetWindowRect(&rcBar);

	r_ExitIcon		= CRect(rcBar.right - 5 - (iw),     rcBar.top + 5, rcBar.right - 5,            rcBar.bottom - 5);
	r_RestoreIcon	= CRect(rcBar.right - 5 - (iw * 2), rcBar.top + 5, rcBar.right - 5 - (iw),     rcBar.bottom - 5);
	r_MinIcon		= CRect(rcBar.right - 5 - (iw * 3), rcBar.top + 5, rcBar.right - 5 - (iw * 2), rcBar.bottom - 5);
	r_FSIcon		= CRect(rcBar.right - 5 - (iw * 4), rcBar.top + 5, rcBar.right - 5 - (iw * 3), rcBar.bottom - 5);
	//
	r_InfoIcon		= CRect(rcBar.right - 5 - (iw * 6), rcBar.top + 5, rcBar.right - 5 - (iw * 5), rcBar.bottom - 5);
	r_SettingsIcon	= CRect(rcBar.right - 5 - (iw * 7), rcBar.top + 5, rcBar.right - 5 - (iw * 6), rcBar.bottom - 5);
	r_PreviewIcon	= CRect(rcBar.right - 5 - (iw * 8), rcBar.top + 5, rcBar.right - 5 - (iw * 7), rcBar.bottom - 5);
	//
	r_LockIcon		= CRect(rcBar.right - 5 - (iw * 9), rcBar.top + 5, rcBar.right - 5 - (iw * 8), rcBar.bottom - 5);
}

void CFlyBar::Scale()
{
	UpdateButtonImages();
}

bool CFlyBar::UpdateButtonImages()
{
	if (m_pMainFrame->m_svgFlybar.IsLoad()) {
		int w = 0;
		int h = m_pMainFrame->ScaleY(AfxGetAppSettings().iToolbarSize);
		if (HBITMAP hBitmap = m_pMainFrame->m_svgFlybar.Rasterize(w, h)) {
			if (w == h * 25) {
				SAFE_DELETE(m_pButtonImages);
				m_pButtonImages = DNew CImageList();
				m_pButtonImages->Create(h, h, ILC_COLOR32 | ILC_MASK, 1, 0);
				ImageList_Add(m_pButtonImages->GetSafeHandle(), hBitmap, nullptr);

				iw = h;
			}
			DeleteObject(hBitmap);
		}

		if (m_pButtonImages) {
			return true;
		}
	}

	return false;
}

void CFlyBar::DrawButton(CDC *pDC, int nImage, int x, int z)
{
	m_pButtonImages->Draw(pDC, nImage, POINT{ x - 5 - (iw * z), 5 }, ILD_NORMAL);
}

void CFlyBar::DrawPreviewButton(CDC *pDC, int x)
{
	const int left = x - 5 - iw * 8;
	const auto px = [&](int value) { return left + MulDiv(value, iw, 120); };
	const auto py = [&](int value) { return 5 + MulDiv(value, iw, 120); };
	const bool enabled = AfxGetAppSettings().fSmartSeek;
	const COLORREF color = m_btIdx == 8 ? RGB(255, 255, 255)
		: enabled ? RGB(225, 225, 225) : RGB(145, 145, 145);
	CPen pen(PS_SOLID, std::max(1, MulDiv(8, iw, 120)), color);
	CPen* oldPen = pDC->SelectObject(&pen);
	CBrush* oldBrush = static_cast<CBrush*>(pDC->SelectStockObject(NULL_BRUSH));
	POINT eye[] = {
		{ px(22), py(60) }, { px(42), py(33) }, { px(77), py(33) }, { px(98), py(60) },
		{ px(77), py(87) }, { px(42), py(87) }, { px(22), py(60) }
	};
	pDC->PolyBezier(eye, std::size(eye));
	pDC->Ellipse(px(51), py(51), px(69), py(69));
	if (!enabled) {
		pDC->MoveTo(px(32), py(91));
		pDC->LineTo(px(88), py(29));
	}
	pDC->SelectObject(oldBrush);
	pDC->SelectObject(oldPen);
}

void CFlyBar::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_pMainFrame->SetFocus();

	CPoint p;
	GetCursorPos(&p);
	CalcButtonsRect();

	m_btIdx = 0;

	if (r_ExitIcon.PtInRect(p)) {
		ShowWindow(SW_HIDE);
		m_pMainFrame->PostMessageW(WM_COMMAND, ID_FILE_EXIT);
	} else if (r_MinIcon.PtInRect(p)) {
		m_pMainFrame->m_bTrayIcon ? m_pMainFrame->SendMessageW(WM_SYSCOMMAND, SC_MINIMIZE, -1) : m_pMainFrame->ShowWindow(SW_SHOWMINIMIZED);
		m_pMainFrame->RepaintVideo();
	} else if (r_RestoreIcon.PtInRect(p)) {
		if (m_pMainFrame->m_bFullScreen) {
			m_pMainFrame->ToggleFullscreen(true, true);
		}
		m_pMainFrame->ShowWindow(m_pMainFrame->IsZoomed() ? SW_SHOWNORMAL : SW_SHOWMAXIMIZED);
		m_pMainFrame->RepaintVideo();
		Invalidate();
	} else if (r_SettingsIcon.PtInRect(p)) {
		m_pMainFrame->PostMessageW(WM_COMMAND, ID_VIEW_OPTIONS);
		Invalidate();
	} else if (r_PreviewIcon.PtInRect(p)) {
		CAppSettings& s = AfxGetAppSettings();
		m_pMainFrame->SetSmartSeekEnabled(!s.fSmartSeek);
		s.SaveSettings();
		UpdateWnd(point);
	} else if (r_InfoIcon.PtInRect(p)) {
		OAFilterState fs = m_pMainFrame->GetMediaState();
		if (fs != -1) {
			m_pMainFrame->PostMessageW(WM_COMMAND, ID_FILE_PROPERTIES);
		}
		Invalidate();
	} else if (r_FSIcon.PtInRect(p)) {
		m_pMainFrame->OnViewFullscreen();
		Invalidate();
	} else if (r_LockIcon.PtInRect(p)) {
		CAppSettings& s = AfxGetAppSettings();
		s.fFlybarOnTop = !s.fFlybarOnTop;
		UpdateWnd(point);
	}
}

void CFlyBar::OnMouseMove(UINT nFlags, CPoint point)
{
	SetCursor(LoadCursorW(nullptr, IDC_HAND));

	if (!m_bTrackingMouseLeave) {
		TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, GetSafeHwnd() };
		if (TrackMouseEvent(&tme)) {
			m_bTrackingMouseLeave = true;
		}
	}

	UpdateWnd(point);
}

void CFlyBar::OnMouseLeave()
{
	m_bTrackingMouseLeave = false;
}

void CFlyBar::UpdateWnd(CPoint point)
{
	ClientToScreen(&point);

	CString str, str2;
	m_tooltip.GetText(str,this);

	if (r_ExitIcon.PtInRect(point)) {
		if (str != ResStr(IDS_AG_EXIT)) {
			m_tooltip.UpdateTipText(ResStr(IDS_AG_EXIT), this);
		}
		m_btIdx = 1;
	}
	else if (r_RestoreIcon.PtInRect(point)) {
		WINDOWPLACEMENT wp;
		m_pMainFrame->GetWindowPlacement(&wp);
		str2 = (wp.showCmd == SW_SHOWMAXIMIZED) ? ResStr(IDS_TOOLTIP_RESTORE) : ResStr(IDS_TOOLTIP_MAXIMIZE);
		if (str != str2) {
			m_tooltip.UpdateTipText(str2, this);
		}
		m_btIdx = 2;
	}
	else if (r_MinIcon.PtInRect(point)) {
		if (str != ResStr(IDS_TOOLTIP_MINIMIZE)) {
			m_tooltip.UpdateTipText(ResStr(IDS_TOOLTIP_MINIMIZE), this);
		}
		m_btIdx = 3;
	}
	else if (r_FSIcon.PtInRect(point)) {
		str2 = m_pMainFrame->m_bFullScreen ? ResStr(IDS_TOOLTIP_WINDOW) : ResStr(IDS_TOOLTIP_FULLSCREEN);
		if (str != str2) {
			m_tooltip.UpdateTipText(str2, this);
		}
		m_btIdx = 4;
	}
	else if (r_InfoIcon.PtInRect(point)) {
		if (str != ResStr(IDS_AG_PROPERTIES)) {
			m_tooltip.UpdateTipText(ResStr(IDS_AG_PROPERTIES), this);
		}
		m_btIdx = 6;
	}
	else if (r_SettingsIcon.PtInRect(point)) {
		if (str != ResStr(IDS_AG_OPTIONS)) {
			m_tooltip.UpdateTipText(ResStr(IDS_AG_OPTIONS), this);
		}
		m_btIdx = 7;
	}
	else if (r_PreviewIcon.PtInRect(point)) {
		str2 = AfxGetAppSettings().fSmartSeek ? ResStr(IDS_TOOLTIP_DISABLE_SEARCH_PREVIEW)
			: ResStr(IDS_TOOLTIP_ENABLE_SEARCH_PREVIEW);
		if (str != str2) {
			m_tooltip.UpdateTipText(str2, this);
		}
		m_btIdx = 8;
	}
	else if (r_LockIcon.PtInRect(point)) {
		str2 = AfxGetAppSettings().fFlybarOnTop ? ResStr(IDS_TOOLTIP_UNLOCK) : ResStr(IDS_TOOLTIP_LOCK);
		if (str != str2) {
			m_tooltip.UpdateTipText(str2, this);
		}
		m_btIdx = 9;
	}
	else {
		if (str.GetLength() > 0) {
			m_tooltip.UpdateTipText(L"", this);
		}
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		m_btIdx = 0;
	}

	// set tooltip position
	CRect r_tooltip;
	m_tooltip.GetWindowRect(&r_tooltip);
	MONITORINFO mi = { sizeof(mi) };
	GetMonitorInfoW(MonitorFromWindow(this->m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
	CPoint p;
	p.x = std::max(0L, std::min(point.x, mi.rcMonitor.right - r_tooltip.Width()));
	const int iCursorHeight = 24;
	m_tooltip.SetWindowPos(nullptr, p.x, point.y + iCursorHeight, r_tooltip.Width(), r_tooltip.Height(), SWP_NOACTIVATE | SWP_NOZORDER);

	Invalidate();
}

void CFlyBar::OnPaint()
{
	CPaintDC dc(this);
	DrawWnd();
}

void CFlyBar::DrawWnd()
{
	CClientDC dc(this);
	if (IsWindowVisible()) {
		const CAppSettings& s = AfxGetAppSettings();

		CRect rcBar;
		GetClientRect(&rcBar);
		int x = rcBar.Width();

		WINDOWPLACEMENT wp;
		m_pMainFrame->GetWindowPlacement(&wp);

		OAFilterState fs = m_pMainFrame->GetMediaState();
		CDC mdc;
		mdc.CreateCompatibleDC(&dc);
		CBitmap bm;
		bm.CreateCompatibleBitmap(&dc, x, rcBar.Height());
		CBitmap* pOldBm = mdc.SelectObject(&bm);
		mdc.SetBkMode(TRANSPARENT);
		mdc.FillSolidRect(rcBar, RGB(0, 0, 0));

		int nImage = (m_btIdx == 1) ? IMG_EXIT_A : IMG_EXIT;
		DrawButton(&mdc, nImage, x, 1);

		if (wp.showCmd == SW_SHOWMAXIMIZED) {
			nImage = (m_btIdx == 2) ? IMG_STDWND_A : IMG_STDWND;
		} else {
			nImage = (m_btIdx == 2) ? IMG_MAXWND_A : IMG_MAXWND;
		}
		DrawButton(&mdc, nImage, x, 2);

		nImage = (m_btIdx == 3) ? IMG_MINWND_A : IMG_MINWND;
		DrawButton(&mdc, nImage, x, 3);

		if (m_pMainFrame->m_bFullScreen) {
			nImage = (m_btIdx == 4) ? IMG_WINDOW_A : IMG_WINDOW;
		} else {
			nImage = (m_btIdx == 4) ? IMG_FULSCR_A : IMG_FULSCR;
		}
		DrawButton(&mdc, nImage, x, 4);

		if (fs == -1) {
			nImage = IMG_INFO_D;
		} else {
			nImage = (m_btIdx == 6) ? IMG_INFO_A : IMG_INFO;
		}
		DrawButton(&mdc, nImage, x, 6);

		nImage = (m_btIdx == 7) ? IMG_SETS_A : IMG_SETS;
		DrawButton(&mdc, nImage, x, 7);
		DrawPreviewButton(&mdc, x);

		if (s.fFlybarOnTop) {
			nImage = (m_btIdx == 9) ? IMG_LOCK_A : IMG_LOCK;
		} else {
			nImage = (m_btIdx == 9) ? IMG_UNLOCK_A : IMG_UNLOCK;
		}
		DrawButton(&mdc, nImage, x, 9);

		dc.BitBlt(0, 0, x, rcBar.Height(), &mdc, 0, 0, SRCCOPY);

		mdc.SelectObject(pOldBm);
		bm.DeleteObject();
		mdc.DeleteDC();
	}

	m_btIdx = 0;
}

BOOL CFlyBar::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}
