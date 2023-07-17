/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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
	Bool do_restrict;
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
	if (!ptr) return;
	if (ptr->keys) {
		u32 i, count=gf_list_count(ptr->keys);
		for (i=0;i<count;i++) {
			IniKey *k = (IniKey *) gf_list_get(ptr->keys, i);
			if (k->value) gf_free(k->value);
			if (k->name) gf_free(k->name);
			gf_free(k);
		}
		gf_list_del(ptr->keys);
	}
	if (ptr->section_name) gf_free(ptr->section_name);
	gf_free(ptr);
}

/*!
 * \brief Clear the structure
\param iniFile The structure to clear
 */
static void gf_cfg_clear(GF_Config * iniFile)
{
	if (!iniFile) return;
	if (iniFile->sections) {
		u32 i, count=gf_list_count(iniFile->sections);
		for (i=0; i<count; i++) {
			IniSection *p = (IniSection *) gf_list_get(iniFile->sections, i);
			DelSection(p);
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
	IniKey *k=NULL;
	FILE *file;
	char *line;
	u32 line_alloc = MAX_INI_LINE;
	char fileName[GF_MAX_PATH];
	Bool line_done = GF_FALSE;
	u32 nb_empty_lines=0;
	Bool is_opts_probe = GF_FALSE;
	Bool in_multiline = GF_FALSE;
	gf_cfg_clear(tmp);

	if (strstr(file_name, "/all_opts.txt")) {
		if (filePath && !strcmp(filePath, "probe")) {
			filePath = NULL;
			is_opts_probe = GF_TRUE;
		}
	}

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

#define FLUSH_EMPTY_LINES \
			if (k&& nb_empty_lines) {\
				u32 klen = (u32) strlen(k->value)+1+nb_empty_lines;\
				k->value = gf_realloc(k->value, sizeof(char)*klen);\
				while (nb_empty_lines) {\
					nb_empty_lines--;\
					strcat(k->value, "\n");\
				}\
				k=NULL;\
			}\
			nb_empty_lines=0;\

	while (!gf_feof(file)) {
		char *ret;
		u32 read, nb_pass;
		char *fsep;
		ret = gf_fgets(line, line_alloc, file);
		read = (u32) strlen(line);

		nb_pass = 1;
		while (read + nb_pass == line_alloc) {
			line_alloc += MAX_INI_LINE;
			line = (char*)gf_realloc(line, sizeof(char)*line_alloc);
			ret = gf_fgets(line+read, MAX_INI_LINE, file);
			read = (u32) strlen(line);
			nb_pass++;
		}
		if (!ret) continue;

		/* get rid of the end of line stuff */
		fsep = strchr(line, '=');
		if (!fsep || (fsep[1]!='@')) {
			while (!in_multiline) {
				u32 len = (u32) strlen(line);
				if (!len) break;
				if ((line[len-1] != '\n') && (line[len-1] != '\r')) break;
				line[len-1] = 0;
			}
		}
		if (!strlen(line)) {
			if (k) nb_empty_lines++;
			continue;
		}
		if (line[0] == '#') {
			FLUSH_EMPTY_LINES
			continue;
		}

		/* new section */
		if (line[0] == '[') {
			//compat with old arch
			if (nb_empty_lines) nb_empty_lines--;

			FLUSH_EMPTY_LINES

			p = (IniSection *) gf_malloc(sizeof(IniSection));
			p->keys = gf_list_new();
			p->section_name = gf_strdup(line + 1);
			p->section_name[strlen(line) - 2] = 0;
			while (p->section_name[strlen(p->section_name) - 1] == ']' || p->section_name[strlen(p->section_name) - 1] == ' ') p->section_name[strlen(p->section_name) - 1] = 0;
			gf_list_add(tmp->sections, p);
		}
		else if (strlen(line) && !in_multiline && (strchr(line, '=') != NULL) ) {
			if (!p) {
				gf_fclose(file);
				gf_free(line);
				gf_cfg_clear(tmp);
				return GF_IO_ERR;
			}
			FLUSH_EMPTY_LINES

			k = (IniKey *) gf_malloc(sizeof(IniKey));
			memset((void *)k, 0, sizeof(IniKey));
			ret = strchr(line, '=');
			if (ret) {
				ret[0] = 0;
				k->name = gf_strdup(line);
				while (k->name[0] && (k->name[strlen(k->name) - 1] == ' '))
					k->name[strlen(k->name) - 1] = 0;
				ret[0] = '=';
				ret += 1;
				while (ret[0] == ' ') ret++;
				if (ret[0]=='@') {
					ret++;
					in_multiline = GF_TRUE;
				}
				if ( ret[0] != 0) {
					k->value = gf_strdup(ret);
					while (k->value[strlen(k->value) - 1] == ' ') k->value[strlen(k->value) - 1] = 0;
				} else {
					k->value = gf_strdup("");
				}
				line_done = GF_FALSE;
			}
			gf_list_add(p->keys, k);
			if (line_done) k=NULL;
			line_done=GF_FALSE;

			if (is_opts_probe) {
				if (!strcmp(p->section_name, "version") && !strcmp(k->name, "version")) {
					break;
				}
			}

		} else if (k) {
			u32 l1 = (u32) strlen(k->value);
			u32 l2 = (u32) strlen(line);
			k->value = gf_realloc(k->value, sizeof(char) * (l1 + l2 + 1 + nb_empty_lines) );
			l2 += l1 + nb_empty_lines;
			while (nb_empty_lines) {
				strcat(k->value, "\n");
				nb_empty_lines--;
			}
			strcat(k->value, line);
			l2 -= 2; //@ and \n
			if (k->value[l2] == '@') {
				k->value[l2] = 0;
				k=NULL;
				in_multiline = GF_FALSE;
			}
		} else {
			k = NULL;
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
const char * gf_cfg_get_filename(GF_Config *iniFile)
{
	if (!iniFile)
		return NULL;
	return iniFile->fileName;
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
		if (!strnicmp(sec->section_name, "temp", 4)) continue;

		gf_fprintf(file, "[%s]\n", sec->section_name);
		j=0;
		while ( (key = (IniKey *) gf_list_enum(sec->keys, &j)) ) {
			if (strchr(key->value, '\n')) {
				gf_fprintf(file, "%s=@%s@\n", key->name, key->value);
			} else {
				gf_fprintf(file, "%s=%s\n", key->name, key->value);
			}
		}
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

#if 0 //unused
void gf_cfg_remove(GF_Config *iniFile)
{
	if (!iniFile) return;
	gf_file_delete(iniFile->fileName);
	gf_cfg_clear(iniFile);
	gf_free(iniFile);
}
#endif

const char *gf_cfg_get_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, Bool restricted_only)
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
		if (!strcmp(key->name, keyName)) {
			if (!restricted_only || key->do_restrict)
				return key->value;
			return NULL;
		}
	}
	return NULL;
}
GF_EXPORT
const char *gf_cfg_get_key(GF_Config *iniFile, const char *secName, const char *keyName)
{
	return gf_cfg_get_key_internal(iniFile, secName, keyName, GF_FALSE);
}

#if 0 //unused
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
#endif

GF_Err gf_cfg_set_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue, Bool is_restrict)
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
	if (has_changed)
		iniFile->hasChanged = GF_TRUE;
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
	key->value = gf_strdup(keyValue);
	if (has_changed)
		iniFile->hasChanged = GF_TRUE;
	gf_list_add(sec->keys, key);
	key->do_restrict = is_restrict;
	return GF_OK;

set_value:
	if (!keyValue) {
		gf_list_del_item(sec->keys, key);
		if (key->name) gf_free(key->name);
		if (key->value) gf_free(key->value);
		gf_free(key);
		if (has_changed)
			iniFile->hasChanged = GF_TRUE;
		return GF_OK;
	}
	key->do_restrict = is_restrict;
	/* same value, don't update */
	if (!strcmp(key->value, keyValue)) return GF_OK;

	if (key->value) gf_free(key->value);
	key->value = gf_strdup(keyValue);
	if (has_changed)
		iniFile->hasChanged = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_cfg_set_key(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue)
{
	return gf_cfg_set_key_internal(iniFile, secName, keyName, keyValue, GF_FALSE);

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

#if 0 //unused
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
#endif

#if 0 //unused
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
#endif

GF_Err gf_cfg_set_filename(GF_Config *iniFile, const char * fileName)
{
	if (!fileName) return GF_OK;
	if (iniFile->fileName) gf_free(iniFile->fileName);
	iniFile->fileName = gf_strdup(fileName);
	return iniFile->fileName ? GF_OK : GF_OUT_OF_MEM;
}


