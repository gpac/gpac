/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/config_file.h>
#include <gpac/list.h>

#define MAX_INI_LINE			2046

typedef struct
{
	char *name;
	char *value;
} IniKey;

typedef struct
{
	char *section_name;
	GF_List *keys;
} IniSection;

struct __tag_config
{
	char *fileName;
	GF_List *sections;
	Bool hasChanged, skip_changes;
};


static void DelSection(IniSection *ptr)
{
	IniKey *k;
	if (!ptr) return;
	if (ptr->keys) {
		while (gf_list_count(ptr->keys)) {
			k = (IniKey *) gf_list_get(ptr->keys, 0);
			if (k->value) gf_free(k->value);
			if (k->name) gf_free(k->name);
			gf_free(k);
			gf_list_rem(ptr->keys, 0);
		}
		gf_list_del(ptr->keys);
	}
	if (ptr->section_name) gf_free(ptr->section_name);
	gf_free(ptr);
}

/*!
 * \brief Clear the structure
 * \param iniFile The structure to clear
 */
static void gf_cfg_clear(GF_Config * iniFile) {
	IniSection *p;
	if (!iniFile) return;
	if (iniFile->sections) {
		while (gf_list_count(iniFile->sections)) {
			p = (IniSection *) gf_list_get(iniFile->sections, 0);
			DelSection(p);
			gf_list_rem(iniFile->sections, 0);
		}
		gf_list_del(iniFile->sections);
	}
	if (iniFile->fileName)
		gf_free(iniFile->fileName);
	memset((void *)iniFile, 0, sizeof(GF_Config));
}

/*!
 * \brief Parses the config file if any and clears the existing structure
 */
GF_Err gf_cfg_parse_config_file(GF_Config * tmp, const char * filePath, const char * file_name)
{
	IniSection *p;
	IniKey *k;
	FILE *file;
	char *ret;
	char *line;
	u32 line_alloc = MAX_INI_LINE;
	char fileName[GF_MAX_PATH];

	gf_cfg_clear(tmp);

	if (filePath && ((filePath[strlen(filePath)-1] == '/') || (filePath[strlen(filePath)-1] == '\\')) ) {
		strcpy(fileName, filePath);
		strcat(fileName, file_name);
	} else if (filePath) {
		sprintf(fileName, "%s%c%s", filePath, GF_PATH_SEPARATOR, file_name);
	} else {
		strcpy(fileName, file_name);
	}

	tmp->fileName = gf_strdup(fileName);
	tmp->sections = gf_list_new();
	file = gf_fopen(fileName, "rt");
	if (!file)
		return GF_IO_ERR;
	/* load the file */
	p = NULL;
	line = (char*)gf_malloc(sizeof(char)*line_alloc);
	memset(line, 0, sizeof(char)*line_alloc);

	while (!feof(file)) {
		u32 read, nb_pass;
		ret = fgets(line, line_alloc, file);
		read = (u32) strlen(line);
		nb_pass = 1;
		while (read + nb_pass == line_alloc) {
			line_alloc += MAX_INI_LINE;
			line = (char*)gf_realloc(line, sizeof(char)*line_alloc);
			ret = fgets(line+read, MAX_INI_LINE, file);
			read = (u32) strlen(line);
			nb_pass++;
		}
		if (!ret) continue;

		/* get rid of the end of line stuff */
		while (1) {
			u32 len = (u32) strlen(line);
			if (!len) break;
			if ((line[len-1] != '\n') && (line[len-1] != '\r')) break;
			line[len-1] = 0;
		}
		if (!strlen(line)) continue;
		if (line[0] == '#') continue;


		/* new section */
		if (line[0] == '[') {
			p = (IniSection *) gf_malloc(sizeof(IniSection));
			p->keys = gf_list_new();
			p->section_name = gf_strdup(line + 1);
			p->section_name[strlen(line) - 2] = 0;
			while (p->section_name[strlen(p->section_name) - 1] == ']' || p->section_name[strlen(p->section_name) - 1] == ' ') p->section_name[strlen(p->section_name) - 1] = 0;
			gf_list_add(tmp->sections, p);
		}
		else if (strlen(line) && (strchr(line, '=') != NULL) ) {
			if (!p) {
				gf_list_del(tmp->sections);
				gf_free(tmp->fileName);
				gf_free(tmp);
				gf_fclose(file);
				gf_free(line);
				return GF_IO_ERR;
			}

			k = (IniKey *) gf_malloc(sizeof(IniKey));
			memset((void *)k, 0, sizeof(IniKey));
			ret = strchr(line, '=');
			if (ret) {
				ret[0] = 0;
				k->name = gf_strdup(line);
				while (k->name[strlen(k->name) - 1] == ' ') k->name[strlen(k->name) - 1] = 0;
				ret[0] = '=';
				ret += 1;
				while (ret[0] == ' ') ret++;
				if ( ret[0] != 0) {
					k->value = gf_strdup(ret);
					while (k->value[strlen(k->value) - 1] == ' ') k->value[strlen(k->value) - 1] = 0;
				} else {
					k->value = gf_strdup("");
				}
			}
			gf_list_add(p->keys, k);
		}
	}
	gf_free(line);
	gf_fclose(file);
	return GF_OK;
}

GF_EXPORT
GF_Config *gf_cfg_force_new(const char *filePath, const char* file_name) {
	GF_Config *tmp = (GF_Config *)gf_malloc(sizeof(GF_Config));
	memset((void *)tmp, 0, sizeof(GF_Config));
	gf_cfg_parse_config_file(tmp, filePath, file_name);
	return tmp;
}


GF_EXPORT
GF_Config *gf_cfg_new(const char *filePath, const char* file_name)
{
	GF_Config *tmp = (GF_Config *)gf_malloc(sizeof(GF_Config));
	memset((void *)tmp, 0, sizeof(GF_Config));
	if (!filePath && !file_name) {
		tmp->sections = gf_list_new();
		return tmp;
	}

	if (gf_cfg_parse_config_file(tmp, filePath, file_name)) {
		gf_cfg_clear(tmp);
		gf_free(tmp);
		tmp = NULL;
	}
	return tmp;
}

GF_EXPORT
char * gf_cfg_get_filename(GF_Config *iniFile)
{
	if (!iniFile)
		return NULL;
	return iniFile->fileName ? gf_strdup(iniFile->fileName) : NULL;
}

GF_EXPORT
GF_Err gf_cfg_save(GF_Config *iniFile)
{
	u32 i, j;
	IniSection *sec;
	IniKey *key;
	FILE *file;

	if (!iniFile->hasChanged) return GF_OK;
	if (iniFile->skip_changes) return GF_OK;
	if (!iniFile->fileName) return GF_OK;

	file = gf_fopen(iniFile->fileName, "wt");
	if (!file) return GF_IO_ERR;

	i=0;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i)) ) {
		/*Temporary sections are not saved*/
		if (!strnicmp(sec->section_name, "Temp", 4)) continue;

		fprintf(file, "[%s]\n", sec->section_name);
		j=0;
		while ( (key = (IniKey *) gf_list_enum(sec->keys, &j)) ) {
			fprintf(file, "%s=%s\n", key->name, key->value);
		}
		/* end of section */
		fprintf(file, "\n");
	}
	gf_fclose(file);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_cfg_discard_changes(GF_Config *iniFile)
{
	if (!iniFile) return GF_BAD_PARAM;
	iniFile->skip_changes = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
void gf_cfg_del(GF_Config *iniFile)
{
	if (!iniFile) return;
	gf_cfg_save(iniFile);
	gf_cfg_clear(iniFile);
	gf_free(iniFile);
}

GF_EXPORT
void gf_cfg_remove(GF_Config *iniFile)
{
	if (!iniFile) return;
	gf_delete_file(iniFile->fileName);
	gf_cfg_clear(iniFile);
	gf_free(iniFile);
}


GF_EXPORT
const char *gf_cfg_get_key(GF_Config *iniFile, const char *secName, const char *keyName)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	i=0;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) goto get_key;
	}
	return NULL;

get_key:
	i=0;
	while ( (key = (IniKey *) gf_list_enum(sec->keys, &i)) ) {
		if (!strcmp(key->name, keyName)) return key->value;
	}
	return NULL;
}

GF_EXPORT
const char *gf_cfg_get_ikey(GF_Config *iniFile, const char *secName, const char *keyName)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	i=0;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i)) ) {
		if (!stricmp(secName, sec->section_name)) goto get_key;
	}
	return NULL;

get_key:
	i=0;
	while ( (key = (IniKey *) gf_list_enum(sec->keys, &i)) ) {
		if (!stricmp(key->name, keyName)) return key->value;
	}
	return NULL;
}


GF_EXPORT
GF_Err gf_cfg_set_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue)
{
	u32 i;
	Bool has_changed = GF_TRUE;
	IniSection *sec;
	IniKey *key;

	if (!iniFile || !secName || !keyName) return GF_BAD_PARAM;

	if (!strnicmp(secName, "temp", 4)) has_changed = GF_FALSE;

	i=0;
	while ((sec = (IniSection *) gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) goto get_key;
	}
	/* need a new key */
	sec = (IniSection *) gf_malloc(sizeof(IniSection));
	sec->section_name = gf_strdup(secName);
	sec->keys = gf_list_new();
	if (has_changed) iniFile->hasChanged = GF_TRUE;
	gf_list_add(iniFile->sections, sec);

get_key:
	i=0;
	while ( (key = (IniKey *) gf_list_enum(sec->keys, &i) ) ) {
		if (!strcmp(key->name, keyName)) goto set_value;
	}
	if (!keyValue) return GF_OK;
	/* need a new key */
	key = (IniKey *) gf_malloc(sizeof(IniKey));
	key->name = gf_strdup(keyName);
	key->value = gf_strdup("");
	if (has_changed) iniFile->hasChanged = GF_TRUE;
	gf_list_add(sec->keys, key);

set_value:
	if (!keyValue) {
		gf_list_del_item(sec->keys, key);
		if (key->name) gf_free(key->name);
		if (key->value) gf_free(key->value);
		gf_free(key);
		if (has_changed) iniFile->hasChanged = GF_TRUE;
		return GF_OK;
	}
	/* same value, don't update */
	if (!strcmp(key->value, keyValue)) return GF_OK;

	if (key->value) gf_free(key->value);
	key->value = gf_strdup(keyValue);
	if (has_changed) iniFile->hasChanged = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
u32 gf_cfg_get_section_count(GF_Config *iniFile)
{
	return gf_list_count(iniFile->sections);
}

GF_EXPORT
const char *gf_cfg_get_section_name(GF_Config *iniFile, u32 secIndex)
{
	IniSection *is = (IniSection *) gf_list_get(iniFile->sections, secIndex);
	if (!is) return NULL;
	return is->section_name;
}

GF_EXPORT
u32 gf_cfg_get_key_count(GF_Config *iniFile, const char *secName)
{
	u32 i = 0;
	IniSection *sec;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) return gf_list_count(sec->keys);
	}
	return 0;
}

GF_EXPORT
const char *gf_cfg_get_key_name(GF_Config *iniFile, const char *secName, u32 keyIndex)
{
	u32 i = 0;
	IniSection *sec;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i) ) ) {
		if (!strcmp(secName, sec->section_name)) {
			IniKey *key = (IniKey *) gf_list_get(sec->keys, keyIndex);
			return key ? key->name : NULL;
		}
	}
	return NULL;
}

GF_EXPORT
void gf_cfg_del_section(GF_Config *iniFile, const char *secName)
{
	u32 i;
	IniSection *p;
	if (!iniFile) return;

	i = 0;
	while ((p = (IniSection*)gf_list_enum(iniFile->sections, &i))) {
		if (!strcmp(secName, p->section_name)) {
			DelSection(p);
			gf_list_rem(iniFile->sections, i-1);
			iniFile->hasChanged = GF_TRUE;
			return;
		}
	}
}

GF_EXPORT
GF_Err gf_cfg_insert_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue, u32 index)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	if (!iniFile || !secName || !keyName|| !keyValue) return GF_BAD_PARAM;

	i=0;
	while ( (sec = (IniSection *) gf_list_enum(iniFile->sections, &i) ) ) {
		if (!strcmp(secName, sec->section_name)) break;
	}
	if (!sec) return GF_BAD_PARAM;

	i=0;
	while ( (key = (IniKey *) gf_list_enum(sec->keys, &i) ) ) {
		if (!strcmp(key->name, keyName)) return GF_BAD_PARAM;
	}

	key = (IniKey *) gf_malloc(sizeof(IniKey));
	key->name = gf_strdup(keyName);
	key->value = gf_strdup(keyValue);
	gf_list_insert(sec->keys, key, index);
	iniFile->hasChanged = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
const char *gf_cfg_get_sub_key(GF_Config *iniFile, const char *secName, const char *keyName, u32 sub_index)
{
	u32 j;
	char *subKeyValue, *returnKey;
	char *keyValue;


	keyValue = gf_strdup(gf_cfg_get_key(iniFile, secName, keyName));
	if (!keyValue) {
		return NULL;
	}

	j = 0;
	subKeyValue = strtok((char*)keyValue,";");
	while (subKeyValue!=NULL) {
		if (j==sub_index) {
			returnKey = gf_strdup(subKeyValue);
			gf_free(keyValue);
			return returnKey;
		}
		j++;
		subKeyValue = strtok (NULL, ";");
	}
	gf_free(keyValue);
	return NULL;
}

GF_EXPORT
GF_Err gf_cfg_set_filename(GF_Config *iniFile, const char * fileName)
{
	if (!fileName) return GF_OK;
	if (iniFile->fileName) gf_free(iniFile->fileName);
	iniFile->fileName = gf_strdup(fileName);
	return iniFile->fileName ? GF_OK : GF_OUT_OF_MEM;
}
