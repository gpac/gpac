/////////////////////////////////////////////////////////////////////////////
// Name:        wxMenuButton
// Purpose:     A button with a dropdown wxMenu
// Author:      John Labenski
// Modified by:
// Created:     11/05/2002
// RCS-ID:
// Copyright:   (c) John Labenki
// Licence:     wxWidgets licence
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "menubtn.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/control.h"
#include "wx/menu.h"
#include "wx/settings.h"
#include "wx/bitmap.h"
#include "wx/pen.h"
#include "wx/dc.h"
#endif // WX_PRECOMP

#include <wx/tglbtn.h>
#include <wx/dcclient.h>
#include <wx/timer.h>
#include <wx/image.h>

#include "menubtn.h"



// ==========================================================================
// wxCustomButton
// ==========================================================================
IMPLEMENT_DYNAMIC_CLASS( wxCustomButton, wxControl )

BEGIN_EVENT_TABLE(wxCustomButton,wxControl)
	EVT_MOUSE_EVENTS ( wxCustomButton::OnMouseEvents )
	EVT_PAINT        ( wxCustomButton::OnPaint )
	EVT_SIZE         ( wxCustomButton::OnSize )
END_EVENT_TABLE()

wxCustomButton::~wxCustomButton()
{
	if (HasCapture()) ReleaseMouse();
	if (m_timer) delete m_timer;
}

void wxCustomButton::Init()
{
	m_focused = FALSE;
	m_labelMargin = wxSize(4,4);
	m_bitmapMargin = wxSize(2,2);
	m_down = 0;
	m_timer = NULL;
	m_eventType = 0;
	m_button_style = wxCUSTBUT_TOGGLE|wxCUSTBUT_BOTTOM;
}

bool wxCustomButton::Create(wxWindow* parent, wxWindowID id,
                            const wxString& label, const wxBitmap &bitmap,
                            const wxPoint& pos, const wxSize& size,
                            long style, const wxValidator& val,
                            const wxString& name)
{
	if (!wxControl::Create(parent,id,pos,size,wxNO_BORDER|wxCLIP_CHILDREN,val,name))
		return FALSE;

	wxControl::SetLabel(label);
	wxControl::SetBackgroundColour(parent->GetBackgroundColour());
	wxControl::SetForegroundColour(parent->GetForegroundColour());
	wxControl::SetFont(parent->GetFont());

	if (bitmap.Ok()) m_bmpLabel = bitmap;

	if (!SetButtonStyle(style)) return FALSE;

	wxSize bestSize = DoGetBestSize();
	SetSize(wxSize(size.x<0 ? bestSize.x:size.x, size.y<0 ? bestSize.y:size.y));
#if (wxMINOR_VERSION<8)
	SetBestSize(GetSize());
#else
	SetInitialSize(GetSize());
#endif

	CalcLayout(TRUE);
	return TRUE;
}

void wxCustomButton::SetValue(bool depressed)
{
	wxCHECK_RET(!(m_button_style & wxCUSTBUT_NOTOGGLE), wxT("can't set button state"));
	m_down = depressed ? 1 : 0;
	Refresh(FALSE);
}

bool wxCustomButton::SetButtonStyle(long style)
{
	int n_styles = 0;
	if ((style & wxCUSTBUT_LEFT) != 0)   n_styles++;
	if ((style & wxCUSTBUT_RIGHT) != 0)  n_styles++;
	if ((style & wxCUSTBUT_TOP) != 0)    n_styles++;
	if ((style & wxCUSTBUT_BOTTOM) != 0) n_styles++;
	wxCHECK_MSG(n_styles < 2, FALSE, wxT("Only one wxCustomButton label position allowed"));

	n_styles = 0;
	if ((style & wxCUSTBUT_NOTOGGLE) != 0)       n_styles++;
	if ((style & wxCUSTBUT_BUTTON) != 0)         n_styles++;
	if ((style & wxCUSTBUT_TOGGLE) != 0)         n_styles++;
	if ((style & wxCUSTBUT_BUT_DCLICK_TOG) != 0) n_styles++;
	if ((style & wxCUSTBUT_TOG_DCLICK_BUT) != 0) n_styles++;
	wxCHECK_MSG(n_styles < 2, FALSE, wxT("Only one wxCustomButton style allowed"));

	m_button_style = style;

	if ((m_button_style & wxCUSTBUT_BUTTON) != 0)
		m_down = 0;

	CalcLayout(TRUE);
	return TRUE;
}

void wxCustomButton::SetLabel( const wxString &label )
{
	wxControl::SetLabel(label);
	CalcLayout(TRUE);
}

// sequence of events in GTK is up, dclick, up.

void wxCustomButton::OnMouseEvents(wxMouseEvent& event)
{
	if (m_button_style & wxCUSTBUT_NOTOGGLE) return;

	if (event.LeftDown() || event.RightDown())
	{
		if (!HasCapture())
			CaptureMouse(); // keep depressed until up

		m_down++;
		Redraw();
	}
	else if (event.LeftDClick() || event.RightDClick())
	{
		m_down++; // GTK eats second down event
		Redraw();
	}
	else if (event.LeftUp())
	{
		if (HasCapture())
			ReleaseMouse();

		m_eventType = wxEVT_LEFT_UP;

#if (wxMINOR_VERSION<8)
		if (wxRect(wxPoint(0,0), GetSize()).Inside(event.GetPosition()))
#else
		if (wxRect(wxPoint(0,0), GetSize()).Contains(event.GetPosition()))
#endif
		{
			if ((m_button_style & wxCUSTBUT_BUTTON) && (m_down > 0))
			{
				m_down = 0;
				Redraw();
				SendEvent();
				return;
			}
			else
			{
				if (!m_timer)
				{
					m_timer = new wxTimer(this, m_down+1);
					m_timer->Start(200, TRUE);
				}
				else
				{
					m_eventType = wxEVT_LEFT_DCLICK;
				}

				if ((m_button_style & wxCUSTBUT_TOGGLE) &&
				        (m_button_style & wxCUSTBUT_TOG_DCLICK_BUT)) m_down++;
			}
		}

		Redraw();
	}
	else if (event.RightUp())
	{
		if (HasCapture())
			ReleaseMouse();

		m_eventType = wxEVT_RIGHT_UP;

#if (wxMINOR_VERSION<8)
		if (wxRect(wxPoint(0,0), GetSize()).Inside(event.GetPosition()))
#else
		if (wxRect(wxPoint(0,0), GetSize()).Contains(event.GetPosition()))
#endif
		{
			if ((m_button_style & wxCUSTBUT_BUTTON) && (m_down > 0))
			{
				m_down = 0;
				Redraw();
				SendEvent();
				return;
			}
			else
			{
				m_down++;

				if (!m_timer)
				{
					m_timer = new wxTimer(this, m_down);
					m_timer->Start(250, TRUE);
				}
				else
				{
					m_eventType = wxEVT_RIGHT_DCLICK;
				}
			}
		}

		Redraw();
	}
	else if (event.Entering())
	{
		m_focused = TRUE;
		if ((event.LeftIsDown() || event.RightIsDown()) && HasCapture())
			m_down++;

		Redraw();
	}
	else if (event.Leaving())
	{
		m_focused = FALSE;
		if ((event.LeftIsDown() || event.RightIsDown()) && HasCapture())
			m_down--;

		Redraw();
	}
}



void wxCustomButton::SendEvent()
{
	if (((m_button_style & wxCUSTBUT_TOGGLE) && (m_eventType == wxEVT_LEFT_UP)) ||
	        ((m_button_style & wxCUSTBUT_BUT_DCLICK_TOG) && (m_eventType == wxEVT_LEFT_DCLICK)) ||
	        ((m_button_style & wxCUSTBUT_TOG_DCLICK_BUT) && (m_eventType == wxEVT_LEFT_UP)))
	{
		wxCommandEvent eventOut(wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, GetId());
		eventOut.SetInt(m_down%2 ? 1 : 0);
		eventOut.SetExtraLong(m_eventType);
		eventOut.SetEventObject(this);
		GetEventHandler()->ProcessEvent(eventOut);
	}
	else
	{
		wxCommandEvent eventOut(wxEVT_COMMAND_BUTTON_CLICKED, GetId());
		eventOut.SetInt(0);
		eventOut.SetExtraLong(m_eventType);
		eventOut.SetEventObject(this);
		GetEventHandler()->ProcessEvent(eventOut);
	}
}

wxBitmap wxCustomButton::CreateBitmapDisabled(const wxBitmap &bitmap) const
{
	wxCHECK_MSG(bitmap.Ok(), wxNullBitmap, wxT("invalid bitmap"));

	unsigned char br = GetBackgroundColour().Red();
	unsigned char bg = GetBackgroundColour().Green();
	unsigned char bb = GetBackgroundColour().Blue();

	wxImage image = bitmap.ConvertToImage();
	int pos, width = image.GetWidth(), height = image.GetHeight();
	unsigned char *img_data = image.GetData();

	for (int j=0; j<height; j++)
	{
		for (int i=j%2; i<width; i+=2)
		{
			pos = (j*width+i)*3;
			img_data[pos  ] = br;
			img_data[pos+1] = bg;
			img_data[pos+2] = bb;
		}
	}

	return wxBitmap(image);
}

void wxCustomButton::SetBitmapLabel(const wxBitmap& bitmap)
{
	m_bmpLabel = bitmap;
	CalcLayout(TRUE);
}

void wxCustomButton::OnPaint(wxPaintEvent& WXUNUSED(event))
{
	wxPaintDC dc(this);
	Paint(dc);
}

void wxCustomButton::Redraw()
{
	wxClientDC dc(this);
	Paint(dc);
}

void wxCustomButton::Paint( wxDC &dc )
{
#if (wxMINOR_VERSION<8)
	dc.BeginDrawing();
#endif

	int w, h;
	GetSize(&w,&h);

	wxColour foreColour = GetForegroundColour();
	wxColour backColour = GetBackgroundColour();

	if (m_focused)
	{
		backColour.Set( wxMin(backColour.Red()   + 20, 255),
		                wxMin(backColour.Green() + 20, 255),
		                wxMin(backColour.Blue()  + 20, 255) );
	}

	wxBitmap bitmap;

	if (IsEnabled())
	{
		if (GetValue() && m_bmpSelected.Ok())
			bitmap = m_bmpSelected;
		else if (m_focused && m_bmpFocus.Ok())
			bitmap = m_bmpFocus;
		else if (m_bmpLabel.Ok())
			bitmap = m_bmpLabel;
	}
	else
	{
		// try to create disabled if it doesn't exist
		if (!m_bmpDisabled.Ok() && m_bmpLabel.Ok())
			m_bmpDisabled = CreateBitmapDisabled(m_bmpLabel);

		if (m_bmpDisabled.Ok())
			bitmap = m_bmpDisabled;
		else if (m_bmpLabel.Ok())
			bitmap = m_bmpLabel;

		foreColour = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
	}

	wxBrush brush(backColour, wxSOLID);
	dc.SetBackground(brush);
	dc.SetBrush(brush);
	dc.SetPen(*wxTRANSPARENT_PEN);

	dc.DrawRectangle(0, 0, w, h);

	if (bitmap.Ok())
		dc.DrawBitmap(bitmap, m_bitmapPos.x, m_bitmapPos.y, TRUE );

	if (!GetLabel().IsEmpty())
	{
		dc.SetFont(GetFont());
		dc.SetTextBackground(backColour);
		dc.SetTextForeground(foreColour);
		dc.DrawText(GetLabel(), m_labelPos.x, m_labelPos.y);
	}

	if (GetValue())                                        // draw sunken border
	{
		dc.SetPen(*wxGREY_PEN);
		dc.DrawLine(0,h-1,0,0);
		dc.DrawLine(0,0,w,0);
		dc.SetPen(*wxWHITE_PEN);
		dc.DrawLine(w-1,1,w-1,h-1);
		dc.DrawLine(w-1,h-1,0,h-1);
		dc.SetPen(*wxBLACK_PEN);
		dc.DrawLine(1,h-2,1,1);
		dc.DrawLine(1,1,w-1,1);
	}
	else if (((m_button_style & wxCUSTBUT_FLAT) == 0) || m_focused) // draw raised border
	{
		dc.SetPen(*wxWHITE_PEN);
		dc.DrawLine(0,h-2,0,0);
		dc.DrawLine(0,0,w-1,0);
		dc.SetPen(*wxBLACK_PEN);
		dc.DrawLine(w-1,0,w-1,h-1);
		dc.DrawLine(w-1,h-1,-1,h-1);
		dc.SetPen(*wxGREY_PEN);
		dc.DrawLine(2,h-2,w-2,h-2);
		dc.DrawLine(w-2,h-2,w-2,1);
	}

	dc.SetBackground(wxNullBrush);
	dc.SetBrush(wxNullBrush);
	dc.SetPen(wxNullPen);
#if (wxMINOR_VERSION<8)
	dc.EndDrawing();
#endif
}

void wxCustomButton::OnSize( wxSizeEvent &event )
{
	CalcLayout(TRUE);
	event.Skip();
}

void wxCustomButton::SetMargins(const wxSize &margin, bool fit)
{
	m_labelMargin = margin;
	m_bitmapMargin = margin;
	CalcLayout(TRUE);
	if (fit) SetSize(DoGetBestSize());
}
void wxCustomButton::SetLabelMargin(const wxSize &margin, bool fit)
{
	m_labelMargin = margin;
	CalcLayout(TRUE);
	if (fit) SetSize(DoGetBestSize());
}
void wxCustomButton::SetBitmapMargin(const wxSize &margin, bool fit)
{
	m_bitmapMargin = margin;
	CalcLayout(TRUE);
	if (fit) SetSize(DoGetBestSize());
}

wxSize wxCustomButton::DoGetBestSize() const
{
	int lw=0, lh=0;
	int bw=0, bh=0;
	bool has_bitmap = FALSE;
	bool has_label = FALSE;

	if (!GetLabel().IsEmpty())
	{
		GetTextExtent(GetLabel(), &lw, &lh);
		lw += 2*m_labelMargin.x;
		lh += 2*m_labelMargin.y;
		has_label = TRUE;
	}
	if (m_bmpLabel.Ok())
	{
		bw = m_bmpLabel.GetWidth() + 2*m_bitmapMargin.x;
		bh = m_bmpLabel.GetHeight() + 2*m_bitmapMargin.y;
		has_bitmap = TRUE;
	}

	if ((m_button_style & wxCUSTBUT_LEFT) || (m_button_style & wxCUSTBUT_RIGHT))
	{
		int h = bh > lh ? bh : lh;
		if (has_bitmap && has_label) lw -= wxMin(m_labelMargin.x, m_bitmapMargin.x);
		return wxSize(lw+bw, h);
	}

	int w = bw > lw ? bw : lw;
	if (has_bitmap && has_label) lh -= wxMin(m_labelMargin.y, m_bitmapMargin.y);
	return wxSize(w, lh+bh);
}

void wxCustomButton::CalcLayout(bool refresh)
{
	int w, h;
	GetSize(&w,&h);

	int bw = 0, bh = 0;
	int lw = 0, lh = 0;

	if (m_bmpLabel.Ok()) // assume they're all the same size
	{
		bw = m_bmpLabel.GetWidth();
		bh = m_bmpLabel.GetHeight();
	}
	wxString label = GetLabel();
	if (!label.IsEmpty())
	{
		GetTextExtent(label, &lw, &lh);
	}

	// Center the label or bitmap if only one or the other
	if (!m_bmpLabel.Ok())
	{
		m_bitmapPos = wxPoint(0,0);
		m_labelPos = wxPoint((w-lw)/2, (h-lh)/2);
	}
	else if (label.IsEmpty())
	{
		m_bitmapPos = wxPoint((w-bw)/2, (h-bh)/2);
		m_labelPos = wxPoint(0,0);
	}
	else if (m_button_style & wxCUSTBUT_LEFT)
	{
		int mid_margin = wxMax(m_labelMargin.x, m_bitmapMargin.x);
		m_labelPos  = wxPoint((w - (bw+lw+m_labelMargin.x+m_bitmapMargin.x+mid_margin))/2 + m_labelMargin.x, (h - lh)/2);
		m_bitmapPos = wxPoint(m_labelPos.x + lw + mid_margin,         (h - bh)/2);
	}
	else if (m_button_style & wxCUSTBUT_RIGHT)
	{
		int mid_margin = wxMax(m_labelMargin.x, m_bitmapMargin.x);
		m_bitmapPos = wxPoint((w - (bw+lw+m_labelMargin.x+m_bitmapMargin.x+mid_margin))/2 + m_bitmapMargin.x, (h - bh)/2);
		m_labelPos  = wxPoint(m_bitmapPos.x + bw + mid_margin,        (h - lh)/2);
	}
	else if (m_button_style & wxCUSTBUT_TOP)
	{
		int mid_margin = wxMax(m_labelMargin.y, m_bitmapMargin.y);
		m_labelPos  = wxPoint((w - lw)/2, (h - (bh+lh+m_labelMargin.y+m_bitmapMargin.y+mid_margin))/2 + m_labelMargin.y);
		m_bitmapPos = wxPoint((w - bw)/2, m_labelPos.y + lh + mid_margin);
	}
	else // if (m_button_style & wxCUSTBUT_BOTTOM)  DEFAULT
	{
		int mid_margin = wxMax(m_labelMargin.y, m_bitmapMargin.y);
		m_bitmapPos = wxPoint((w - bw)/2, (h - (bh+lh+m_labelMargin.y+m_bitmapMargin.y+mid_margin))/2 + m_bitmapMargin.y);
		m_labelPos  = wxPoint((w - lw)/2, m_bitmapPos.y + bh + mid_margin);
	}

	if (refresh) Refresh(FALSE);
}


/* XPM */
static const char *down_arrow_xpm_data[] = {
	/* columns rows colors chars-per-pixel */
	"5 3 2 1",
	"  c None",
	"a c Black",
	/* pixels */
	"aaaaa",
	" aaa ",
	"  a  "
};

static wxBitmap s_dropdownBitmap; // all buttons share the same bitmap

enum
{
	IDD_DROPDOWN_BUTTON = 100
};

//-----------------------------------------------------------------------------
// wxMenuButtonEvents
//-----------------------------------------------------------------------------

DEFINE_LOCAL_EVENT_TYPE(wxEVT_MENUBUTTON_OPEN)

// ==========================================================================
// MenuDropButton
// ==========================================================================

class MenuDropButton : public wxCustomButton
{
public:
	MenuDropButton( wxWindow *parent, wxWindowID id, long style) : wxCustomButton()
	{
		if (!s_dropdownBitmap.Ok())
			s_dropdownBitmap = wxBitmap(down_arrow_xpm_data);

		Create( parent, id, wxEmptyString, s_dropdownBitmap, wxDefaultPosition,
		        wxSize(wxMENUBUTTON_DROP_WIDTH, wxMENUBUTTON_DROP_HEIGHT), style);
	}

	virtual void Paint( wxDC &dc )
	{
		wxCustomButton *labelBut = ((wxMenuButton*)GetParent())->GetLabelButton();

		// pretend that both buttons have focus (for flat style)
		if (labelBut)
		{
			wxPoint p = GetParent()->ScreenToClient(wxGetMousePosition());

#if (wxMINOR_VERSION<8)
			if (GetRect().Inside(p) || labelBut->GetRect().Inside(p))
#else
			if (GetRect().Contains(p) || labelBut->GetRect().Contains(p))
#endif
			{
				m_focused = TRUE;

				if (!labelBut->GetFocused())
					labelBut->SetFocused(TRUE);
			}
			else
			{
				m_focused = FALSE;

				if (labelBut->GetFocused())
					labelBut->SetFocused(FALSE);
			}
		}

		wxCustomButton::Paint(dc);
	}
};

// ==========================================================================
// MenuLabelButton
// ==========================================================================

class MenuLabelButton : public wxCustomButton
{
public:
	MenuLabelButton( wxWindow* parent, wxWindowID id,
	                 const wxString &label,
	                 const wxBitmap &bitmap,
	                 long style ) : wxCustomButton()
	{
		Create(parent, id, label, bitmap, wxDefaultPosition, wxDefaultSize, style);
	}

	virtual void Paint( wxDC &dc )
	{
		wxCustomButton *dropBut = ((wxMenuButton*)GetParent())->GetDropDownButton();

		// pretend that both buttons have focus (for flat style)
		if (dropBut)
		{
			wxPoint p = GetParent()->ScreenToClient(wxGetMousePosition());

#if (wxMINOR_VERSION<8)
			if (GetRect().Inside(p) || dropBut->GetRect().Inside(p))
#else
			if (GetRect().Contains(p) || dropBut->GetRect().Contains(p))
#endif
			{
				m_focused = TRUE;

				if (!dropBut->GetFocused())
					dropBut->SetFocused(TRUE);
			}
			else
			{
				m_focused = FALSE;

				if (dropBut->GetFocused())
					dropBut->SetFocused(FALSE);
			}
		}

		wxCustomButton::Paint(dc);
	}
};

// ==========================================================================
// wxMenuButton
// ==========================================================================

IMPLEMENT_DYNAMIC_CLASS( wxMenuButton, wxControl )

BEGIN_EVENT_TABLE(wxMenuButton,wxControl)
	EVT_BUTTON(wxID_ANY, wxMenuButton::OnButton)

#ifdef __WXMSW__
	EVT_MENU(wxID_ANY, wxMenuButton::OnMenu)
#endif
END_EVENT_TABLE()

wxMenuButton::~wxMenuButton()
{
	AssignMenu(NULL, TRUE);
}

void wxMenuButton::Init()
{
	m_labelButton = NULL;
	m_dropdownButton = NULL;
	m_menu = NULL;
	m_menu_static = FALSE;
	m_style = 0;
}

bool wxMenuButton::Create( wxWindow* parent, wxWindowID id,
                           const wxString &label,
                           const wxBitmap &bitmap,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxValidator& val,
                           const wxString& name)
{
	m_style = style;

	long flat = style & wxMENUBUT_FLAT;

	wxControl::Create(parent,id,pos,size,wxNO_BORDER|wxCLIP_CHILDREN,val,name);
	wxControl::SetLabel(label);
	SetBackgroundColour(parent->GetBackgroundColour());
	SetForegroundColour(parent->GetForegroundColour());
	SetFont(parent->GetFont());

	m_labelButton = new MenuLabelButton(this, id, label, bitmap, wxCUSTBUT_BUTTON|flat);
	m_dropdownButton = new MenuDropButton(this, IDD_DROPDOWN_BUTTON, wxCUSTBUT_BUTTON|flat);

	wxSize bestSize = DoGetBestSize();
	SetSize( wxSize(size.x < 0 ? bestSize.x : size.x,
	                size.y < 0 ? bestSize.y : size.y) );

#if (wxMINOR_VERSION<8)
	SetBestSize(GetSize());
#else
	SetInitialSize(GetSize());
#endif

	return TRUE;
}

#ifdef __WXMSW__
// FIXME - I think there was a patch to fix this
void wxMenuButton::OnMenu( wxCommandEvent &event )
{
	event.Skip();
	wxMenuItem *mi = m_menu->FindItem(event.GetId());
	if (mi && (mi->GetKind() == wxITEM_RADIO))
		m_menu->Check(event.GetId(), TRUE);
}
#endif // __WXMSW__

void wxMenuButton::OnButton( wxCommandEvent &event)
{
	int win_id = event.GetId();

	if (win_id == IDD_DROPDOWN_BUTTON)
	{
		wxNotifyEvent mevent(wxEVT_MENUBUTTON_OPEN, GetId());
		mevent.SetEventObject(this);
		if (GetEventHandler()->ProcessEvent(mevent) && !mevent.IsAllowed())
			return;

		if (!m_menu)
			return;

		PopupMenu(m_menu, wxPoint(0, GetSize().y));

		m_labelButton->Refresh(FALSE);
		m_dropdownButton->Refresh(FALSE);
	}
	else if (win_id == m_labelButton->GetId())
	{

		wxCommandEvent cevent(wxEVT_COMMAND_MENU_SELECTED, win_id);
		cevent.SetEventObject(this);
		cevent.SetId(win_id);
		GetParent()->GetEventHandler()->ProcessEvent(cevent);

		if (!m_menu) return;

		const wxMenuItemList &items = m_menu->GetMenuItems();
		int first_radio_id = -1;
		int checked_id = -1;
		bool check_next = FALSE;

		// find the next available radio item to check
		for (wxMenuItemList::Node *node = items.GetFirst(); node; node = node->GetNext())
		{
			wxMenuItem *mi = (wxMenuItem*)node->GetData();
			if (mi && (mi->GetKind() == wxITEM_RADIO))
			{
				if (first_radio_id == -1)
					first_radio_id = mi->GetId();

				if (check_next)
				{
					check_next = FALSE;
					checked_id = mi->GetId();
					break;
				}
				else if (mi->IsChecked())
					check_next = TRUE;
			}
		}
		// the last item was checked, go back to the first
		if (check_next && (first_radio_id != -1))
			checked_id = first_radio_id;

		if (checked_id != -1)
		{
			m_menu->Check(checked_id, TRUE);

			wxCommandEvent mevent( wxEVT_COMMAND_MENU_SELECTED, checked_id);
			mevent.SetEventObject( m_menu );
			mevent.SetInt(1);
			GetEventHandler()->ProcessEvent(mevent);
		}
	}
}

int wxMenuButton::GetSelection() const
{
	wxCHECK_MSG(m_menu != NULL, wxNOT_FOUND, wxT("No attached menu in wxMenuButton::GetSelection"));

	const wxMenuItemList &items = m_menu->GetMenuItems();

	for (wxMenuItemList::Node *node = items.GetFirst(); node; node = node->GetNext())
	{
		wxMenuItem *mi = (wxMenuItem*)node->GetData();
		if (mi && (mi->GetKind() == wxITEM_RADIO))
		{
			if (mi->IsChecked())
				return mi->GetId();
		}
	}

	return wxNOT_FOUND;
}

void wxMenuButton::AssignMenu(wxMenu *menu, bool static_menu)
{
	if (!m_menu_static && m_menu)
		delete m_menu;

	m_menu = menu;
	m_menu_static = static_menu;
}

void wxMenuButton::SetToolTip(const wxString &tip)
{
	wxWindow::SetToolTip(tip);
	((wxWindow*)m_labelButton)->SetToolTip(tip);
	((wxWindow*)m_dropdownButton)->SetToolTip(tip);
}
void wxMenuButton::SetToolTip(wxToolTip *tip)
{
	wxWindow::SetToolTip(tip);
	((wxWindow*)m_labelButton)->SetToolTip(tip);
	((wxWindow*)m_dropdownButton)->SetToolTip(tip);
}

void wxMenuButton::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
	wxSize curSize( GetSize() );
	wxSize bestSize( DoGetBestSize() );

	if (width == -1)
		width = curSize.GetWidth();
	if (width < 10)
		width = bestSize.GetWidth();

	if (height == -1)
		height = curSize.GetHeight();
	if (height < 5)
		height = bestSize.GetHeight();

	wxWindow::DoSetSize(x, y, width, height, sizeFlags);

	if (m_labelButton)
		m_labelButton->SetSize(0, 0, width - wxMENUBUTTON_DROP_WIDTH, height);
	if (m_dropdownButton)
		m_dropdownButton->SetSize(width-wxMENUBUTTON_DROP_WIDTH, 0, wxMENUBUTTON_DROP_WIDTH, height);
}

wxSize wxMenuButton::DoGetBestSize()
{
	if (!m_labelButton || !m_dropdownButton)
		return wxSize(wxMENUBUTTON_DROP_WIDTH+wxMENUBUTTON_DROP_HEIGHT, wxMENUBUTTON_DROP_HEIGHT);

	wxSize size = m_labelButton->GetBestSize();
	size.x += wxMENUBUTTON_DROP_WIDTH;
	return size;
}
