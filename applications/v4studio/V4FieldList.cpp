#include "safe_include.h" 

// For compilers that supports precompilation , includes "wx/wx.h"
#include "wx/wxprec.h"

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include <wx/colordlg.h>
#include <wx/fontdlg.h>

#include "V4FieldList.h"

#include "V4StudioFrame.h"

BEGIN_EVENT_TABLE(V4FieldList, wxGrid)
	EVT_GRID_CELL_CHANGE(V4FieldList::OnCellChanged) 
	EVT_GRID_CELL_LEFT_CLICK(V4FieldList::OnCellLeftClick) 
	EVT_GRID_CELL_RIGHT_CLICK(V4FieldList::OnCellRightClick) 
	EVT_GRID_CELL_LEFT_DCLICK(V4FieldList::OnCellLeftDClick)
END_EVENT_TABLE()

class Position {
public :
	Position(u32 fi, s32 fp) : fieldIndex(fi), fieldPosition(fp) {}
	u32 fieldIndex;
	s32 fieldPosition;
};

V4FieldList::V4FieldList(wxWindow *parent_, wxSize size) : wxGrid(parent_, -1, wxDefaultPosition, size) {
	positions = gf_list_new();
	CreateGrid(0,0);
	SetRowLabelSize(0);
	SetColLabelSize(20);
	SetColLabelValue(0, wxString(" "));
	SetColLabelValue(1, wxString("Field Name"));
	SetColLabelValue(2, wxString("Field Value"));
	InsertCols(0,3);
	AutoSizeColumns(true);
	EnableGridLines(true);

	parent = (V4StudioFrame *) parent_;
}

V4FieldList::~V4FieldList() 
{
	s32 i;
	for (i=gf_list_count(positions)-1; i>=0; i--) {
		Position *p = (Position *)gf_list_get(positions, i);
		delete p;
		p = NULL;
		gf_list_rem(positions, i);
	}
	gf_list_del(positions);
}

void V4FieldList::Create() 
{
	GF_Node *node = m_pNode;

	if (!node) return;

	DeleteCols(0,3);
	u32 nbRows = GetNumberRows();
	if (nbRows) DeleteRows(0,nbRows);
	InsertCols(0,3);

	s32 i;
	for (i=gf_list_count(positions)-1; i>=0; i--) {
		Position *p = (Position *)gf_list_get(positions, i);
		delete p;
		p = NULL;
		gf_list_rem(positions, i);
	}

	u32 pos = 0; // counts rows added

  // adds a row to modify DefName
  InsertRows(pos, 1);
  SetCellValue(pos, 1, wxString("Name"));
  const char * defName = gf_node_get_name(node);
  if (defName) SetCellValue(pos, 2, wxString(defName));
  gf_list_add(positions, new Position(-1, -1));
  // enables modifying the DefName
  for (i=0; i<2; i++) SetReadOnly(pos, i, TRUE);
  SetReadOnly(pos, 2, FALSE);
  pos++;

  // adds all fields
	GF_FieldInfo field;
	u32 count = gf_node_get_field_count(node);
	u32 j;

	for (j=0; j<count; j++) {
		gf_node_get_field(node, j, &field);
		switch (field.eventType) {
			case GF_SG_EVENT_FIELD:
			case GF_SG_EVENT_EXPOSED_FIELD:
				InsertOneFieldRow(pos, field);
				pos++;
				break;
			default:
				break;
		}
	}
	AutoSizeColumns(true);
}

void V4FieldList::InsertOneFieldRow(u32 pos, GF_FieldInfo field) 
{
	InsertRows(pos, 1);
	gf_list_add(positions, new Position(field.fieldIndex, -1));
	SetReadOnly(pos, 0, TRUE);
  SetReadOnly(pos, 1, TRUE);
  SetReadOnly(pos, 2, FALSE);
	SetCellFieldValue(field, pos);
}

void V4FieldList::SetFieldValue(GF_FieldInfo f, wxString *value, int pos) 
{
	void *ptr = NULL;
	s32 type = -1;
	if (gf_sg_vrml_is_sf_field(f.fieldType)) {
		type = f.fieldType;
		ptr = f.far_ptr;
	} else {
		if (pos < 0) return;
		gf_sg_vrml_mf_get_item(f.far_ptr, f.fieldType, &ptr, pos);
		type = gf_sg_vrml_get_sf_type(f.fieldType);
	}
	switch (type) {
		case GF_SG_VRML_SFCOLOR:
			sscanf(value->c_str(), "%f %f %f", &(((SFColor *)ptr)->red), &(((SFColor *)ptr)->green), &(((SFColor *)ptr)->blue));
			break;
		case GF_SG_VRML_SFVEC2F:
			sscanf(value->c_str(), "%f %f", &(((SFVec2f *)ptr)->x), &(((SFVec2f *)ptr)->y));
			break;
		case GF_SG_VRML_SFFLOAT:
			sscanf(value->c_str(), "%f", (SFFloat *)ptr);
			break;
		case GF_SG_VRML_SFINT32:
			sscanf(value->c_str(), "%d", (SFInt32 *)ptr);
			break;
		case GF_SG_VRML_SFBOOL:
			if (!stricmp(value->c_str(), "true")) *((SFBool *)ptr) = 1;
			else *((SFBool *)ptr) = 0;
			break;
		case GF_SG_VRML_SFSTRING:
			if (((SFString *)ptr)->buffer) free(((SFString *)ptr)->buffer);
			((SFString *)ptr)->buffer = strdup(value->c_str());
			break;
		default:
			break;
	}	
}

void V4FieldList::GetFieldValue(GF_FieldInfo f, wxString *s, int pos) 
{
	void *ptr = NULL;
	s32 type = -1;
	if (gf_sg_vrml_is_sf_field(f.fieldType)) {
		type = f.fieldType;
		ptr = f.far_ptr;
	} else {
		if (pos < 0) return;
		gf_sg_vrml_mf_get_item(f.far_ptr, f.fieldType, &ptr, pos);
		type = gf_sg_vrml_get_sf_type(f.fieldType);
	}
	switch (type) {
		case GF_SG_VRML_SFBOOL:
			if (*(SFBool *)ptr == 0) s->Printf("false");
			else s->Printf("true");
			break;
		case GF_SG_VRML_SFINT32:
			s->Printf("%d", *(SFInt32 *)ptr);
			break;
		case GF_SG_VRML_SFCOLOR:
			s->Printf("%.2f %.2f %.2f", ((SFColor *)ptr)->red, ((SFColor *)ptr)->green, ((SFColor *)ptr)->blue);
			break;
		case GF_SG_VRML_SFVEC2F:
			s->Printf("%.2f %.2f", ((SFVec2f *)ptr)->x, ((SFVec2f *)ptr)->y);
			break;
		case GF_SG_VRML_SFFLOAT:
			s->Printf("%.2f", *(SFFloat *)ptr);
			break;
		case GF_SG_VRML_SFSTRING:
			s->Printf("%s", ((SFString *)ptr)->buffer);
			break;
		case GF_SG_VRML_SFSCRIPT:
			s->Printf("%s", ((SFScript *)ptr)->script_text);
			break;
		default:
			break;
	}	
	gf_node_dirty_set(m_pNode, 0, 1);
}

// OnCellChanged -- user has validated the changes made to a field, updates the node with the new value
void V4FieldList::OnCellChanged(wxGridEvent &evt) 
{
	u32 row;
	GF_FieldInfo field;
	wxString value;

	if (!m_pNode || evt.GetCol() == 0) {
		evt.Skip();
		return;
	}

	row = evt.GetRow();
	value = GetCellValue(row, 2);
	Position *pSelected = (Position *)gf_list_get(positions, row);
	if (!pSelected) {
		evt.Skip();
		return;
	}

  // defName requires special treatment
  if (pSelected->fieldIndex == (u32) -1) {
    u32 id = gf_node_get_id(m_pNode);
    if (!id) id = gf_sg_get_next_available_node_id(parent->GetV4SceneManager()->GetSceneGraph());
    gf_node_set_id(m_pNode, id, value.c_str());
    parent->GetV4SceneManager()->pools.Add(m_pNode);
  } else {
    // modifies any kind of field
	  gf_node_get_field(m_pNode, pSelected->fieldIndex, &field);
	  if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		  SetFieldValue(field, &value, -1);
	  } else {
		  if (pSelected->fieldPosition >= 0) {
			  SetFieldValue(field, &value, pSelected->fieldPosition);
		  } else {
			  GenMFField *mf = (GenMFField *)field.far_ptr;		
			  void *ptr;
			  gf_sg_vrml_mf_insert(mf, field.fieldType, &ptr, mf->count);
			  SetFieldValue(field, &value, mf->count-1);
			  SetCellValue(row, 2, "");
			  wxString sign = GetCellValue(row, 0);
			  switch (sign.GetChar(0)) {
			  case '-':
				  {
					  u32 insertPosition = row+mf->count+1;
					  InsertRows(insertPosition-1, 1);
					  gf_list_insert(positions, new Position(pSelected->fieldIndex, mf->count-1), insertPosition);
					  wxString tmp;
					  tmp << '['; tmp << (mf->count-1); tmp << ']';
					  SetCellValue(insertPosition-1, 1, tmp);
					  wxString buf;
					  GetFieldValue(field, &buf, mf->count-1);
					  SetCellValue(insertPosition-1, 2, buf);
				  }
				  break;
			  case '+':
				  break;
			  case ' ':
				  SetCellValue(row, 0, "+");
				  break;
			  }
		  }
	  }
  }
	gf_node_dirty_set(m_pNode, 0, 1);
	V4StudioFrame *parent = (V4StudioFrame *)this->GetParent();
	parent->Update();
}

void V4FieldList::SetLog(wxString s) 
{
	V4StudioFrame *parent = (V4StudioFrame *)this->GetParent();
	parent->GetStatusBar()->SetStatusText(s);
}

void V4FieldList::SetCellFieldValue(GF_FieldInfo field, u32 pos) 
{
	wxString buf;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		SetCellValue(pos,1,wxString(field.name));
		GetFieldValue(field, &buf, -1);
		SetCellValue(pos,2,buf);
	} else {
		GenMFField *mf = (GenMFField *)field.far_ptr;
		if (mf->count > 0 ) {
			SetCellValue(pos,0,"+");
		} else {
			SetCellValue(pos,0," ");
		}
		SetCellValue(pos,1,field.name);
	}
}

// OnCellLeftClick -- draws columns for multivalued fields
void V4FieldList::OnCellLeftClick(wxGridEvent &evt) 
{
	u32 col = evt.GetCol();
	u32 row = evt.GetRow();
	GF_FieldInfo field;
	GF_Node *node = m_pNode;

	if (col != 0) {
		evt.Skip();
		return;
	}

	Position *pSelected = (Position *)gf_list_get(positions, row);
	if (!pSelected) {
		evt.Skip();
		return;
	}

	gf_node_get_field(node, pSelected->fieldIndex, &field);
	if (gf_sg_vrml_is_sf_field(field.fieldType) || field.fieldType == GF_SG_VRML_MFNODE) {
		evt.Skip();
		return;
	}

	wxString value = GetCellValue(row, col);
	char c = value.GetChar(0);
	GenMFField *mf = (GenMFField *)field.far_ptr;
	if (c == ' ') {
		evt.Skip();
		return;
	} else if (c == '+') {
		value.SetChar(0, '-');
		SetCellValue(row,0,value);
		InsertRows(row+1, mf->count);
		for (u32 j = 0; j<mf->count; j++) {
			Position *p = new Position(pSelected->fieldIndex, j);
			gf_list_insert(positions, p, row+1+j);
			wxString tmp;
			tmp << '['; tmp << j; tmp << ']';
			SetCellValue(row+1+j, 1, tmp);
			wxString buf;
			GetFieldValue(field, &buf, j);
			SetCellValue(row+1+j, 2, buf);
		}
	} else if (c == '-') {
		value.SetChar(0, '+');
		SetCellValue(row,0,value);
		DeleteRows(row+1, mf->count);
		for (s32 j = mf->count-1; j>=0; j--) {
			Position *p = (Position *)gf_list_get(positions,row+1+j);
			delete p; 
			p = NULL;
			gf_list_rem(positions, row+1+j);
		}
	}
	evt.Skip();
}

void V4FieldList::OnCellRightClick(wxGridEvent &evt) 
{
	int row = evt.GetRow();
	GF_FieldInfo field;
	GF_Node *node = m_pNode;

	Position *pSelected = (Position *)gf_list_get(positions,row);
	if (!pSelected) {
		evt.Skip();
		return;
	}

	gf_node_get_field(node, pSelected->fieldIndex, &field);
	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		evt.Skip();
		return;
	}

	GenMFField *mf = (GenMFField *)field.far_ptr;
	if (pSelected->fieldPosition == -1) {
		evt.Skip();
		return;
	} 

	InsertRows(row, 1);
	gf_list_insert(positions, new Position(pSelected->fieldIndex, pSelected->fieldPosition), row);
	void *ptr;
	gf_sg_vrml_mf_insert(mf, field.fieldType, &ptr, pSelected->fieldPosition);
	SetCellValue(row, 2, "");
	for (u32 i=pSelected->fieldPosition; i<mf->count; i++) {
		wxString tmp;
		tmp << '['; tmp << i; tmp << ']';
		SetCellValue(row+i, 1, tmp);
	}
}

// OnCellLeftDClick -- Edit a given field
void V4FieldList::OnCellLeftDClick(wxGridEvent &evt) 
{
	int row = evt.GetRow();
	GF_FieldInfo field;
	GF_Node *node = m_pNode;

	Position *pSelected = (Position *)gf_list_get(positions,row);
	if (!pSelected) {
		evt.Skip();
		return;
	}

  // editing the DEF name is NOT modifying a field
  // we just need to modify the string itself
  if (pSelected->fieldIndex == (u32) -1) {
    evt.Skip();
    return;
  }

	gf_node_get_field(node, pSelected->fieldIndex, &field);
	u32 type;
	void *ptr;
	if (gf_sg_vrml_is_sf_field(field.fieldType)) {
		type = field.fieldType;
		ptr = field.far_ptr;
	} else {
		if (pSelected->fieldPosition < 0) {
			evt.Skip();
			return;
		}
		gf_sg_vrml_mf_get_item(field.far_ptr, field.fieldType, &ptr, pSelected->fieldPosition);
		type = gf_sg_vrml_get_sf_type(field.fieldType);
	}
	
	if (type== GF_SG_VRML_SFCOLOR) {
		wxColourDialog cd(this);
		if (cd.ShowModal() == wxID_OK)
		{
			wxColourData retData = cd.GetColourData();
			wxColour col = retData.GetColour();
			((SFColor *)ptr)->red = col.Red()/255.0f;					
			((SFColor *)ptr)->green = col.Green()/255.0f;
			((SFColor *)ptr)->blue = col.Blue()/255.0f;
		} else {
			return;
		}
	} else if (!stricmp(field.name,"family")) {
		wxFontDialog fd(this);
		if (fd.ShowModal() == wxID_OK) {
			wxFontData retData = fd.GetFontData();
			wxFont font = retData.GetChosenFont();
			wxString name = font.GetFaceName();
			if (((SFString *)ptr)->buffer) free(((SFString *)ptr)->buffer);
			((SFString *)ptr)->buffer = strdup(name.c_str());
		} else {
			return;
		}

	} else {
		evt.Skip();
		return;
	}
	V4StudioFrame *parent = (V4StudioFrame *)this->GetParent();
	parent->Update();
}