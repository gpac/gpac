/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph Generator sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "svggen.h"
static FILE *BeginHtml()
{
	FILE *f;
	char sPath[GF_MAX_PATH];

	sprintf(sPath, "C:%cUsers%cCyril%ccontent%csvg%cregression%cregression_table.html", GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
	f = gf_fopen(sPath, "wt");

	fprintf(f, "<?xml version='1.0' encoding='UTF-8'?>\n");
	fprintf(f, "<html xmlns='http://www.w3.org/1999/xhtml'>\n");
	fprintf(f, "<head>\n");
	fprintf(f, "<title>Status of the SVG implementation in GPAC</title>\n");

	{
		time_t rawtime;
		time(&rawtime);
		fprintf(f, "<!--File generated on GMT %s bY SVGGen for GPAC Version %s-->\n\n", asctime(gmtime(&rawtime)), GPAC_VERSION);
	}
	fprintf(f, "<!--\n%s-->\n", COPYRIGHT);
	fprintf(f, "<link rel='stylesheet' type='text/css' href='gpac_table_style.css'/>\n");

	fprintf(f, "</head>\n");
	fprintf(f, "<body>\n");
	fprintf(f, "<h1 align='center'>Status of the SVG implementation in GPAC</h1>\n");
	return f;
}

static void EndHtml(FILE *f)
{
	fprintf(f, "</body>\n");
	fprintf(f, "</html>\n");
	gf_fclose(f);
}

/* Generates an HTML table */
void generate_table(GF_List *elements)
{
	u32 i, j;
	u32 nbExamples = 0;
	FILE *f;
	f = BeginHtml();


	fprintf(f, "<h2>Legend</h2>\n");
	fprintf(f, "<table>\n");
	fprintf(f, "<thead>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<th>Status</th>\n");
	fprintf(f, "<th>Color</th>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</thead>\n");
	fprintf(f, "<tbody>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Not supported</td>\n");
	fprintf(f, "<td class='not-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Partially supported</td>\n");
	fprintf(f, "<td class='partially-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<td>Fully supported</td>\n");
	fprintf(f, "<td class='fully-supported'></td>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</tbody>\n");
	fprintf(f, "</table>\n");

	fprintf(f, "<h2>SVG Tiny 1.2 Elements</h2>\n");
	fprintf(f, "<table class='reference-list' align='center'>\n");
	fprintf(f, "<thead>\n");
	fprintf(f, "<tr>\n");
	fprintf(f, "<th>Element Name</th>\n");
	fprintf(f, "<th>Status</th>\n");
	fprintf(f, "<th>Observations</th>\n");
	fprintf(f, "<th>Example(s)</th>\n");
	fprintf(f, "<th>Bug(s)</th>\n");
	fprintf(f, "</tr>\n");
	fprintf(f, "</thead>\n");
	fprintf(f, "<tbody>\n");
	for (i = 0; i < gf_list_count(elements); i++) {
		SVGGenElement *elt = gf_list_get(elements, i);
		fprintf(f, "<tr class='element' id='element_%s'>\n", elt->implementation_name);
		fprintf(f, "<td class='element-name'><a href='#table_%s'>%s</a></td>\n", elt->svg_name, elt->svg_name);
		fprintf(f, "<td class='not-supported'></td>\n");
		fprintf(f, "<td class='element-observation'>&nbsp;</td>\n");
		fprintf(f, "<td class='element-example'>&nbsp;</td>\n");
		fprintf(f, "<td class='element-bug'>&nbsp;</td>\n");
		fprintf(f, "</tr>\n");
	}
	fprintf(f, "</tbody>\n");
	fprintf(f, "</table>\n");

	for (i = 0; i < gf_list_count(elements); i++) {
		SVGGenElement *elt = gf_list_get(elements, i);
		fprintf(f, "<h2 id='table_%s'>%s</h2>\n", elt->implementation_name, elt->svg_name);
		fprintf(f, "<table>\n");
		fprintf(f, "<thead>\n");
		fprintf(f, "<tr>\n");
		fprintf(f, "<th>Attribute Name</th>\n");
		fprintf(f, "<th>Status</th>\n");
		fprintf(f, "<th>Observations</th>\n");
		fprintf(f, "<th>Example(s)</th>\n");
		fprintf(f, "<th>Bug(s)</th>\n");
		fprintf(f, "</tr>\n");
		fprintf(f, "</thead>\n");
		fprintf(f, "<tbody>\n");
		for (j = 0; j < gf_list_count(elt->attributes); j++) {
			SVGGenAttribute *att = gf_list_get(elt->attributes, j);
			if (!strcmp(att->svg_name, "textContent")) continue;
			fprintf(f, "<tr>\n");
			fprintf(f, "<td class='attribute-name'>%s</td>\n",att->svg_name);
			fprintf(f, "<td class='not-supported'></td>\n");
			fprintf(f, "<td class='attribute-observation'>&nbsp;</td>\n");
			fprintf(f, "<td class='attribute-example'>%d - &nbsp;</td>\n",++nbExamples);
			fprintf(f, "<td class='attribute-bug'>&nbsp;</td>\n");
			fprintf(f, "</tr>\n");
		}
		fprintf(f, "</tbody>\n");
		fprintf(f, "</table>\n");
	}

	EndHtml(f);
	gf_list_del(elements);
}

