/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#include <gpac/module.h>
#include <gpac/list.h>


#define MAX_SECTION_NAME		500
#define MAX_INI_LINE			2046

typedef struct
{
	char name[MAX_SECTION_NAME];
	char value[MAX_INI_LINE];
} IniKey;

typedef struct
{
	char section_name[MAX_SECTION_NAME];
	GF_List *keys;
} IniSection;

struct __tag_config
{
	char *fileName;
	char *filePath;
	GF_List *sections;
	Bool hasChanged;
};

GF_Config *gf_cfg_new(const char *filePath, const char* file_name)
{
	IniSection *p;
	IniKey *k;
	u32 i;
	FILE *file;
	char *ret;
	char fileName[GF_MAX_PATH];
	char line[MAX_INI_LINE];
	GF_Config *tmp;

	if (filePath) {
		if (filePath[strlen(filePath)-1] == GF_PATH_SEPARATOR) {
			strcpy(fileName,filePath);
			strcat(fileName, file_name);
		} else {
			sprintf(fileName, "%s%c%s", filePath, GF_PATH_SEPARATOR, file_name);
		}
	} else {
		strcpy(fileName,file_name);
	}
	file = fopen(fileName, "rt");
	if (!file) return NULL;

	tmp = malloc(sizeof(GF_Config));
	memset(tmp, 0, sizeof(GF_Config));

	tmp->filePath = malloc(sizeof(char) * (strlen(filePath)+1));
	strcpy(tmp->filePath, filePath ? filePath : "");
	tmp->fileName = malloc(sizeof(char) * (strlen(fileName)+1));
	strcpy(tmp->fileName, fileName);
	tmp->sections = gf_list_new();

	//load the file
	p = NULL;

	while (!feof(file)) {
		ret = fgets(line, MAX_INI_LINE, file);

		if (!ret) continue;
		if (!strlen(line)) continue;
		if (line[0] == '#') continue;

		//get rid of the end of line stuff
		while ((strlen(line) > 0) && ((line[strlen(line)-1] == '\n') || (line[strlen(line)-1] == '\r')) )
			line[strlen(line)-1] = 0;

		
		//new section
		if (line[0] == '[') {
			p = malloc(sizeof(IniSection));
			p->keys = gf_list_new();
			strcpy(p->section_name, line + 1);
			p->section_name[strlen(line) - 2] = 0;
			while (p->section_name[strlen(p->section_name) - 1] == ']' || p->section_name[strlen(p->section_name) - 1] == ' ') p->section_name[strlen(p->section_name) - 1] = 0;
			gf_list_add(tmp->sections, p);
		}
		else if (strlen(line) && (strchr(line, '=') != NULL) ) {
			if (!p) {
				gf_list_del(tmp->sections);
				free(tmp->fileName);
				free(tmp->filePath);
				free(tmp);
				fclose(file);
				return NULL;
			}
			k = malloc(sizeof(IniKey));
			i = 0;
			while (line[i] != '=') {
				k->name[i] = line[i];
				i++;
			}
			k->name[i] = 0;
			while (k->name[strlen(k->name) - 1] == ' ') k->name[strlen(k->name) - 1] = 0;
			i++;
			while (line[i] == ' ') i++;
			strcpy(k->value, line + i);
			while (k->value[strlen(k->value) - 1] == ' ') k->value[strlen(k->value) - 1] = 0;
			gf_list_add(p->keys, k);
		}
	}
	fclose(file);
	return tmp;
}

void DelSection(IniSection *ptr)
{
	IniKey *k;
	if (!ptr) return;

	while (gf_list_count(ptr->keys)) {
		k = gf_list_get(ptr->keys, 0);
		free(k);
		gf_list_rem(ptr->keys, 0);
	}
	gf_list_del(ptr->keys);
	free(ptr);
}


GF_Err WriteIniFile(GF_Config *iniFile)
{
	u32 i, j;
	IniSection *sec;
	IniKey *key;
	FILE *file;

	if (!iniFile->hasChanged) return GF_OK;

	file = fopen(iniFile->fileName, "wt");
	if (!file) return GF_IO_ERR;

	i=0;
	while ( (sec = gf_list_enum(iniFile->sections, &i)) ) {
		fprintf(file, "[%s]\n", sec->section_name);
		j=0;
		while ( (key = gf_list_enum(sec->keys, &j)) ) {
			fprintf(file, "%s=%s\n", key->name, key->value);
		}
		//end of section
		fprintf(file, "\n");
	}
	fclose(file);
	return GF_OK;
}

void gf_cfg_del(GF_Config *iniFile)
{
	IniSection *p;
	if (!iniFile) return;

	WriteIniFile(iniFile);
	while (gf_list_count(iniFile->sections)) {
		p = gf_list_get(iniFile->sections, 0);
		DelSection(p);
		gf_list_rem(iniFile->sections, 0);
	}
	gf_list_del(iniFile->sections);
	free(iniFile->fileName);
	free(iniFile->filePath);
	free(iniFile);
}

char *IF_GetPath(GF_Config *iniFile)
{
	return iniFile->filePath;
}


const char *gf_cfg_get_key(GF_Config *iniFile, const char *secName, const char *keyName)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	i=0;
	while ( (sec = gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) goto get_key;
	}
	return NULL;

get_key:
	i=0;
	while ( (key = gf_list_enum(sec->keys, &i)) ) {
		if (!strcmp(key->name, keyName)) return key->value;
	}
	return NULL;
}



GF_Err gf_cfg_set_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	if (!iniFile || !secName || !keyName) return GF_BAD_PARAM;

	i=0;
	while ((sec = gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) goto get_key;
	}
	//need a new key
	sec = malloc(sizeof(IniSection));
	strcpy(sec->section_name, secName);
	sec->keys = gf_list_new();
	iniFile->hasChanged = 1;
	gf_list_add(iniFile->sections, sec);

get_key:
	i=0;
	while ( (key = gf_list_enum(sec->keys, &i) ) ) {
		if (!strcmp(key->name, keyName)) goto set_value;
	}
	if (!keyValue) return GF_OK;
	//need a new key
	key = malloc(sizeof(IniKey));
	strcpy(key->name, keyName);
	strcpy(key->value, "");
	iniFile->hasChanged = 1;
	gf_list_add(sec->keys, key);

set_value:
	if (!keyValue) {
		gf_list_del_item(sec->keys, key);
		free(key);
		iniFile->hasChanged = 1;
		return GF_OK;
	}
	//same value, don't update
	if (!strcmp(key->value, keyValue)) return GF_OK;

	strcpy(key->value, keyValue);
	iniFile->hasChanged = 1;
	return GF_OK;
}

u32 gf_cfg_get_section_count(GF_Config *iniFile)
{
	return gf_list_count(iniFile->sections);
}
const char *gf_cfg_get_section_name(GF_Config *iniFile, u32 secIndex)
{
	IniSection *is = gf_list_get(iniFile->sections, secIndex);
	if (!is) return NULL;
	return is->section_name;
}
u32 gf_cfg_get_key_count(GF_Config *iniFile, const char *secName)
{
	u32 i = 0;
	IniSection *sec;
	while ( (sec = gf_list_enum(iniFile->sections, &i)) ) {
		if (!strcmp(secName, sec->section_name)) return gf_list_count(sec->keys);
	}
	return 0;
}

const char *gf_cfg_get_key_name(GF_Config *iniFile, const char *secName, u32 keyIndex)
{
	u32 i = 0;
	IniSection *sec;
	while ( (sec = gf_list_enum(iniFile->sections, &i) ) ) {
		if (!strcmp(secName, sec->section_name)) {
			IniKey *key = gf_list_get(sec->keys, keyIndex);
			return key ? key->name : NULL;
		}
	}
	return NULL;
}

GF_Err gf_cfg_insert_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue, u32 index)
{
	u32 i;
	IniSection *sec;
	IniKey *key;

	if (!iniFile || !secName || !keyName|| !keyValue) return GF_BAD_PARAM;

	i=0;
	while ( (sec = gf_list_enum(iniFile->sections, &i) ) ) {
		if (!strcmp(secName, sec->section_name)) break;
	}
	if (!sec) return GF_BAD_PARAM;

	i=0;
	while ( (key = gf_list_enum(sec->keys, &i) ) ) {
		if (!strcmp(key->name, keyName)) return GF_BAD_PARAM;
	}

	key = malloc(sizeof(IniKey));
	strcpy(key->name, keyName);
	strcpy(key->value, keyValue);
	gf_list_insert(sec->keys, key, index);
	iniFile->hasChanged = 1;
	return GF_OK;
}

