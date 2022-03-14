//This software module was originally developed by TelecomParisTech in the
//course of the development of MPEG-U Widgets (ISO/IEC 23007-1) standard.
//
//This software module is an implementation of a part of one or
//more MPEG-U Widgets (ISO/IEC 23007-1) tools as specified by the MPEG-U Widgets
//(ISO/IEC 23007-1) standard. ISO/IEC gives users of the MPEG-U Widgets
//(ISO/IEC 23007-1) free license to this software module or modifications
//thereof for use in hardware or software products claiming conformance to
//the MPEG-U Widgets (ISO/IEC 23007-1). Those intending to use this software
//module in hardware or software products are advised that its use may
//infringe existing patents.
//The original developer of this software module and his/her company, the
//subsequent editors and their companies, and ISO/IEC have no liability
//for use of this software module or modifications thereof in an implementation.
//Copyright is not released for non MPEG-U Widgets (ISO/IEC 23007-1) conforming
//products.
//Telecom ParisTech retains full right to use the code for his/her own purpose,
//assign or donate the code to a third party and to inhibit third parties from
//using the code for non MPEG-U Widgets (ISO/IEC 23007-1) conforming products.
//
//This copyright notice must be included in all copies or derivative works.
//
//Copyright (c) 2009 Telecom ParisTech.
//
// Alternatively, this software module may be redistributed and/or modified
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.
//
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:
//					Jean Le Feuvre, Telecom ParisTech
//					Cyril Concolato, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////

#include "widgetman.h"

#ifdef GPAC_HAS_SPIDERMONKEY

#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "js32")
# elif defined (_WIN64)
#  pragma comment(lib, "js")
# elif defined (WIN32)
#  pragma comment(lib, "js")
# endif
#endif


JSBool gf_sg_js_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);
JSBool gf_sg_js_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node);



static Bool is_same_path(const char *p1, const char *p2, u32 len)
{
	char c1, c2;
	u32 i=0;
	do {
		if (len && (i==len))
			break;
		c1 = p1[i];
		c2 = p2[i];
		if (p1[i] != p2[i]) {
			if ((c1=='/') && (c2=='\\')) {}
			else if ((c1=='\\') && (c2=='/')) {}
			else return GF_FALSE;
		}
		i++;
	} while (c1);

	return GF_TRUE;
}

static void widget_package_extract_file(GF_WidgetPackage *wpack, GF_WidgetPackageResource *res)
{
	u32 i;

	if (wpack->is_zip) {
		unz_global_info gi;
		unzFile uf = unzOpen2(wpack->package_path, NULL);
		if (!uf) return;

		unzGetGlobalInfo(uf, &gi);
		for (i=0; i<gi.number_entry; i++)  {
			char buf[8192];
			int err;
			FILE *fout;
			char filename_inzip[256];

			unz_file_info file_info;
			unzGetCurrentFileInfo(uf, &file_info, filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

			if (strcmp(res->inner_path, filename_inzip)) {
				if ((i+1)<gi.number_entry)
					unzGoToNextFile(uf);
				continue;
			}

			unzOpenCurrentFile3(uf, NULL, NULL, 0, NULL/*password*/);

			fout=gf_fopen(res->extracted_path, "wb");
			if (!fout) break;
			do {
				err = unzReadCurrentFile(uf,buf,8192);
				if (err<0) break;
				if (err>0)
					if (gf_fwrite(buf,err,1,fout)!=1) {
						//err=UNZ_ERRNO;
						break;
					}
			} while (err>0);
			if (fout) gf_fclose(fout);

			res->extracted = GF_TRUE;
			break;
		}
		unzClose(uf);
#ifndef GPAC_DISABLE_ISOM
	} else {
		u32 count;
		GF_ISOFile *isom = gf_isom_open(wpack->package_path, GF_ISOM_OPEN_READ, 0);
		if (!isom ) return;

		count = gf_isom_get_meta_item_count(isom, GF_TRUE, 0);
		for (i=0; i<count; i++)  {
			u32 ID;
			const char *url, *urn, *enc;
			Bool self_ref;
			const char *item_name;

			gf_isom_get_meta_item_info(isom, GF_TRUE, 0, i+1, &ID, NULL, NULL, &self_ref, &item_name, NULL, &enc, &url, &urn);
			if (strcmp(res->inner_path, item_name)) continue;

			gf_isom_extract_meta_item(isom, GF_TRUE, 0, ID, res->extracted_path);
			res->extracted = GF_TRUE;
			break;
		}
		gf_isom_close(isom);
#endif /*GPAC_DISABLE_ISOM*/
	}

}

static Bool package_find_res(GF_WidgetPackage *wpack, char *res_path, char *relocated_path, char *localized_rel_path)
{
	u32 count, i;
	count = gf_list_count(wpack->resources);
	for (i=0; i<count; i++) {
		GF_WidgetPackageResource *pack_res = (GF_WidgetPackageResource*)gf_list_get(wpack->resources, i);
		if (is_same_path(res_path, pack_res->inner_path, 0)) {
			strcpy(localized_rel_path, res_path);
			strcpy(relocated_path, pack_res->extracted_path);
			if (!pack_res->extracted) widget_package_extract_file(wpack, pack_res);
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

/* Checks if a resource in the package has the given rel_path, potentially in a localized sub-folder */
static Bool widget_package_relocate_uri(void *__self, const char *parent_uri, const char *rel_path, char *relocated_path, char *localized_rel_path)
{
	char path[GF_MAX_PATH];
	const char *opt;
	GF_WidgetPackage *wpack = (GF_WidgetPackage *)__self;

	assert(parent_uri);
	/*resource belongs to our archive*/
	if (strstr(rel_path, wpack->archive_id)) {
		rel_path = strstr(rel_path, wpack->archive_id) + strlen(wpack->archive_id);
	}
	/*parent resource belongs to our archive*/
	else if (strstr(parent_uri, wpack->archive_id)) {
	}
	/*resource doesn't belong to our archive*/
	else {
		return GF_FALSE;
	}

	/* First try to locate the resource in the locales folder */
	opt = gf_opts_get_key("core", "lang");
	if (opt) {
		if (!strcmp(opt, "*") || !strcmp(opt, "un") )
			opt = NULL;
	}

	while (opt) {
		char lan[100];
		char *sep;
		char *sep_lang = strchr(opt, ';');
		if (sep_lang) sep_lang[0] = 0;

		while (strchr(" \t", opt[0]))
			opt++;
		strcpy(lan, opt);

		if (sep_lang) {
			sep_lang[0] = ';';
			opt = sep_lang+1;
		} else {
			opt = NULL;
		}

		while (1) {
			sep = strstr(lan, "-*");
			if (!sep) break;
			strncpy(sep, sep+2, strlen(sep)-2);
		}

		sprintf(path, "locales/%s/%s", lan, rel_path);
		if (package_find_res(wpack, path, relocated_path, localized_rel_path))
			return GF_TRUE;

		/*recursively remove region (sub)tags*/
		while (1) {
			sep = strrchr(lan, '-');
			if (!sep) break;
			sep[0] = 0;
			sprintf(path, "locales/%s/%s", lan, rel_path);
			if (package_find_res(wpack, path, relocated_path, localized_rel_path))
				return GF_TRUE;
		}
	}

	/*no locale*/
	if (package_find_res(wpack, (char*)rel_path, relocated_path, localized_rel_path))
		return GF_TRUE;

	strcpy(localized_rel_path, "");
	strcpy(relocated_path, "");
	return GF_FALSE;
}


static GF_WidgetPackage *widget_isom_new(GF_WidgetManager *wm, const char *path)
{
#ifdef GPAC_DISABLE_ISOM
	GF_LOG(GF_LOG_ERROR, GF_LOG_MODULE, ("[Widgetman] GPAC was compiled without ISO File Format support\n"));
	return NULL;
#else
	GF_WidgetPackageResource *pack_res;
	char szPath[GF_MAX_PATH];
	const char *dir;
	GF_WidgetPackage *wzip;
	u32 brand = 0;
	u32 i, count;
	GF_ISOFile *isom = gf_isom_open(path, GF_ISOM_OPEN_READ, 0);
	if (!isom ) return NULL;

	brand = gf_isom_get_meta_type(isom, GF_TRUE, 0);
	if ((brand!=GF_4CC('m','w','g','t') )		|| !gf_isom_has_meta_xml(isom, GF_TRUE, 0) ) {
		gf_isom_close(isom);
		return NULL;
	}

	GF_SAFEALLOC(wzip, GF_WidgetPackage);
	if (!wzip) return NULL;

	wzip->wm = wm;
	wzip->relocate_uri = widget_package_relocate_uri;
	wzip->resources = gf_list_new();
	dir = gf_opts_get_key("core", "cache");
	/* create the extracted path for the package root using:
	   the cache dir + a CRC of the file path and the instance*/
	sprintf(wzip->root_extracted_path, "%s%p", path, wzip);
	i = gf_crc_32((char *)wzip->root_extracted_path, (u32) strlen(wzip->root_extracted_path));
	sprintf(wzip->archive_id, "GWM_%08X_", i);
	sprintf(wzip->root_extracted_path, "%s/%s", dir, wzip->archive_id);


	strcpy(szPath, wzip->root_extracted_path);
	strcat(szPath, "config.xml");
	if (gf_isom_extract_meta_xml(isom, GF_TRUE, 0, szPath, NULL) != GF_OK) {
		gf_list_del(wzip->resources);
		gf_free(wzip);
		gf_isom_close(isom);
		return NULL;
	}

	wzip->package_path = gf_strdup(path);

	GF_SAFEALLOC(pack_res, GF_WidgetPackageResource);
	if (!pack_res) {
		gf_list_del(wzip->resources);
		gf_free(wzip);
		gf_isom_close(isom);
		return NULL;
	}
	pack_res->extracted_path = gf_strdup(szPath);
	pack_res->inner_path = gf_strdup("config.xml");
	pack_res->extracted = GF_TRUE;
	gf_list_add(wzip->resources, pack_res);


	count = gf_isom_get_meta_item_count(isom, GF_TRUE, 0);
	for (i=0; i<count; i++)  {
		u32 ID;
		const char *url, *urn, *enc;
		Bool self_ref;
		char *sep;
		const char *item_name;

		gf_isom_get_meta_item_info(isom, GF_TRUE, 0, i+1, &ID, NULL, NULL, &self_ref, &item_name, NULL, &enc, &url, &urn);

		sep = strrchr(item_name, '/');
		if (!sep) sep = strrchr(item_name, '\\');
		if (sep) {
			sep[0] = 0;
			sprintf(szPath, "%s_%08X_%s", wzip->root_extracted_path, gf_crc_32((char*)item_name, (u32) strlen(item_name)), sep+1);
			sep[0] = '/';
		} else {
			strcpy(szPath, wzip->root_extracted_path);
			strcat(szPath, item_name);
		}
		GF_SAFEALLOC(pack_res, GF_WidgetPackageResource);
		if (!pack_res) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[WidgetMan] Failed to allocate widget resource\n"));
			continue;
		}
		pack_res->extracted_path = gf_strdup(szPath);
		pack_res->inner_path = gf_strdup(item_name);
		pack_res->extracted = GF_FALSE;
		gf_list_add(wzip->resources, pack_res);
	}
	gf_isom_close(isom);


	/* register this widget package as a relocator to enable localization of resources inside this package */
	gf_mx_p(wm->term->net_mx);
	gf_list_add(wm->term->uri_relocators, wzip);
	gf_mx_v(wm->term->net_mx);
	return wzip;
#endif /*GPAC_DISABLE_ISOM*/
}

static GF_WidgetPackage *widget_zip_new(GF_WidgetManager *wm, const char *path)
{
	unz_global_info gi;
	const char *dir;
	u32 i;
	GF_WidgetPackage *wzip;
	unzFile uf = unzOpen2(path, NULL);
	if (!uf) return NULL;

	GF_SAFEALLOC(wzip, GF_WidgetPackage);
	if (!wzip) return NULL;

	wzip->wm = wm;
	wzip->is_zip = GF_TRUE;
	wzip->relocate_uri = widget_package_relocate_uri;
	wzip->resources = gf_list_new();
	wzip->package_path = gf_strdup(path);
	dir = gf_opts_get_key("core", "cache");
	/* create the extracted path for the package root using:
	   the cache dir + a CRC of the file path and the instance*/
	sprintf(wzip->root_extracted_path, "%s%p", path, wzip);
	i = gf_crc_32((char *)wzip->root_extracted_path, (u32) strlen(wzip->root_extracted_path));
	sprintf(wzip->archive_id, "GWM_%08X_", i);
	sprintf(wzip->root_extracted_path, "%s/%s", dir, wzip->archive_id);

	unzGetGlobalInfo(uf, &gi);
	for (i=0; i<gi.number_entry; i++)  {
		char szPath[GF_MAX_PATH];
		char *sep;
		GF_WidgetPackageResource *pack_res;
		char filename_inzip[256];
		unz_file_info file_info;
		unzGetCurrentFileInfo(uf, &file_info, filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

		sep = strrchr(filename_inzip, '/');
		if (!sep) sep = strrchr(filename_inzip, '\\');
		if (sep) {
			sep[0] = 0;
			sprintf(szPath, "%s_%08X_%s", wzip->root_extracted_path, gf_crc_32(filename_inzip, (u32) strlen(filename_inzip)), sep+1);
			sep[0] = '/';
		} else {
			strcpy(szPath, wzip->root_extracted_path);
			strcat(szPath, filename_inzip);
		}


		if (!strcmp(filename_inzip, "config.xml")) {
			int err;
			char buf[8192];
			FILE *fout;
			unzOpenCurrentFile3(uf, NULL, NULL, 0, NULL/*password*/);

			fout=gf_fopen(szPath,"wb");
			if (!fout) return NULL;

			do {
				err = unzReadCurrentFile(uf,buf,8192);
				if (err<0) break;

				if (err>0)
					if (gf_fwrite(buf,err,1,fout)!=1) {
						err=UNZ_ERRNO;
						break;
					}
			} while (err>0);
			if (fout) gf_fclose(fout);
			if (err==0) {
				GF_SAFEALLOC(pack_res, GF_WidgetPackageResource);
				if (!pack_res) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[WidgetMan] Failed to allocate widget resource\n"));
					continue;
				}
				
				pack_res->extracted_path = gf_strdup(szPath);
				pack_res->inner_path = gf_strdup(filename_inzip);
				pack_res->extracted = GF_TRUE;
				gf_list_add(wzip->resources, pack_res);
			}
		} else {
			GF_SAFEALLOC(pack_res, GF_WidgetPackageResource);
			if (!pack_res) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[WidgetMan] Failed to allocate widget resource\n"));
				continue;
			}
			pack_res->extracted_path = gf_strdup(szPath);
			pack_res->inner_path = gf_strdup(filename_inzip);
			pack_res->extracted = GF_FALSE;
			gf_list_add(wzip->resources, pack_res);
		}

		if ((i+1)<gi.number_entry)
			unzGoToNextFile(uf);
	}
	unzClose(uf);

	/* register this widget package as a relocator to enable localization of resources inside this package */
	gf_mx_p(wm->term->net_mx);
	gf_list_add(wm->term->uri_relocators, wzip);
	gf_mx_v(wm->term->net_mx);
	return wzip;
}

GF_WidgetPackage *widget_package_new(GF_WidgetManager *wm, const char *path)
{
	if (gf_unzip_probe(path)) {
		return widget_zip_new(wm, path);
	}
#ifndef GPAC_DISABLE_ISOM
	/*ISOFF-based packaged widget */
	else if (gf_isom_probe_file(path)) {
		return widget_isom_new(wm, path);
	}
#endif
	return NULL;
}

static void widget_package_del(GF_WidgetManager *wm, GF_WidgetPackage *wpackage)
{
	gf_mx_p(wm->term->net_mx);
	gf_list_del_item(wm->term->uri_relocators, wpackage);
	gf_mx_v(wm->term->net_mx);

	while (gf_list_count(wpackage->resources)) {
		GF_WidgetPackageResource *wu = (GF_WidgetPackageResource*)gf_list_get(wpackage->resources, 0);
		gf_list_rem(wpackage->resources, 0);
		gf_delete_file(wu->extracted_path);
		gf_free(wu->extracted_path);
		gf_free(wu->inner_path);
		gf_free(wu);
	}
	gf_list_del(wpackage->resources);
	if (wpackage->sess) gf_dm_sess_del(wpackage->sess);
	gf_free(wpackage->package_path);
	gf_free(wpackage);
}



static void wm_delete_message_param(GF_WidgetPin *mp)
{
	if (!mp) return;
	if (mp->node) gf_free(mp->node);
	if (mp->attribute) gf_free(mp->attribute);
	if (mp->default_value) gf_free(mp->default_value);
	if (mp->name) gf_free(mp->name);
	gf_free(mp);
}

static void wm_delete_widget_content(GF_WidgetContent *content)
{
	if (!content) return;

	while (gf_list_count(content->interfaces)) {
		GF_WidgetInterface *ifce = (GF_WidgetInterface*)gf_list_last(content->interfaces);
		gf_list_rem_last(content->interfaces);

		while (gf_list_count(ifce->messages)) {
			GF_WidgetMessage *msg = (GF_WidgetMessage*)gf_list_last(ifce->messages);
			gf_list_rem_last(ifce->messages);

			while (gf_list_count(msg->params)) {
				GF_WidgetPin *par = (GF_WidgetPin*)gf_list_last(msg->params);
				gf_list_rem_last(msg->params);
				wm_delete_message_param(par);
			}
			gf_list_del(msg->params);

			wm_delete_message_param(msg->input_action);
			wm_delete_message_param(msg->output_trigger);
			gf_free(msg->name);
			gf_free(msg);
		}
		gf_list_del(ifce->messages);
		wm_delete_message_param(ifce->bind_action);
		wm_delete_message_param(ifce->unbind_action);
		if (ifce->obj) gf_js_remove_root(ifce->content->widget->wm->ctx, &ifce->obj, GF_JSGC_OBJECT);

		if (ifce->connectTo) gf_free(ifce->connectTo);
		gf_free(ifce->type);
		gf_free(ifce);
	}
	gf_list_del(content->interfaces);

	while (gf_list_count(content->components)) {
		GF_WidgetComponent *comp = (GF_WidgetComponent*)gf_list_last(content->components);
		gf_list_rem_last(content->components);

		wm_delete_message_param(comp->activateTrigger);
		wm_delete_message_param(comp->deactivateTrigger);

		while (gf_list_count(comp->required_interfaces)) {
			char *type = (char*)gf_list_last(comp->required_interfaces);
			gf_list_rem_last(comp->required_interfaces);
			if (type) gf_free(type);
		}
		gf_list_del(comp->required_interfaces);
		if (comp->id) gf_free(comp->id);
		if (comp->src) gf_free(comp->src);
		gf_free(comp);
	}
	gf_list_del(content->components);

	while (gf_list_count(content->preferences)) {
		GF_WidgetPreference *pref = (GF_WidgetPreference*)gf_list_last(content->preferences);
		gf_list_rem_last(content->preferences);

		wm_delete_message_param(pref->connectTo);
		if (pref->value) gf_free(pref->value);
		gf_free(pref->name);
		gf_free(pref);
	}
	gf_list_del(content->preferences);

	wm_delete_message_param(content->saveTrigger);
	wm_delete_message_param(content->restoreTrigger);
	wm_delete_message_param(content->savedAction);
	wm_delete_message_param(content->restoredAction);

	gf_free(content->src);
	gf_free(content->relocated_src);
	gf_free(content->mimetype);
	gf_free(content->encoding);
	gf_free(content);
}

static void wm_delete_widget(GF_WidgetManager *wm, GF_Widget *wid)
{
	gf_list_del_item(wm->widgets, wid);

	if (wid->url) gf_free(wid->url);
	if (wid->manifest_path) gf_free(wid->manifest_path);
	wm_delete_widget_content(wid->main);
	if (wid->local_path) gf_free(wid->local_path);
	if (wid->name) gf_free(wid->name);
	if (wid->shortname) gf_free(wid->shortname);
	if (wid->identifier) gf_free(wid->identifier);
	if (wid->authorName) gf_free(wid->authorName);
	if (wid->authorEmail) gf_free(wid->authorEmail);
	if (wid->authorHref) gf_free(wid->authorHref);
	if (wid->description) gf_free(wid->description);
	if (wid->license) gf_free(wid->license);
	if (wid->licenseHref) gf_free(wid->licenseHref);
	if (wid->viewmodes) gf_free(wid->viewmodes);
	if (wid->version) gf_free(wid->version);
	if (wid->uuid) gf_free(wid->uuid);


	while (gf_list_count(wid->icons)) {
		GF_WidgetContent *icon = (GF_WidgetContent*)gf_list_get(wid->icons, 0);
		gf_list_rem(wid->icons, 0);
		wm_delete_widget_content(icon);
	}
	gf_list_del(wid->icons);

	while (gf_list_count(wid->features)) {
		GF_WidgetFeature *f = (GF_WidgetFeature*)gf_list_get(wid->features, 0);
		gf_list_rem(wid->features, 0);
		if (f->name) gf_free(f->name);
		while (gf_list_count(f->params)) {
			GF_WidgetFeatureParam *p = (GF_WidgetFeatureParam*)gf_list_get(f->params, 0);
			gf_list_rem(f->params, 0);
			if (p->name) gf_free(p->name);
			if (p->value) gf_free(p->value);
			gf_free(p);
		}
		gf_free(f);
	}
	gf_list_del(wid->features);

	if (wid->wpack) widget_package_del(wm, wid->wpack);
	gf_free(wid);
}

static void wm_delete_interface_instance(GF_WidgetManager *wm, GF_WidgetInterfaceInstance *bifce)
{
	if (bifce->hostname) gf_free(bifce->hostname);
	if (bifce->obj) {
		SMJS_SET_PRIVATE(wm->ctx, bifce->obj, NULL);
		gf_js_remove_root(wm->ctx, &bifce->obj, GF_JSGC_OBJECT);
	}
	gf_free(bifce);
}

static void wm_delete_widget_instance(GF_WidgetManager *wm, GF_WidgetInstance *widg)
{

	while (gf_list_count(widg->components)) {
		GF_WidgetComponentInstance *comp = (GF_WidgetComponentInstance*)gf_list_get(widg->components, 0);
		gf_list_rem(widg->components, 0);
		if (comp->wid) wm_delete_widget_instance(wm, comp->wid);
		gf_free(comp);
	}
	gf_list_del(widg->components);

	while (gf_list_count(widg->bound_ifces)) {
		GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance*)gf_list_get(widg->bound_ifces, 0);
		gf_list_rem(widg->bound_ifces, 0);
		wm_delete_interface_instance(wm, bifce);
	}
	gf_list_del(widg->bound_ifces);

	gf_list_del(widg->output_triggers);

	if (widg->obj) {
		SMJS_SET_PRIVATE(wm->ctx, widg->obj, NULL);
		gf_js_remove_root(wm->ctx, &widg->obj, GF_JSGC_OBJECT);
	}
	gf_list_del_item(wm->widget_instances, widg);
	widg->widget->nb_instances--;
	if (!widg->widget->nb_instances) wm_delete_widget(wm, widg->widget);

	if (!widg->permanent) {
		gf_cfg_del_section(wm->term->user->config, (const char *)widg->secname);
		gf_cfg_set_key(wm->term->user->config, "Widgets", (const char *)widg->secname, NULL);
	}
	if (widg->mpegu_context) gf_xml_dom_del(widg->mpegu_context);

	gf_free(widg);
}



static JSBool wm_widget_call_script(GF_WidgetInstance *wid, GF_WidgetPin *pin, uintN argc, jsval *argv, jsval *rval)
{
	jsval fval;
	if (!wid->scene_context || !wid->scene_global) return JS_FALSE;

	/*if on_load property is assigned to this widget, add an event listener on the root.*/
	JS_LookupProperty(wid->scene_context, wid->scene_global, pin->node, &fval);
	if (JSVAL_IS_OBJECT(fval)) {
		JS_CallFunctionValue(wid->scene_context, wid->scene_global, fval, argc, argv, rval);
	}
	return JS_TRUE;
}

static JSBool wm_widget_set_scene_input_value(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, u32 type, GF_WidgetInstance *wid, GF_WidgetPin *param, const char *value)
{
	char *str_val;
	GF_Node *n;
	GF_FieldInfo info;
	GF_WidgetMessage *msg;
	GF_WidgetInterface *ifce;

	if (!wid && obj) wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;
	if (!wid->scene) return JS_TRUE;

	if (!param) {
		if (!JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

		switch (type) {
		/*set_input*/
		case 0:
			if (argc!=2) return JS_FALSE;
			param = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
			break;
		/*call_input_action*/
		case 1:
			msg = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
			param = msg ? msg->input_action : NULL;
			break;
		/*bind_interface*/
		case 2:
			ifce = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
			param = ifce ? ifce->bind_action : NULL;
			break;
		/*unbind_interface*/
		case 3:
			ifce = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
			param = ifce ? ifce->unbind_action : NULL;
			break;
		}
	}

	if (!param  || !param->node) return JS_TRUE;
	/*this is a script call*/
	if (!param->attribute) {
		return wm_widget_call_script(wid, param, 0, NULL, rval);
	}

	n = gf_sg_find_node_by_name(wid->scene, param->node);
	if (!n) return JS_TRUE;

	if (param->in_action) return JS_TRUE;

	param->in_action = GF_TRUE;

#ifndef GPAC_DISABLE_SVG
	if (n->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) {
		u32 evt_type;
		if (!type) {
			char *_str_val = NULL;
			if (value) {
				str_val = (char *)value;
			} else {
				if (!JSVAL_IS_STRING(argv[1])) goto exit;
				str_val = _str_val = SMJS_CHARS(c, argv[1]);
			}

			/* first check if the set_input refers to an attribute name or to an event name */
			evt_type = gf_dom_event_type_by_name(param->attribute);

			/*events are not allowed for <input> and <output>*/
			if (evt_type == GF_EVENT_UNKNOWN) {
				/* modify an attribute */
				if (!strcmp(param->attribute, "textContent")) {
					gf_dom_set_textContent(n, (char *)str_val);
					gf_node_changed(n, NULL);
				}
				else {
					if (gf_node_get_attribute_by_name(n, param->attribute, 0, GF_TRUE, GF_FALSE, &info)==GF_OK) {
						gf_svg_parse_attribute(n, &info, (char *)str_val, 0);
						if (info.fieldType==XMLRI_datatype) gf_node_dirty_set(n, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);
						gf_node_changed(n, &info);
					}
				}
			}
			SMJS_FREE(c, _str_val);

		} else {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			/* first check if the set_input refers to an attribute name or to an event name */
			evt_type = gf_dom_event_type_by_name(param->attribute);
			/* launch an event */
			if (evt_type != GF_EVENT_UNKNOWN) {
				evt.type = evt_type;
				gf_dom_event_fire(n, &evt);
			} else {
				/*should we fire a DOMAttrModified event ? to clarify in the spec*/

				if (gf_node_get_attribute_by_name(n, param->attribute, 0, GF_TRUE, GF_FALSE, &info)==GF_OK) {
					evt.bubbles = 1;
					evt.type = GF_EVENT_ATTR_MODIFIED;
					evt.attr = &info;
					gf_dom_event_fire(n, &evt);
				}
			}
		}
	} else
#endif


#ifndef GPAC_DISABLE_VRML
	{
		if (gf_node_get_field_by_name(n, param->attribute, &info) != GF_OK) return JS_TRUE;

		if (!type) {
			char *_str_val = NULL;
			jsdouble val;

			switch (info.fieldType) {
			case GF_SG_VRML_SFSTRING:
				if (value) {
					str_val = (char *)value;
				} else {
					if (!JSVAL_IS_STRING(argv[1]))goto exit;
					str_val = SMJS_CHARS(c, argv[1]);
				}
				if ( ((SFString*)info.far_ptr)->buffer) gf_free(((SFString*)info.far_ptr)->buffer);
				((SFString*)info.far_ptr)->buffer = str_val ? gf_strdup(str_val) : NULL;
				break;
			case GF_SG_VRML_SFBOOL:
				if (value) *((SFBool*)info.far_ptr) = (!strcmp(value, "true")) ? 1 : 0;
				else if (JSVAL_IS_BOOLEAN(argv[1])) *((SFBool*)info.far_ptr) = JSVAL_TO_BOOLEAN(argv[1]);
				else if (JSVAL_IS_INT(argv[1])) *((SFBool*)info.far_ptr) = JSVAL_TO_INT(argv[1]);
				else if (JSVAL_IS_STRING(argv[1])) {
					str_val = SMJS_CHARS(c, argv[1]);
					*((SFBool*)info.far_ptr) = (str_val && !strcmp(str_val, "true")) ? 1 : 0;
					SMJS_FREE(c, str_val);
				} else
					goto exit;
				break;
			case GF_SG_VRML_SFINT32:
				if (value) *((SFInt32*)info.far_ptr) = (s32) atof(value);
				else if (JSVAL_IS_INT(argv[1])) *((SFInt32*)info.far_ptr) = JSVAL_TO_INT(argv[1]);
				else if (JSVAL_IS_NUMBER(argv[1])) {
					JS_ValueToNumber(c, argv[1], &val);
					*((SFInt32*)info.far_ptr) = (s32) val;
				} else if (JSVAL_IS_STRING(argv[1])) {
					Double a_val;
					str_val = SMJS_CHARS(c, argv[1]);
					a_val = str_val ? atof(str_val) : 0;
					*((SFInt32*)info.far_ptr) = (s32) a_val;
					SMJS_FREE(c, str_val);
				} else
					goto exit;
				break;
			case GF_SG_VRML_SFFLOAT:
				if (value) *((SFFloat*)info.far_ptr) = FLT2FIX(atof(value));
				else if (JSVAL_IS_INT(argv[1])) *((SFFloat *)info.far_ptr) = INT2FIX( JSVAL_TO_INT(argv[1]) );
				else if (JSVAL_IS_NUMBER(argv[1])) {
					JS_ValueToNumber(c, argv[1], &val);
					*((SFFloat *)info.far_ptr) = FLT2FIX( val );
				} else if (JSVAL_IS_STRING(argv[1])) {
					Double a_val;
					str_val = SMJS_CHARS(c, argv[1]);
					a_val = str_val ? atof(str_val) : 0;
					*((SFFloat*)info.far_ptr) = FLT2FIX(a_val);
					SMJS_FREE(c, str_val);
				} else
					goto exit;
				break;
			case GF_SG_VRML_SFTIME:
				if (value) *((SFTime*)info.far_ptr) = atof(value);
				else if (JSVAL_IS_INT(argv[1])) *((SFTime *)info.far_ptr) = JSVAL_TO_INT(argv[1]);
				else if (JSVAL_IS_NUMBER(argv[1])) {
					JS_ValueToNumber(c, argv[1], &val);
					*((SFTime *)info.far_ptr) = val;
				} else if (JSVAL_IS_STRING(argv[1])) {
					Double a_val;
					str_val = SMJS_CHARS(c, argv[1]);
					a_val = str_val ? atof(str_val) : 0;
					*((SFTime *)info.far_ptr) = a_val;
					SMJS_FREE(c, str_val);
				} else
					goto exit;
				break;
			case GF_SG_VRML_MFSTRING:
				if (value) {
					str_val = (char *)value;
				} else {
					if (!JSVAL_IS_STRING(argv[1])) goto exit;
					str_val = _str_val = SMJS_CHARS(c, argv[1]);
				}
				if ( ((GenMFField *)info.far_ptr)->count != 1) {
					gf_sg_vrml_mf_reset(info.far_ptr, GF_SG_VRML_MFSTRING);
					gf_sg_vrml_mf_alloc(info.far_ptr, GF_SG_VRML_MFSTRING, 1);
				}
				if ( ((MFString*)info.far_ptr)->vals[0]) gf_free( ((MFString*)info.far_ptr)->vals[0] );
				((MFString*)info.far_ptr)->vals[0] = str_val ? gf_strdup(str_val) : NULL;

				SMJS_FREE(c, _str_val);
				break;
			}
		}

		//if this is a script eventIn call directly script
		if ((n->sgprivate->tag==TAG_MPEG4_Script)
#ifndef GPAC_DISABLE_X3D
		        || (n->sgprivate->tag==TAG_X3D_Script)
#endif
		   )
			gf_sg_script_event_in(n, &info);

		gf_node_changed(n, &info);

		/*if this is an exposedField, route it*/
		if (info.eventType==GF_SG_EVENT_EXPOSED_FIELD) {
			gf_node_event_out(n, info.fieldIndex);
		}
	}
#endif /*GPAC_DISABLE_VRML*/

exit:
	param->in_action = GF_FALSE;
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(wm_widget_set_input)
{
	SMJS_OBJ
	SMJS_ARGS
	SMJS_DECL_RVAL
	return wm_widget_set_scene_input_value(c, obj, argc, argv, rval, 0, NULL, NULL, NULL);
}
static JSBool SMJS_FUNCTION(wm_widget_call_input_action)
{
	SMJS_OBJ
	SMJS_ARGS
	SMJS_DECL_RVAL
	return wm_widget_set_scene_input_value(c, obj, 1, argv, rval, 1, NULL, NULL, NULL);
}


static JSBool SMJS_FUNCTION(wm_widget_call_input_script)
{
	GF_WidgetMessage *msg;
	GF_WidgetPin *param;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || (argc!=2) ) return JS_FALSE;
	if (!wid->scene) return JS_TRUE;

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	msg = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	param = msg ? msg->input_action : NULL;
	if (!param || !param->node || param->attribute) return JS_FALSE;

	if (JSVAL_IS_OBJECT(argv[1])) {
		jsval *args;
		JSObject *list = JSVAL_TO_OBJECT(argv[1]);
		u32 i, count;
		JS_GetArrayLength(c, list, (jsuint*) &count);
		args = (jsval*)gf_malloc(sizeof(jsval)*count);
		for (i=0; i<count; i++) {
			JS_GetElement(c, list, (jsint) i, &args[i] );
		}

		wm_widget_call_script(wid, param, count, args, SMJS_GET_RVAL);

		gf_free(args);
	}
	return JS_TRUE;
}


static void wm_handler_destroy(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SVG_handlerElement *handler = (SVG_handlerElement *)node;
		if (handler->js_fun_val) {
			gf_js_remove_root(handler->js_context, &handler->js_fun_val, GF_JSGC_VAL);
			handler->js_fun_val=0;
		}
	}
}

static SVG_handlerElement *wm_create_scene_listener(GF_WidgetInstance *wid, GF_WidgetPin *param)
{
	/*FIXME - we need to split SVG and base DOM !!*/
#ifdef GPAC_DISABLE_SVG
	return NULL;
#else
	u32 evt_type, att_name;
	GF_Node *listener;
	GF_FieldInfo info;
	GF_Node *n = NULL;
	SVG_handlerElement *handler;

	evt_type = GF_EVENT_ATTR_MODIFIED;
	n = gf_sg_find_node_by_name(wid->scene, param->node);
	if (!n)
		return NULL;

	att_name = 0;

#ifndef GPAC_DISABLE_SVG
	if (n->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) {
		/* first check if the set_input refers to an attribute name or to an event name */
		evt_type = gf_dom_event_type_by_name(param->attribute);
		if (evt_type == GF_EVENT_UNKNOWN) {
			evt_type = GF_EVENT_ATTR_MODIFIED;

			/* modify textContent */
			if (!strcmp(param->attribute, "textContent")) {
				att_name = (u32) -1;
			}
			/* modify an attribute */
			else if (gf_node_get_attribute_by_name(n, param->attribute, 0, GF_TRUE, GF_FALSE, &info)==GF_OK) {
				att_name = info.fieldIndex;
			}
			else {
				return NULL;
			}
		}
	} else
#endif
	{
		if (gf_node_get_field_by_name(n, param->attribute, &info) != GF_OK)
			return NULL;
		att_name = info.fieldIndex;
	}

	listener = gf_node_new(wid->scene, TAG_SVG_listener);

	handler = (SVG_handlerElement *) gf_node_new(wid->scene, TAG_SVG_handler);
	/*we register the handler with the listener node to avoid modifying the DOM*/
	gf_node_register((GF_Node *)handler, listener);
	gf_node_list_add_child(& ((GF_ParentNode *)listener)->children, (GF_Node*)handler);
	handler->sgprivate->UserCallback = wm_handler_destroy;

	/*create attributes if needed*/
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
	((XMLEV_Event*)info.far_ptr)->type = evt_type;
	((XMLEV_Event*)info.far_ptr)->parameter = att_name;
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_handler, GF_TRUE, GF_FALSE, &info);
	((XMLRI*)info.far_ptr)->target = (GF_Node*)handler;
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_target, GF_TRUE, GF_FALSE, &info);
	((XMLRI*)info.far_ptr)->target = n;

	gf_node_get_attribute_by_tag((GF_Node*)handler, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
	((XMLEV_Event*)info.far_ptr)->type = evt_type;
	((XMLEV_Event*)info.far_ptr)->parameter = att_name;

	gf_node_dom_listener_add((GF_Node *) n, listener);

	return handler;
#endif
}

static void wm_component_activation_event(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer, Bool unload)
{
	JSObject *obj;
	JSContext *c;
	GF_WidgetInstance *wid;
	GF_WidgetComponent *comp;
	SVG_handlerElement *handler = (SVG_handlerElement *)hdl;

	c = (JSContext*)handler->js_context;
	obj = (JSObject*)handler->evt_listen_obj;
	if (!c || !obj) return;
	wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return;

	comp = (GF_WidgetComponent *)handler->js_fun;
	if (unload) {
		wm_deactivate_component(c, wid, comp, NULL);
	} else {
		wm_activate_component(c, wid, comp, GF_FALSE);
	}
}
static void wm_component_activate_event(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	wm_component_activation_event(hdl, evt, observer, GF_FALSE);
}
static void wm_component_deactivate_event(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	wm_component_activation_event(hdl, evt, observer, GF_TRUE);
}

static void wm_widget_set_pref_event(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	char *att;
	SVG_handlerElement *handler = (SVG_handlerElement *)hdl;
	GF_WidgetInstance *wid = (GF_WidgetInstance *) handler->evt_listen_obj;
	GF_WidgetPreference *pref = (GF_WidgetPreference *) handler->js_fun;

	if (evt->type != GF_EVENT_ATTR_MODIFIED) return;

	if (evt->detail == (u32) -1) {
#ifndef GPAC_DISABLE_SVG
		att = gf_dom_flatten_textContent(evt->target);
#endif
	} else {
		att = gf_node_dump_attribute(evt->target, evt->attr);
	}
	if (!att) return;
	gf_cfg_set_key(wid->widget->wm->term->user->config, (const char *)wid->secname, pref->name, att);
	gf_free(att);
}

static void on_widget_activated(JSContext *c, JSObject *obj)
{
	jsval funval, rval;
	u32 i, count;
	GF_XMLNode *context = NULL;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || wid->activated) return;

	/*widget is now activated*/
	wid->activated = GF_TRUE;

	/*for all components, setup bindings on activateTrigger & deactivateTrigger*/
	count = gf_list_count(wid->widget->main->components);
	for (i=0; i<count; i++) {
		GF_WidgetComponent *comp = (GF_WidgetComponent*)gf_list_get(wid->widget->main->components, i);
		/*setup listener*/
		if (comp->activateTrigger && comp->activateTrigger->attribute) {
			SVG_handlerElement *a_hdl = wm_create_scene_listener(wid, comp->activateTrigger);
			if (!a_hdl) return;
			a_hdl->evt_listen_obj = obj;
			a_hdl->js_context = c;
			a_hdl->js_fun = comp;
			a_hdl->handle_event = wm_component_activate_event;
		}
		/*setup listener*/
		if (comp->deactivateTrigger && comp->deactivateTrigger->attribute) {
			SVG_handlerElement *a_hdl = wm_create_scene_listener(wid, comp->deactivateTrigger);
			if (!a_hdl) continue;
			a_hdl->evt_listen_obj = obj;
			a_hdl->js_context = c;
			a_hdl->js_fun = comp;
			a_hdl->handle_event = wm_component_deactivate_event;
		}
	}

	if (wid->mpegu_context)
		context = gf_xml_dom_get_root(wid->mpegu_context);

	/*load preferences*/
	count = gf_list_count(wid->widget->main->preferences);
	for (i=0; i<count; i++) {
		const char *value;
		GF_WidgetPreference *pref = (GF_WidgetPreference*)gf_list_get(wid->widget->main->preferences, i);


		/*get stored value for this preference*/
		value = gf_opts_get_key((const char *)wid->secname, pref->name);
		/*if none found, use preference*/
		if (!value) value = pref->value;

		/*and overload with migrated context*/
		if (context) {
			GF_XMLNode *pref_node;
			u32 j=0;
			while ((pref_node = (GF_XMLNode*)gf_list_enum(context->content, &j))) {
				const char *att;
				if (pref_node->type != GF_XML_NODE_TYPE) continue;
				if (strcmp(pref_node->name, "preference")) continue;
				att = wm_xml_get_attr(pref_node, "name");
				if (!att) continue;
				if (strcmp(att, pref->name)) continue;

				att = wm_xml_get_attr(pref_node, "value");
				if (att) value = att;
				break;
			}
		}

		if (pref->connectTo) {
			SVG_handlerElement *hdl;

			if (value)
				wm_widget_set_scene_input_value(NULL, NULL, 0, 0, 0, 0, wid, pref->connectTo, value);

			/*preference is read only, do not setup listener*/
			if (pref->flags & GF_WM_PREF_READONLY) continue;
			/*preference is migratable only, do not setup listener*/
			if (!(pref->flags & GF_WM_PREF_SAVE)) continue;

			/*create the listener*/
			hdl = wm_create_scene_listener(wid, pref->connectTo);
			if (!hdl) continue;

			hdl->evt_listen_obj = wid;
			hdl->js_fun = pref;
			hdl->handle_event = wm_widget_set_pref_event;

		}
	}
	if (count && wid->widget->main->restoredAction) {
		wm_widget_set_scene_input_value(wid->widget->wm->ctx, wid->obj, 0, NULL, &rval, 0, wid, wid->widget->main->restoredAction, NULL);
	}

	if (wid->mpegu_context) {
		gf_xml_dom_del(wid->mpegu_context);
		wid->mpegu_context = NULL;
	}

	gf_sg_lock_javascript(wid->widget->wm->ctx, GF_TRUE);
	/*refresh all interface bindings*/
	JS_LookupProperty(wid->widget->wm->ctx, wid->widget->wm->obj, "check_bindings", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		JS_CallFunctionValue(wid->widget->wm->ctx, wid->widget->wm->obj, funval, 0, 0, &rval);
	}

	/*if on_load property is assigned to this widget, call it.*/
	JS_LookupProperty(c, obj, "on_load", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		JS_CallFunctionValue(wid->widget->wm->ctx, wid->obj, funval, 0, 0, &rval);
	}
	gf_sg_lock_javascript(wid->widget->wm->ctx, GF_FALSE);
}


static void wm_widget_load_event(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	JSObject *obj;
	JSContext *c;
	SVG_handlerElement *handler = (SVG_handlerElement *)hdl;

	c = (JSContext*)handler->js_context;
	obj = (JSObject*)handler->evt_listen_obj;
	if (!c || !obj) return;

	on_widget_activated(c, obj);
}


static JSBool SMJS_FUNCTION(wm_widget_activate)
{
#ifndef GPAC_DISABLE_SVG
	SVG_handlerElement *handler;
#endif
	GF_MediaObject *mo;
	Bool direct_trigger = GF_FALSE;
	MFURL url;
	SFURL _url;
	GF_Node *inl;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !argc) return JS_FALSE;

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	inl = gf_sg_js_get_node(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!inl) return JS_FALSE;

	_url.OD_ID = 0;
	_url.url = gf_url_concatenate(wid->widget->url, wid->widget->main->relocated_src);
	url.count = 1;
	url.vals = &_url;
	mo = gf_mo_register(inl, &url, GF_FALSE, GF_TRUE);
	if (mo) {
		wid->scene = gf_mo_get_scenegraph(mo);
		if (wid->scene && gf_sg_get_root_node(wid->scene)) direct_trigger = GF_TRUE;
	}
	if (_url.url) gf_free(_url.url);

	wid->anchor = inl;

	if (direct_trigger) {
		on_widget_activated(c, obj);
	} else {
#ifndef GPAC_DISABLE_SVG
		handler = gf_dom_listener_build(inl, GF_EVENT_SCENE_ATTACHED, 0);
		handler->handle_event = wm_widget_load_event;
		handler->js_context = c;
		handler->evt_listen_obj = obj;
#endif
	}

	return JS_TRUE;
}


static void wm_handle_dom_event(GF_Node *hdl, GF_DOM_Event *event, GF_Node *observer)
{
	GF_FieldInfo info;
	GF_Node *n;
	jsval argv[1], rval, jsfun;
	SVG_handlerElement *handler = (SVG_handlerElement *) hdl;
	GF_WidgetPin *param = (GF_WidgetPin*)handler->js_fun;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)handler->evt_listen_obj;

	if (!wid->scene)
		return;

	n = gf_sg_find_node_by_name(wid->scene, param->node);
	if (!n) return;

	/*this is a regular event output*/
	if (event->type != GF_EVENT_ATTR_MODIFIED) {
		jsfun = (jsval) handler->js_fun_val;
		if (JSVAL_IS_OBJECT(jsfun))
			JS_CallFunctionValue(handler->js_context, wid->obj, (jsval) handler->js_fun_val, 0, 0, &rval);
		return;
	}

	/*otherwise this is a node.attr output through DOMAttributeModified*/
	argv[0] = JSVAL_NULL;

#ifndef GPAC_DISABLE_SVG
	if (n->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) {
		if (!strcmp(param->attribute, "textContent")) {
			char *txt = gf_dom_flatten_textContent(n);
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(handler->js_context, txt ? txt : "") );
			if (txt) gf_free(txt);
		} else if (gf_node_get_attribute_by_name(n, param->attribute, 0, GF_TRUE, GF_FALSE, &info)==GF_OK) {
			char *attValue;
			if (event->attr->fieldIndex != info.fieldIndex) return;

			attValue = gf_node_dump_attribute(n, &info);
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(handler->js_context, attValue) );
			if (attValue) gf_free(attValue);
		} else {
			return;
		}
	} else
#endif
	{
		if (gf_node_get_field_by_name(n, param->attribute, &info) != GF_OK) return;
		if (event->attr->fieldIndex != info.fieldIndex) return;

		switch (info.fieldType) {
		case GF_SG_VRML_SFSTRING:
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(handler->js_context, ((SFString*)info.far_ptr)->buffer ) );
			break;
		case GF_SG_VRML_SFINT32:
			argv[0] = INT_TO_JSVAL( *((SFInt32*)info.far_ptr) );
			break;
		case GF_SG_VRML_SFBOOL:
			argv[0] = BOOLEAN_TO_JSVAL( *((SFBool*)info.far_ptr) );
			break;
		case GF_SG_VRML_SFFLOAT:
			argv[0] = DOUBLE_TO_JSVAL( JS_NewDouble(handler->js_context, FIX2FLT( *((SFFloat*)info.far_ptr) ) ) );
			break;
		case GF_SG_VRML_SFTIME:
			argv[0] = DOUBLE_TO_JSVAL( JS_NewDouble(handler->js_context, *((SFTime *)info.far_ptr) ) );
			break;
		}
	}

	jsfun = (jsval) handler->js_fun_val;
	if (JSVAL_IS_OBJECT(jsfun))
		JS_CallFunctionValue(handler->js_context, wid->obj, (jsval) handler->js_fun_val, 1, argv, &rval);
}

static JSBool SMJS_FUNCTION(wm_widget_get_param_value)
{
	GF_Node *n;
	GF_FieldInfo info;
	GF_WidgetPin *param;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !wid->scene || !argc || !JSVAL_IS_OBJECT(argv[0]) ) return JS_FALSE;

	param = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!param) return JS_FALSE;

	if (!param->node) {
		if (param->default_value) {
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, param->default_value) ) );
			return JS_TRUE;
		}
		return JS_FALSE;
	}

	n = gf_sg_find_node_by_name(wid->scene, param->node);
	if (!n) return JS_FALSE;

#ifndef GPAC_DISABLE_SVG
	if (n->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) {
		if (!strcmp(param->attribute, "textContent")) {
			char *txt = gf_dom_flatten_textContent(n);
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, txt ? txt : "") ));
			if (txt) gf_free(txt);
		} else if (gf_node_get_attribute_by_name(n, param->attribute, 0, GF_TRUE, GF_FALSE, &info)==GF_OK) {
			char *attValue = gf_node_dump_attribute(n, &info);
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, attValue) ));
			if (attValue) gf_free(attValue);
		} else {
			return JS_FALSE;
		}
	} else
#endif
	{
		if (gf_node_get_field_by_name(n, param->attribute, &info) != GF_OK) return JS_FALSE;

		switch (info.fieldType) {
		case GF_SG_VRML_SFSTRING:
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, ((SFString*)info.far_ptr)->buffer ) ) );
			break;
		case GF_SG_VRML_SFINT32:
			SMJS_SET_RVAL( INT_TO_JSVAL( *((SFInt32*)info.far_ptr) ));
			break;
		case GF_SG_VRML_SFBOOL:
			SMJS_SET_RVAL(BOOLEAN_TO_JSVAL( *((SFBool*)info.far_ptr) ));
			break;
		case GF_SG_VRML_SFFLOAT:
			SMJS_SET_RVAL( DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT( *((SFFloat*)info.far_ptr) ) ) ));
			break;
		case GF_SG_VRML_SFTIME:
			SMJS_SET_RVAL( DOUBLE_TO_JSVAL( JS_NewDouble(c, *((SFTime *)info.far_ptr) ) ));
			break;
		}
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_get_message_param)
{
	GF_WidgetPin *par = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetMessage *msg = (GF_WidgetMessage *)SMJS_GET_PRIVATE(c, obj);
	if (!msg || !argc  ) return JS_FALSE;

	if (JSVAL_IS_INT(argv[0])) {
		u32 idx = JSVAL_TO_INT(argv[0]);
		par = gf_list_get(msg->params, idx);
	} else if (JSVAL_IS_STRING(argv[0])) {
		u32 i, count = gf_list_count(msg->params);
		char *name =  SMJS_CHARS(c, argv[0]);
		for (i=0; i<count; i++) {
			par = gf_list_get(msg->params, i);
			if (!strcmp(par->name, name)) break;
			par = NULL;
		}
		SMJS_FREE(c, name);
	}

	if (par) {
		JSObject *obj = JS_NewObject(c, &msg->ifce->content->widget->wm->widgetAnyClass._class, 0, 0);
		SMJS_SET_PRIVATE(c, obj, par);
		JS_DefineProperty(c, obj, "name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, par->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "is_input", BOOLEAN_TO_JSVAL( (par->type == GF_WM_PARAM_OUTPUT) ? JS_FALSE : JS_TRUE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		switch (par->script_type) {
		case GF_WM_PARAM_SCRIPT_BOOL:
			JS_DefineProperty(c, obj, "script_type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, "boolean") ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			break;
		case GF_WM_PARAM_SCRIPT_NUMBER:
			JS_DefineProperty(c, obj, "script_type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, "number") ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			break;
		case GF_WM_PARAM_SCRIPT_STRING:
		default:
			JS_DefineProperty(c, obj, "script_type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, "string") ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			break;
		}
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_get_message)
{
	GF_WidgetMessage *msg;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInterface *ifce = (GF_WidgetInterface*)SMJS_GET_PRIVATE(c, obj);
	if (!ifce || !argc) return JS_FALSE;
	msg = NULL;

	if (JSVAL_IS_INT(argv[0])) {
		u32 idx;
		idx = JSVAL_TO_INT(argv[0]);
		msg = (GF_WidgetMessage*)gf_list_get(ifce->messages, idx);
	} else if (JSVAL_IS_STRING(argv[0])) {
		u32 i, count = gf_list_count(ifce->messages);
		char *name = SMJS_CHARS(c, argv[0]);
		for (i=0; i<count; i++) {
			msg = (GF_WidgetMessage*)gf_list_get(ifce->messages, i);
			if (!strcmp(msg->name, name)) break;
			msg = NULL;
		}
		SMJS_FREE(c, name);
	}
	if (msg) {
		JSObject *obj = JS_NewObject(c, &ifce->content->widget->wm->widgetAnyClass._class, 0, 0);
		SMJS_SET_PRIVATE(c, obj, msg);
		JS_DefineProperty(c, obj, "num_params", INT_TO_JSVAL( gf_list_count(msg->params) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, msg->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "is_input", BOOLEAN_TO_JSVAL( msg->is_output ? JS_FALSE : JS_TRUE ) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "has_output_trigger", BOOLEAN_TO_JSVAL( msg->output_trigger ? JS_TRUE : JS_FALSE) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "has_input_action", BOOLEAN_TO_JSVAL( (msg->input_action && msg->input_action->attribute) ? JS_TRUE : JS_FALSE) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, obj, "has_script_input", BOOLEAN_TO_JSVAL( (msg->input_action && !msg->input_action->attribute) ? JS_TRUE : JS_FALSE) , 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineFunction(c, obj, "get_param", wm_widget_get_message_param, 1, 0);

		SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_get_component)
{
	char *comp_id;
	u32 i, count;
	GF_WidgetComponentInstance *comp_inst;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !argc || !JSVAL_IS_STRING(argv[0]) ) return JS_FALSE;

	comp_id = SMJS_CHARS(c, argv[0]);
	count = gf_list_count(wid->components);
	for (i=0; i<count; i++) {
		comp_inst = (GF_WidgetComponentInstance*)gf_list_get(wid->components, i);
		if (comp_inst->comp->id && !strcmp(comp_inst->comp->id, comp_id)) {
			SMJS_SET_RVAL( OBJECT_TO_JSVAL(comp_inst->wid->obj) );
			SMJS_FREE(c, comp_id);
			return JS_TRUE;
		}
	}
	/*if requested load the component*/
	if ((argc==2) && JSVAL_IS_BOOLEAN(argv[1]) && (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ) {
		count = gf_list_count(wid->widget->main->components);
		for (i=0; i<count; i++) {
			GF_WidgetComponent *comp = (GF_WidgetComponent*)gf_list_get(wid->widget->main->components, i);
			if (!comp->id  || strcmp(comp->id, comp_id)) continue;

			comp_inst = wm_activate_component(c, wid, comp, GF_TRUE);
			if (comp_inst) {
				SMJS_SET_RVAL( OBJECT_TO_JSVAL(comp_inst->wid->obj) );
				SMJS_FREE(c, comp_id);
				return JS_TRUE;
			}
			SMJS_FREE(c, comp_id);
			return JS_TRUE;
		}
	}

	SMJS_FREE(c, comp_id);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_get_interface)
{
	u32 idx;
	GF_WidgetInterface *ifce;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !argc || !JSVAL_IS_INT(argv[0]) ) return JS_FALSE;

	idx = JSVAL_TO_INT(argv[0]);
	ifce = (GF_WidgetInterface*)gf_list_get(wid->widget->main->interfaces, idx);

	if (ifce) {
		if (!ifce->obj) {
			ifce->obj = JS_NewObject(c, &wid->widget->wm->widgetAnyClass._class, 0, 0);
			SMJS_SET_PRIVATE(c, ifce->obj, ifce);
			JS_DefineProperty(c, ifce->obj, "num_messages", INT_TO_JSVAL( gf_list_count(ifce->messages) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, ifce->obj, "type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ifce->type) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, ifce->obj, "serviceProvider", BOOLEAN_TO_JSVAL( ifce->provider ? JS_TRUE : JS_FALSE ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, ifce->obj, "multipleBinding", BOOLEAN_TO_JSVAL( ifce->multiple_binding ? JS_TRUE : JS_FALSE ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineFunction(c, ifce->obj, "get_message", wm_widget_get_message, 1, 0);
			gf_js_add_root(c, &ifce->obj, GF_JSGC_OBJECT);
		}
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(ifce->obj) );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_bind_output_trigger)
{
	GF_WidgetMessage *msg;
	GF_WidgetPin *param;
	SVG_handlerElement *handler;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);

	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_FALSE) );
	if (!wid || !wid->scene || (argc!=3)) return JS_TRUE;

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[2])) return JS_TRUE;

	msg = (GF_WidgetMessage *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]));
	if (!msg) return JS_TRUE;
	param = msg->output_trigger;
	if (!param) return JS_TRUE;


	handler = wm_create_scene_listener(wid, param);
	if (!handler) return JS_TRUE;
	handler->js_fun_val = * (u64 *) & argv[1];
	gf_js_add_root(c, &handler->js_fun_val, GF_JSGC_VAL);
	handler->evt_listen_obj = wid;
	handler->js_fun = param;
	handler->js_context = c;
	handler->handle_event = wm_handle_dom_event;
	handler->sgprivate->UserPrivate = JSVAL_TO_OBJECT(argv[2]);

	gf_list_add(wid->output_triggers, handler);
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_get_context)
{
	GF_BitStream *bs;
	u32 i, count;
	const char *str;
	char *att;
	SMJS_OBJ
	//SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;
	if (!wid->scene) {
		SMJS_SET_RVAL(JSVAL_NULL);
		return JS_TRUE;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	str = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<contextInformation xmlns=\"urn:mpeg:mpegu:schema:widgets:contextinfo:2010\">\n";
	gf_bs_write_data(bs, (const char *) str, (u32) strlen(str) );

	count = gf_list_count(wid->widget->main->preferences);
	for (i=0; i<count; i++) {
		GF_WidgetPreference *pref = (GF_WidgetPreference*)gf_list_get(wid->widget->main->preferences, i);

		/*preference is read only, do not include in context*/
		if (pref->flags & GF_WM_PREF_READONLY) continue;
		/*preference is not migratable, do not include in context*/
		if (!(pref->flags & GF_WM_PREF_MIGRATE)) continue;

		str = " <preference name=\"";
		gf_bs_write_data(bs, (const char *) str, (u32) strlen(str) );

		gf_bs_write_data(bs, (const char *) pref->name, (u32) strlen(pref->name) );

		str = "\" value=\"";
		gf_bs_write_data(bs, (const char *) str, (u32) strlen(str) );

		/*read from node*/
		if (pref->connectTo && pref->connectTo->attribute) {
			GF_FieldInfo info;
			GF_Node *n = gf_sg_find_node_by_name(wid->scene, pref->connectTo->node);
			if (n) {

#ifndef GPAC_DISABLE_SVG
				if ((n->sgprivate->tag >= GF_NODE_FIRST_DOM_NODE_TAG) && !strcmp(pref->connectTo->attribute, "textContent")) {
					char *txt = gf_dom_flatten_textContent(n);
					gf_bs_write_data(bs, (const char *) txt, (u32) strlen(txt) );
				} else
#endif
					if (gf_node_get_field_by_name(n, pref->connectTo->attribute, &info)==GF_OK) {
						att = gf_node_dump_attribute(n, &info);
						if (att) {
							gf_bs_write_data(bs, (const char *)  att, (u32) strlen(att) );
							gf_free(att);
						}
					}
			}
		}
		/*read from config*/
		else {
			att = (char *)gf_opts_get_key((const char *) wid->secname, pref->name);
			if (!att) att = pref->value;

			if (att) gf_bs_write_data(bs, (const char *) att, (u32) strlen(att) );
		}

		str = "\"/>\n";
		gf_bs_write_data(bs, (const char *)  str, (u32) strlen(str) );
	}
	str = "</contextInformation>\n";
	gf_bs_write_data(bs, (const char *)  str, (u32) strlen(str) );

	gf_bs_write_u8(bs, 0);
	att = NULL;
	gf_bs_get_content(bs, &att, &count);
	gf_bs_del(bs);

	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, att) ) );
	gf_free(att);

	return JS_TRUE;
}


static SMJS_FUNC_PROP_GET( wm_widget_getProperty)

JSString *s;
char *prop_name;
const char *opt;
GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
if (!wid) return JS_FALSE;

if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
if (!prop_name) return JS_FALSE;

/*
		Manifest properties
*/
if (!strcmp(prop_name, "manifest")) {
	s = JS_NewStringCopyZ(c, wid->widget->manifest_path);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "url")) {
	s = JS_NewStringCopyZ(c, wid->widget->local_path ? wid->widget->local_path : wid->widget->url);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "main")) {
	s = JS_NewStringCopyZ(c, wid->widget->main->relocated_src);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "localizedSrc")) {
	s = JS_NewStringCopyZ(c, wid->widget->main->src);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "mainEncoding")) {
	s = JS_NewStringCopyZ(c, wid->widget->main->encoding);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "mainMimeType")) {
	s = JS_NewStringCopyZ(c, wid->widget->main->mimetype);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "defaultWidth")) {
	*vp = INT_TO_JSVAL(wid->widget->width);
}
else if (!strcmp(prop_name, "defaultHeight")) {
	*vp = INT_TO_JSVAL(wid->widget->height);
}
else if (!strcmp(prop_name, "icons")) {
	u32 i, count;
	JSObject *arr;
	count = gf_list_count(wid->widget->icons);
	arr = JS_NewArrayObject(c, count, NULL);
	for (i = 0; i<count; i++) {
		GF_WidgetContent *icon = (GF_WidgetContent*)gf_list_get(wid->widget->icons, i);
		if (icon) {
			char *abs_reloc_url;
			jsval icon_obj_val;
			JSObject *icon_obj = JS_NewObject(c, &wid->widget->wm->widgetAnyClass._class, 0, 0);
			SMJS_SET_PRIVATE(c, icon_obj, icon);
			JS_DefineProperty(c, icon_obj, "src", STRING_TO_JSVAL( JS_NewStringCopyZ(c, icon->src) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			if (strlen(icon->relocated_src)) abs_reloc_url = gf_url_concatenate(wid->widget->url, icon->relocated_src);
			else abs_reloc_url = gf_strdup("");
			JS_DefineProperty(c, icon_obj, "relocated_src", STRING_TO_JSVAL( JS_NewStringCopyZ(c, abs_reloc_url) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, icon_obj, "width", INT_TO_JSVAL( icon->width ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, icon_obj, "height", INT_TO_JSVAL( icon->height ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			icon_obj_val = OBJECT_TO_JSVAL(icon_obj);
			JS_SetElement(c, arr, i, &icon_obj_val);
			gf_free(abs_reloc_url);
		}
	}
	*vp = OBJECT_TO_JSVAL(arr);
}
else if (!strcmp(prop_name, "preferences")) {
	u32 i, count;
	JSObject *arr;
	count = gf_list_count(wid->widget->main->preferences);
	arr = JS_NewArrayObject(c, count, NULL);
	for (i = 0; i<count; i++) {
		GF_WidgetPreference *pref = (GF_WidgetPreference*)gf_list_get(wid->widget->main->preferences, i);
		if (pref) {
			jsval pref_obj_val;
			JSObject *pref_obj = JS_NewObject(c, &wid->widget->wm->widgetAnyClass._class, 0, 0);
			SMJS_SET_PRIVATE(c, pref_obj, pref);
			JS_DefineProperty(c, pref_obj, "name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, pref->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, pref_obj, "value", STRING_TO_JSVAL( JS_NewStringCopyZ(c, pref->value) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, pref_obj, "readonly", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ((pref->flags & GF_WM_PREF_READONLY)?"true":"false")) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			pref_obj_val = OBJECT_TO_JSVAL(pref_obj);
			JS_SetElement(c, arr, i, &pref_obj_val);
		}
	}
	*vp = OBJECT_TO_JSVAL(arr);
}
else if (!strcmp(prop_name, "features")) {
	u32 i, count;
	JSObject *arr;
	count = gf_list_count(wid->widget->features);
	arr = JS_NewArrayObject(c, count, NULL);
	for (i = 0; i<count; i++) {
		GF_WidgetFeature *feat = (GF_WidgetFeature*)gf_list_get(wid->widget->features, i);
		if (feat) {
			jsval feat_obj_val;
			JSObject *feat_obj = JS_NewObject(c, &wid->widget->wm->widgetAnyClass._class, 0, 0);
			SMJS_SET_PRIVATE(c, feat_obj, feat);
			JS_DefineProperty(c, feat_obj, "name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, feat->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, feat_obj, "required", BOOLEAN_TO_JSVAL( (feat->required? JS_TRUE : JS_FALSE) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			{
				u32 j, pcount;
				JSObject *params_arr;
				pcount = gf_list_count(feat->params);
				params_arr = JS_NewArrayObject(c, pcount, NULL);
				for (j=0; j < pcount; j++) {
					GF_WidgetFeatureParam *param = (GF_WidgetFeatureParam*)gf_list_get(feat->params, j);
					JSObject *param_obj = JS_NewObject(c, &wid->widget->wm->widgetAnyClass._class, 0, 0);
					jsval param_obj_val;
					SMJS_SET_PRIVATE(c, param_obj, param);
					JS_DefineProperty(c, param_obj, "name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, param->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
					JS_DefineProperty(c, param_obj, "value", STRING_TO_JSVAL( JS_NewStringCopyZ(c, param->value) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
					param_obj_val = OBJECT_TO_JSVAL(param_obj);
					JS_SetElement(c, params_arr, j, &param_obj_val);
				}
				JS_DefineProperty(c, feat_obj, "params", OBJECT_TO_JSVAL(params_arr), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			}
			feat_obj_val = OBJECT_TO_JSVAL(feat_obj);
			JS_SetElement(c, arr, i, &feat_obj_val);
		}
	}
	*vp = OBJECT_TO_JSVAL(arr);
}

else if (!strcmp(prop_name, "components")) {
	u32 i, count;
	jsval val;
	JSObject *arr;
	count = gf_list_count(wid->components);
	arr = JS_NewArrayObject(c, count, NULL);
	for (i = 0; i<count; i++) {
		GF_WidgetComponentInstance *comp = (GF_WidgetComponentInstance*)gf_list_get(wid->components, i);
		val = OBJECT_TO_JSVAL(comp->wid->obj);
		JS_SetElement(c, arr, i, &val);
	}
	*vp = OBJECT_TO_JSVAL(arr);
}
else if (!strcmp(prop_name, "identifier")) {
	s = JS_NewStringCopyZ(c, wid->widget->identifier);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "name")) {
	s = JS_NewStringCopyZ(c, wid->widget->name);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "shortName")) {
	s = JS_NewStringCopyZ(c, wid->widget->shortname);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "authorName")) {
	s = JS_NewStringCopyZ(c, wid->widget->authorName);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "authorEmail")) {
	s = JS_NewStringCopyZ(c, wid->widget->authorEmail);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "authorHref")) {
	s = JS_NewStringCopyZ(c, wid->widget->authorHref);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "description")) {
	s = JS_NewStringCopyZ(c, wid->widget->description);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "viewmodes")) {
	if (wid->widget->viewmodes) s = JS_NewStringCopyZ(c, wid->widget->viewmodes);
	else s = JS_NewStringCopyZ(c, "");
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "license")) {
	s = JS_NewStringCopyZ(c, wid->widget->license);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "licenseHref")) {
	s = JS_NewStringCopyZ(c, wid->widget->licenseHref);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "version")) {
	s = JS_NewStringCopyZ(c, wid->widget->version);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "uuid")) {
	s = JS_NewStringCopyZ(c, wid->widget->uuid);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "discardable")) {
	*vp = BOOLEAN_TO_JSVAL( wid->widget->discardable ? JS_TRUE : JS_FALSE);
}
else if (!strcmp(prop_name, "multipleInstances")) {
	*vp = BOOLEAN_TO_JSVAL( wid->widget->multipleInstance ? JS_TRUE : JS_FALSE);
}

/*
		Widget Manager special properties (common to all implementations)
*/
else if (!strcmp(prop_name, "permanent")) {
	*vp = BOOLEAN_TO_JSVAL( wid->permanent ? JS_TRUE : JS_FALSE);
}
else if (!strcmp(prop_name, "is_component")) {
	*vp = BOOLEAN_TO_JSVAL( wid->parent ? JS_TRUE : JS_FALSE);
}
else if (!strcmp(prop_name, "parent")) {
	*vp = wid->parent ? OBJECT_TO_JSVAL(wid->parent->obj) : JSVAL_NULL;
}
else if (!strcmp(prop_name, "activated")) {
	*vp = BOOLEAN_TO_JSVAL( wid->activated ? JS_TRUE : JS_FALSE);
}
else if (!strcmp(prop_name, "section")) {
	s = JS_NewStringCopyZ(c, (const char *) wid->secname);
	*vp = STRING_TO_JSVAL(s);
}
else if (!strcmp(prop_name, "num_instances")) {
	*vp = INT_TO_JSVAL( wid->widget->nb_instances);
}
else if (!strcmp(prop_name, "num_interfaces")) {
	*vp = INT_TO_JSVAL( gf_list_count(wid->widget->main->interfaces));
}
else if (!strcmp(prop_name, "num_components")) {
	*vp = INT_TO_JSVAL( gf_list_count(wid->components));
}
else if (!strcmp(prop_name, "num_bound_interfaces")) {
	*vp = INT_TO_JSVAL( gf_list_count(wid->bound_ifces));
}
/*all variables used by the WidgetManager script but not stored*/
else if (!strcmp(prop_name, "originating_device_ip")
         || !strcmp(prop_name, "originating_device")
         || !strcmp(prop_name, "device")
        ) {
}
/*
		Widget properties, common to each implementation
*/
else {
	char szName[1024];
	sprintf(szName, "WM:%s", prop_name);
	opt = gf_opts_get_key((const char *) wid->secname, szName);
	if (opt) {
		Double val=0;
		if (!strcmp(opt, "true")) *vp = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!strcmp(opt, "false")) *vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		else if (sscanf(opt, "%lf", &val)==1) {
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, val) );
		} else {
			s = JS_NewStringCopyZ(c, opt);
			*vp = STRING_TO_JSVAL(s);
		}
	}
}
SMJS_FREE(c, prop_name);
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET( wm_widget_setProperty)

char szVal[32];
jsdouble val;
char *prop_name;
GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
if (!wid) return JS_FALSE;

if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));

/*internal to WidgetManager, never stored*/
if (!strcmp(prop_name, "permanent")) {
	wid->permanent = (JSVAL_TO_BOOLEAN(*vp)==JS_TRUE) ? GF_TRUE : GF_FALSE;
}

/*any widget properties*/
else {
	char szName[1024], *value, *_val = NULL;

	if (JSVAL_IS_STRING(*vp)) {
		value = _val = SMJS_CHARS(c, *vp);
		if (!value) value = "";
	}
	else if (JSVAL_IS_BOOLEAN(*vp)) {
		strcpy(szVal, (JSVAL_TO_BOOLEAN(*vp)==JS_TRUE) ? "true" : "false");
		value = szVal;
	}
	else if (JSVAL_IS_NUMBER(*vp)) {
		JS_ValueToNumber(c, *vp, &val);
		sprintf(szVal, "%f", val);
		value = szVal;
	} else {
		SMJS_FREE(c, prop_name);
		return JS_TRUE;
	}

	sprintf(szName, "WM:%s", prop_name);
	gf_cfg_set_key(wid->widget->wm->term->user->config, (const char *) wid->secname, szName, value);
	SMJS_FREE(c, _val);
}

SMJS_FREE(c, prop_name);
return JS_TRUE;
}



static JSBool wm_widget_bind_interface_ex(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool is_unbind)
{
	u32 i, count;
	GF_WidgetInterfaceInstance *bifce;
	GF_WidgetInterface *ifce;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !wid->scene) return JS_FALSE;

	ifce = NULL;
	if (argc) {
		if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
		ifce = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
		if (!ifce) return JS_FALSE;
	}

	if (!is_unbind) {
		char *hostname;
		JSObject *cookie;
		if ((argc<3) || !JSVAL_IS_OBJECT(argv[1]) || !JSVAL_IS_STRING(argv[2])) return JS_FALSE;

		cookie = JSVAL_TO_OBJECT(argv[1]);
		hostname = SMJS_CHARS(c, argv[2]);
		count = gf_list_count(wid->bound_ifces);
		for (i=0; i<count; i++) {
			bifce = (GF_WidgetInterfaceInstance*)gf_list_get(wid->bound_ifces, i);
			if (!strcmp(bifce->ifce->type, ifce->type) && (bifce->cookie==cookie) ) {
				SMJS_FREE(c, hostname);
				return JS_TRUE;
			}
		}
		GF_SAFEALLOC(bifce, GF_WidgetInterfaceInstance);
		bifce->wid = wid;
		bifce->ifce = ifce;
		bifce->cookie = cookie;
		bifce->hostname = gf_strdup(hostname);
		SMJS_FREE(c, hostname);
		gf_list_add(wid->bound_ifces, bifce);

		if (ifce->bind_action) {
			return wm_widget_set_scene_input_value(c, obj, 1, argv, rval, 2, NULL, NULL, NULL);
		} else {
			widget_on_interface_bind(bifce, GF_FALSE);
		}
	} else {
		JSObject *cookie = NULL;
		if ((argc==2) && JSVAL_IS_OBJECT(argv[1])) cookie = JSVAL_TO_OBJECT(argv[1]);

		count = gf_list_count(wid->bound_ifces);
		for (i=0; i<count; i++) {
			bifce = (GF_WidgetInterfaceInstance*)gf_list_get(wid->bound_ifces, i);
			if (!ifce || ( !strcmp(bifce->ifce->type, ifce->type) && (bifce->cookie==cookie)) ) {
				gf_list_rem(wid->bound_ifces, i);
				if (bifce->ifce->unbind_action) {
					wm_widget_set_scene_input_value(c, NULL, 0, NULL, rval, 3, wid, bifce->ifce->unbind_action, NULL);
				} else {
					widget_on_interface_bind(bifce, GF_TRUE);
				}
				if (ifce) {
					/*unregister our message handlers*/
					count = gf_list_count(wid->output_triggers);
					for (i=0; i<count; i++) {
						u32 j, c2, found;
						GF_DOMHandler *handler = (GF_DOMHandler*)gf_list_get(wid->output_triggers, i);
						GF_WidgetPin *param = (GF_WidgetPin*)handler->js_fun;

						if (handler->sgprivate->UserPrivate != cookie) continue;
						found = 0;
						c2 = gf_list_count(bifce->ifce->messages);
						for (j=0; j<c2; j++) {
							GF_WidgetMessage *msg = (GF_WidgetMessage*)gf_list_get(bifce->ifce->messages, j);
							if (msg->output_trigger == param) {
								found = 1;
								break;
							}
						}
						if (found) {
#ifndef GPAC_DISABLE_SVG
							GF_Node *listener = handler->sgprivate->parents->node;
							gf_dom_listener_del(listener, listener->sgprivate->UserPrivate);
#endif
							gf_list_rem(wid->output_triggers, i);
							i--;
							count--;
						}
					}
				}
				wm_delete_interface_instance(wid->widget->wm, bifce);
				if (ifce) return JS_TRUE;
				i--;
				count--;
			}
		}
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_widget_bind_interface)
{
	SMJS_OBJ
	SMJS_ARGS
	SMJS_DECL_RVAL
	return wm_widget_bind_interface_ex(c, obj, argc, argv, rval, GF_FALSE);
}
static JSBool SMJS_FUNCTION(wm_widget_unbind_interface)
{
	SMJS_OBJ
	SMJS_ARGS
	SMJS_DECL_RVAL
	return wm_widget_bind_interface_ex(c, obj, argc, argv, rval, GF_TRUE);
}


static JSBool SMJS_FUNCTION(wm_widget_deactivate)
{
	u32 i, count;
	jsval funval;
	SMJS_OBJ
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;

	/*widget is a component of another widget, unregister*/
	if (wid->parent) {
		GF_WidgetInstance *par_wid = wid->parent;
		count = gf_list_count(par_wid->components);
		for (i=0; i<count; i++) {
			GF_WidgetComponentInstance *comp = (GF_WidgetComponentInstance*)gf_list_get(par_wid->components, i);
			if (comp->wid == wid) {
				gf_list_rem(par_wid->components, i);
				gf_free(comp);
				break;
			}
		}
		wid->parent = NULL;
	}

	/*remove all components*/
	while (gf_list_count(wid->components)) {
		GF_WidgetComponentInstance *comp = (GF_WidgetComponentInstance*)gf_list_get(wid->components, 0);
		wm_deactivate_component(c, wid, NULL, comp);
		gf_list_rem(wid->components, 0);
	}

	/*mark the widget as deactivated, so it is no longer valid when checking all widgets bindings*/
	wid->activated = GF_FALSE;

	/*unbind existing widgets*/
	JS_LookupProperty(wid->widget->wm->ctx, wid->widget->wm->obj, "unbind_widget", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		jsval an_argv[1];
		an_argv[0] = OBJECT_TO_JSVAL(obj);
		JS_CallFunctionValue(wid->widget->wm->ctx, wid->widget->wm->obj, funval, 1, an_argv, SMJS_GET_RVAL );
	}

	/*unbind all interfaces of this widget*/
	wm_widget_bind_interface_ex(c, obj, 0, NULL, SMJS_GET_RVAL, GF_TRUE);

	/*detach scene now that all unbind events have been sent*/
	wid->scene = NULL;
	return JS_TRUE;
}


static void wm_widget_jsbind(GF_WidgetManager *wm, GF_WidgetInstance *wid)
{
	if (wid->obj)
		return;
	wid->obj = JS_NewObject(wm->ctx, &wm->wmWidgetClass._class, 0, 0);
	SMJS_SET_PRIVATE(wm->ctx, wid->obj, wid);
	/*protect from GC*/
	gf_js_add_root(wm->ctx, &wid->obj, GF_JSGC_OBJECT);
}

void wm_deactivate_component(JSContext *c, GF_WidgetInstance *wid, GF_WidgetComponent *comp, GF_WidgetComponentInstance *comp_inst)
{
	jsval fval, rval;

	if (!comp_inst) {
		u32 i=0;
		while ((comp_inst = (GF_WidgetComponentInstance*)gf_list_enum(wid->components, &i))) {
			if (comp_inst->comp == comp) break;
			comp_inst = NULL;
		}
	}
	if (!comp_inst) return;

	if ((JS_LookupProperty(c, wid->widget->wm->obj, "on_widget_remove", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval argv[1];
		argv[0] = OBJECT_TO_JSVAL(comp_inst->wid->obj);
		JS_CallFunctionValue(wid->widget->wm->ctx, wid->obj, fval, 1, argv, &rval);
	}
}

GF_WidgetComponentInstance *wm_activate_component(JSContext *c, GF_WidgetInstance *wid, GF_WidgetComponent *comp, Bool skip_wm_notification)
{
	u32 i, count;
	const char *fun_name = NULL;
	jsval fval, rval;
	GF_WidgetComponentInstance *comp_inst;
	GF_WidgetInstance *comp_wid;

	comp_wid = NULL;
	if (comp->src) {
		char *url = gf_url_concatenate(wid->widget->url, comp->src);

		count = gf_list_count(wid->widget->wm->widget_instances);
		for (i=0; i<count; i++) {
			comp_wid = (GF_WidgetInstance*)gf_list_get(wid->widget->wm->widget_instances, i);
			if (!strcmp(comp_wid->widget->url, url) && !comp_wid->parent) break;
			comp_wid = NULL;
		}
		if (!comp_wid) {
			comp_wid = wm_load_widget(wid->widget->wm, url, 0, GF_FALSE);
			if (comp_wid) comp_wid->permanent = GF_FALSE;
		}
		gf_free(url);
	}
	if (!comp_wid) return NULL;

	if (!comp_wid->activated)
		fun_name = "on_widget_add";

	GF_SAFEALLOC(comp_inst, GF_WidgetComponentInstance);
	if (!comp_inst) return NULL;
	comp_inst->comp = comp;
	comp_inst->wid = comp_wid;
	comp_wid->parent = wid;

	gf_list_add(wid->components, comp_inst);

	if (!comp_inst->wid->obj) wm_widget_jsbind(wid->widget->wm, comp_inst->wid);

	if (!skip_wm_notification && fun_name && (JS_LookupProperty(c, wid->widget->wm->obj, fun_name, &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval argv[1];
		argv[0] = OBJECT_TO_JSVAL(comp_inst->wid->obj);
		JS_CallFunctionValue(wid->widget->wm->ctx, wid->obj, fval, 1, argv, &rval);
	}
	if (comp_inst) return comp_inst;
	return NULL;
}

static JSBool SMJS_FUNCTION(wm_widget_is_interface_bound)
{
	u32 i, count;
	JSObject *cookie;
	GF_WidgetInterface *ifce;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !wid->scene || (argc<1) || !JSVAL_IS_OBJECT(argv[0]) ) return JS_FALSE;

	ifce = SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!ifce) return JS_FALSE;
	cookie = NULL;
	if ((argc==2) && JSVAL_IS_OBJECT(argv[1]) )
		cookie = JSVAL_TO_OBJECT(argv[1]);

	SMJS_SET_RVAL(BOOLEAN_TO_JSVAL(JS_FALSE));
	count = gf_list_count(wid->bound_ifces);
	for (i=0; i<count; i++) {
		GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance*)gf_list_get(wid->bound_ifces, i);
		if (!strcmp(bifce->ifce->type, ifce->type) && (!cookie || (bifce->cookie==cookie))) {
			SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
			break;
		}
	}
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(wm_load)
{
	u32 i, count;
	char *manifest, *url, *widget_ctx;
	GF_WidgetInstance *wid;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	manifest = SMJS_CHARS(c, argv[0]);

	url = NULL;
	if ((argc==2) && ! JSVAL_IS_NULL(argv[1]) && JSVAL_IS_OBJECT(argv[1])) {
		GF_WidgetInstance *parent_widget;
		if (!GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[1]), &wm->wmWidgetClass, NULL) ) return JS_FALSE;
		parent_widget = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[1]) );

		if (parent_widget->widget->url) url = gf_url_concatenate(parent_widget->widget->url, manifest);
	}

	widget_ctx = NULL;
	if ((argc==3) && !JSVAL_IS_NULL(argv[2]) && JSVAL_IS_STRING(argv[2])) {
		widget_ctx = SMJS_CHARS(c, argv[2]);
	}

	if (!url) {
		url = gf_strdup(manifest);
	}

	wid=NULL;
	count = gf_list_count(wm->widget_instances);
	for (i=0; i<count; i++) {
		wid = (GF_WidgetInstance*)gf_list_get(wm->widget_instances, i);
		if (!strcmp(wid->widget->url, url) && !wid->activated) break;
		wid = NULL;
	}
	if (!wid) {
		wid = wm_load_widget(wm, url, 0, GF_TRUE);
	}
	if (url) gf_free(url);

	/*parse context if any*/
	if (wid && wid->mpegu_context) {
		gf_xml_dom_del(wid->mpegu_context);
		wid->mpegu_context = NULL;
	}
	if (wid && widget_ctx && strlen(widget_ctx)) {
		GF_Err e;
		GF_XMLNode *context = NULL;
		wid->mpegu_context = gf_xml_dom_new();
		e = gf_xml_dom_parse_string(wid->mpegu_context, widget_ctx);
		if (!e) {
			context = gf_xml_dom_get_root(wid->mpegu_context);
			if (strcmp(context->name, "contextInformation")) context = NULL;
		} else {
		}

		if (!context && wid->mpegu_context) {
			gf_xml_dom_del(wid->mpegu_context);
			wid->mpegu_context = NULL;
		}
	}

	if (wid) {
		wm_widget_jsbind(wm, wid);
		SMJS_SET_RVAL(OBJECT_TO_JSVAL(wid->obj));
	}
	SMJS_FREE(c, manifest);
	SMJS_FREE(c, widget_ctx);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_unload)
{
	GF_WidgetInstance *wid;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

	if (!GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &wm->wmWidgetClass, NULL) ) return JS_FALSE;
	wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!wid) return JS_TRUE;

	/*unless explecetely requested, remove the section*/
	if ((argc!=2) || !JSVAL_IS_BOOLEAN(argv[1]) || (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ) {
		/*create section*/
		gf_cfg_del_section(wm->term->user->config, (const char *) wid->secname);
		gf_cfg_set_key(wm->term->user->config, "Widgets", (const char *) wid->secname, NULL);
	}
	wm_delete_widget_instance(wm, wid);
	return JS_TRUE;
}



static SMJS_FUNC_PROP_GET( wm_getProperty)

char *prop_name;
GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
if (!wm) return JS_FALSE;

if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
if (!prop_name) return JS_FALSE;

if (!strcmp(prop_name, "num_widgets")) {
	*vp = INT_TO_JSVAL(gf_list_count(wm->widget_instances));
}
else if (!strcmp(prop_name, "last_widget_dir")) {
	const char *opt = gf_opts_get_key("Widgets", "last_widget_dir");
	if (!opt) opt = "/";
	*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(c, opt));
}
SMJS_FREE(c, prop_name);
return JS_TRUE;
}


static SMJS_FUNC_PROP_SET( wm_setProperty)

char *prop_name;
GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
if (!wm) return JS_FALSE;

if (!JSVAL_IS_STRING(*vp)) return JS_TRUE;
if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));

if (!strcmp(prop_name, "last_widget_dir")) {
	char *v = SMJS_CHARS(c, *vp);
	gf_cfg_set_key(wm->term->user->config, "Widgets", "last_widget_dir", v);
	SMJS_FREE(c, v);
}
SMJS_FREE(c, prop_name);
return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_get)
{
	u32 i;
	GF_WidgetInstance *wid;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
	if (!argc || !JSVAL_IS_INT(argv[0])) return JS_TRUE;

	i = JSVAL_TO_INT(argv[0]);
	wid = (GF_WidgetInstance*)gf_list_get(wm->widget_instances, i);
	if (wid) SMJS_SET_RVAL( OBJECT_TO_JSVAL(wid->obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(wm_find_interface)
{
	char *ifce_name;
	u32 i;
	GF_WidgetInstance *wid;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	ifce_name = SMJS_CHARS(c, argv[0]);
	i=0;
	while ( (wid = (GF_WidgetInstance*)gf_list_enum(wm->widget_instances, &i) )) {
		u32 j=0;
		GF_WidgetInterface *wid_ifce;
		while ((wid_ifce = (GF_WidgetInterface*)gf_list_enum(wid->widget->main->interfaces, &j))) {
			if (!strcmp(wid_ifce->type, ifce_name)) {
				SMJS_SET_RVAL( OBJECT_TO_JSVAL(wid->obj) );
				SMJS_FREE(c, ifce_name);
				return JS_TRUE;
			}
		}
	}
	SMJS_FREE(c, ifce_name);
	return JS_TRUE;
}


const char *wm_xml_get_attr(GF_XMLNode *root, const char *name)
{

	u32 i, count;
	count = gf_list_count(root->attributes);
	for (i=0; i<count; i++) {
		char *sep;
		GF_XMLAttribute *att = gf_list_get(root->attributes, i);
		if (!att->name) continue;
		if (!strcmp(att->name, name)) return att->value;
		sep = strchr(att->name, ':');
		if (sep && !strcmp(sep+1, name)) return att->value;
	}
	return NULL;
}

/* TODO Implement real language check according to BCP 47*/
static Bool wm_check_language(const char *xml_lang_value, const char *user_locale)
{
	Bool ret = GF_FALSE;
	char *sep, *val;
	val = (char*)xml_lang_value;
	while (!ret) {
		sep = strchr(val, ';');
		if (sep) sep[0] = 0;
		if (strstr(user_locale, val)) ret = GF_TRUE;
		if (sep) {
			sep[0] = ';';
			val = sep+1;
		} else {
			break;
		}
	}
	return ret;
}

static GF_XMLNode *wm_xml_find(GF_XMLNode *root, const char *ns_prefix, const char *name, const char *user_locale)
{
	GF_XMLNode *localized = NULL;
	GF_XMLNode *non_localized = NULL;
	u32 i, count;

	if (!root) return NULL;

	count = gf_list_count(root->content);
	for (i=0; i<count; i++) {
		GF_XMLNode *n = (GF_XMLNode*)gf_list_get(root->content, i);
		if (n->type==GF_XML_NODE_TYPE && n->name && !strcmp(n->name, name) && ((!ns_prefix && !n->ns) || (ns_prefix && n->ns && !strcmp(ns_prefix, n->ns)))) {
			const char *lang = wm_xml_get_attr(n, "xml:lang");
			if (!lang) {
				if (!non_localized) non_localized = n;
			} else {
				if (user_locale && wm_check_language(lang, user_locale) && !localized) localized = n;
			}
		}
	}
	if (localized) return localized;
	else return non_localized;
}

static GF_WidgetPin *wm_parse_pin(const char *value, u16 type, const char *pin_name, const char *scriptType, const char *default_value)
{
	GF_WidgetPin *pin;
	char *sep;

	if (!value && !scriptType && !default_value) return NULL;

	GF_SAFEALLOC(pin, GF_WidgetPin);
	if (!pin) return NULL;
	
	pin->type = type;
	if (pin_name) pin->name = gf_strdup(pin_name);

	if (value) {
		sep = strrchr(value, '.');
		if (!sep && (type==GF_WM_PREF_CONNECT)) {
			gf_free(pin);
			return NULL;
		}

		/*node.event || node.attribute*/
		if (sep) {
			sep[0] = 0;
			pin->node = gf_strdup(value);
			pin->attribute = gf_strdup(sep+1);
			sep[0] = '.';
		}
		/*script function*/
		else {
			pin->node = gf_strdup(value);
		}
	}

	if (scriptType) {
		if (!strcmp(scriptType, "boolean")) pin->script_type = GF_WM_PARAM_SCRIPT_BOOL;
		else if (!strcmp(scriptType, "number")) pin->script_type = GF_WM_PARAM_SCRIPT_NUMBER;
	} else if (default_value) {
		pin->default_value = gf_strdup(default_value);
	}
	return pin;
}

static void wm_parse_mpegu_content_element(GF_WidgetContent *content, GF_XMLNode *root, const char *ns_prefix, GF_List *global_prefs)
{
	GF_XMLNode *ifces, *context, *pref_node;
	GF_WidgetMessage *msg;
	GF_WidgetInterface *ifce;
	GF_XMLNode *ifce_node;
	GF_WidgetPreference *pref;
	const char *att;
	u32 i = 0;

	ifces = wm_xml_find(root, ns_prefix, "interfaces", NULL);
	if (ifces) {

		/*get all interface element*/
		while ((ifce_node = (GF_XMLNode*)gf_list_enum(ifces->content, &i))) {
			GF_XMLNode *msg_node;
			u32 j;
			const char *ifce_type, *act;
			if (ifce_node->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(ifce_node->name, "interface")) continue;
			ifce_type = wm_xml_get_attr(ifce_node, "type");
			if (!ifce_type) continue;

			GF_SAFEALLOC(ifce, GF_WidgetInterface);
			ifce->type = gf_strdup(ifce_type);
			ifce->messages = gf_list_new();
			ifce->content = content;
			gf_list_add(content->interfaces, ifce);

			act = wm_xml_get_attr(ifce_node, "serviceProvider");
			if (act && !strcmp(act, "true")) ifce->provider = GF_TRUE;

			act = wm_xml_get_attr(ifce_node, "multipleBindings");
			if (act && !strcmp(act, "true")) ifce->multiple_binding = GF_TRUE;

			act = wm_xml_get_attr(ifce_node, "required");
			if (act && !strcmp(act, "true")) ifce->required = GF_TRUE;

			act = wm_xml_get_attr(ifce_node, "connectTo");
			if (act) ifce->connectTo = gf_strdup(act);

			act = wm_xml_get_attr(ifce_node, "bindAction");
			if (act) {
				ifce->bind_action = wm_parse_pin(act, GF_WM_BIND_ACTION, NULL, NULL, NULL);
			}
			act = wm_xml_get_attr(ifce_node, "unbindAction");
			if (act) {
				ifce->unbind_action = wm_parse_pin(act, GF_WM_UNBIND_ACTION, NULL, NULL, NULL);
			}

			j=0;
			while ((msg_node = (GF_XMLNode*)gf_list_enum(ifce_node->content, &j))) {
				u32 k;
				GF_XMLNode *par_node;
				const char *msg_name, *action;
				if (msg_node->type != GF_XML_NODE_TYPE) continue;
				if (strcmp(msg_node->name, "messageIn") && strcmp(msg_node->name, "messageOut")) continue;

				msg_name = wm_xml_get_attr(msg_node, "name");
				if (!msg_name) continue;
				GF_SAFEALLOC(msg, GF_WidgetMessage);
				msg->name = gf_strdup(msg_name);
				msg->params = gf_list_new();
				msg->ifce = ifce;
				if (!strcmp(msg_node->name, "messageOut")) msg->is_output = 1;

				gf_list_add(ifce->messages, msg);

				/*get inputAction*/
				action = wm_xml_get_attr(msg_node, "inputAction");
				if (action) {
					msg->input_action = wm_parse_pin(action, GF_WM_INPUT_ACTION, NULL, NULL, NULL);
				}

				/*get outputTrigger*/
				action = wm_xml_get_attr(msg_node, "outputTrigger");
				if (action) {
					msg->output_trigger = wm_parse_pin(action, GF_WM_OUTPUT_TRIGGER, NULL, NULL, NULL);
				}

				/*get params*/
				k=0;
				while ((par_node=gf_list_enum(msg_node->content, &k))) {
					GF_WidgetPin *wpin;
					u16 type;
					const char *par_name, *att_name;
					if (par_node->type != GF_XML_NODE_TYPE) continue;
					if (strcmp(par_node->name, "input") && strcmp(par_node->name, "output")) continue;


					par_name = wm_xml_get_attr(par_node, "name");
					/*invalid param, discard message*/
					if (!par_name) {
						gf_list_del_item(ifce->messages, msg);
						gf_list_del(msg->params);
						gf_free(msg->name);
						gf_free(msg);
						break;
					}

					if (!strcmp(par_node->name, "output")) {
						type = GF_WM_PARAM_OUTPUT;
						att_name = wm_xml_get_attr(par_node, "attributeModified");
					} else {
						type = GF_WM_PARAM_INPUT;
						att_name = wm_xml_get_attr(par_node, "setAttribute");
					}
					wpin = wm_parse_pin(att_name, type, par_name, wm_xml_get_attr(par_node, "scriptParamType"), wm_xml_get_attr(par_node, "default"));
					if (!wpin) continue;
					wpin->msg = msg;

					gf_list_add(msg->params, wpin);
				}
			}
		}
	}

	/*get all component elements*/
	i=0;
	while (root && (ifce_node = gf_list_enum(root->content, &i))) {
		GF_XMLNode *req_node;
		u32 j;
		const char *src, *id, *act;
		GF_WidgetComponent *comp;
		if (ifce_node->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(ifce_node->name, "component")) continue;
		src = wm_xml_get_attr(ifce_node, "src");
		id = wm_xml_get_attr(ifce_node, "id");

		GF_SAFEALLOC(comp, GF_WidgetComponent);
		comp->required_interfaces = gf_list_new();
		comp->content = content;
		if (id) comp->id = gf_strdup(id);
		if (src) comp->src = gf_strdup(src);
		j=0;
		while ((req_node=gf_list_enum(ifce_node->content, &j))) {
			const char *ifce_type;
			if (req_node->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(req_node->name, "requiredInterface")) continue;
			ifce_type = wm_xml_get_attr(req_node, "type");
			if (!ifce_type) continue;
			gf_list_add(comp->required_interfaces, gf_strdup(ifce_type));
		}

		act = wm_xml_get_attr(ifce_node, "activateTrigger");
		if (act) comp->activateTrigger = wm_parse_pin(act, GF_WM_ACTIVATE_TRIGGER, NULL, NULL, NULL);
		act = wm_xml_get_attr(ifce_node, "deactivateTrigger");
		if (act) comp->deactivateTrigger = wm_parse_pin(act, GF_WM_DEACTIVATE_TRIGGER, NULL, NULL, NULL);

		gf_list_add(content->components, comp);
	}

	/*clone global prefs for this content*/
	i=0;
	while ((pref = gf_list_enum(global_prefs, &i))) {
		GF_WidgetPreference *apref;
		GF_SAFEALLOC(apref, GF_WidgetPreference);
		apref->name = gf_strdup(pref->name);
		if (pref->value) apref->value = gf_strdup(pref->value);
		apref->flags = pref->flags;
		gf_list_add(content->preferences, apref);
	}

	context = wm_xml_find(root, ns_prefix, "contextConfiguration", NULL);
	if (context) {
		u32 j;
		/*get all preference elements*/
		i=0;
		while ((pref_node = gf_list_enum(context->content, &i))) {
			const char *att;
			if (pref_node->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(pref_node->name, "preferenceConnect")) continue;
			att = wm_xml_get_attr(pref_node, "name");
			if (!att) continue;

			j=0;
			while ((pref = gf_list_enum(content->preferences, &j))) {
				if (!strcmp(pref->name, att) && !pref->connectTo) break;
			}

			if (!pref) {
				GF_SAFEALLOC(pref, GF_WidgetPreference);
				if (!pref) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[WidgetMan] Failed to allocate widget preference\n"));
					continue;
				}

				pref->name = gf_strdup(att);
				gf_list_add(content->preferences, pref);
			}
			att = wm_xml_get_attr(pref_node, "value");
			if (att) {
				if (pref->value) gf_free(pref->value);
				pref->value = gf_strdup(att);
			}
			att = wm_xml_get_attr(pref_node, "readOnly");
			if (att && !strcmp(att, "true")) pref->flags |= GF_WM_PREF_READONLY;
			att = wm_xml_get_attr(pref_node, "migratable");
			if (att && !strcmp(att, "saveOnly")) pref->flags |= GF_WM_PREF_SAVE;
			else if (att && !strcmp(att, "migrateOnly")) pref->flags |= GF_WM_PREF_MIGRATE;
			else pref->flags |= GF_WM_PREF_SAVE | GF_WM_PREF_MIGRATE;

			att = wm_xml_get_attr(pref_node, "connectTo");
			if (att) pref->connectTo = wm_parse_pin(att, GF_WM_PREF_CONNECT, NULL, NULL, NULL);
		}

		att = wm_xml_get_attr(context, "savedAction");
		if (att) content->savedAction = wm_parse_pin(att, GF_WM_PREF_SAVEDACTION, NULL, NULL, NULL);
		att = wm_xml_get_attr(context, "restoredAction");
		if (att) content->restoredAction = wm_parse_pin(att, GF_WM_PREF_RESTOREDACTION, NULL, NULL, NULL);
		att = wm_xml_get_attr(context, "saveTrigger");
		if (att) content->saveTrigger = wm_parse_pin(att, GF_WM_PREF_SAVETRIGGER, NULL, NULL, NULL);
		att = wm_xml_get_attr(context, "restoreTrigger");
		if (att) content->restoreTrigger = wm_parse_pin(att, GF_WM_PREF_RESTORETRIGGER, NULL, NULL, NULL);
	}
}

/* Implements the W3C P&C rule for getting a single attribute value
   see http://www.w3.org/TR/widgets/#rule-for-getting-a-single-attribute-valu0
   should be different from getting normalized text but for now it's ok
   */
static char *wm_get_single_attribute(const char *input) {
	u32 i, j, len;
	char *output;
	Bool first_space_copied;

	if (!input) return gf_strdup("");

	len = (u32) strlen(input);
	output = gf_malloc(len+1);

	first_space_copied = 1;
	j = 0;
	for (i = 0; i<len; i++) {
		switch (input[i]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (!first_space_copied) {
				output[j] = ' ';
				j++;
				first_space_copied = 1;
			}
			break;
		default:
			output[j] = input[i];
			j++;
			first_space_copied = 0;
		}
	}
	if (j && output[j-1] == ' ') output[j-1] = 0;
	output[j] = 0;
	return output;
}

/* Implements the W3C P&C rule for getting a single attribute value
   see http://www.w3.org/TR/widgets/#rule-for-getting-a-single-attribute-valu0 */
static u32 wm_parse_non_neg(const char *input) {
	u32 result = 0;
	if (strlen(input) && !strchr(input, '-')) {
		if (sscanf(input, "%u", &result) != 1) result = 0;
	}
	return result;
}

/* returns the localized concatenated text content
see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#Node3-textContent
and http://www.w3.org/TR/widgets/#rule-for-getting-text-content0
*/
static char *wm_get_text_content(GF_XMLNode *node, char *inherited_locale, const char *user_locale) {
	if (node->type == GF_XML_TEXT_TYPE) {
		if (node->name) return gf_strdup(node->name);
		else return gf_strdup("");
	} else {
		char *xml_lang = (char *)wm_xml_get_attr(node, "xml:lang");
		if (!xml_lang) xml_lang = inherited_locale;
		/*
		if (xml_lang && user_locale && wm_check_language(xml_lang, user_locale))
		*/
		{
			u32 i, count;
			char *text_content;
			u32 text_content_len = 0;
			text_content = gf_strdup("");
			count = gf_list_count(node->content);
			for (i=0; i<count; i++) {
				GF_XMLNode *child = (GF_XMLNode *)gf_list_get(node->content, i);
				char *child_content = wm_get_text_content(child, xml_lang, user_locale);
				u32 child_content_len = (u32) strlen(child_content);
				text_content = gf_realloc(text_content, text_content_len+child_content_len+1);
				memcpy(text_content+text_content_len, child_content, child_content_len);
				text_content[text_content_len+child_content_len] = 0;
				text_content_len+=child_content_len;
				gf_free(child_content);
			}
			return text_content;
		}
		/*
		} else {
			return gf_strdup("");
		*/
	}
}

static char *wm_get_normalized_text_content(GF_XMLNode *node, char *inherited_locale, const char *user_locale) {
	char *text_content, *result;
	/* first aggregate text content */
	text_content = wm_get_text_content(node, inherited_locale, user_locale);
	/* calling normalization of text content */
	result = wm_get_single_attribute(text_content);
	gf_free(text_content);
	return result;
}

/* When relocating resources over HTTP, we check if the resources for a given locale exists
  on the server by sending a HEAD message with an Accept-Language header. */
void wm_relocate_proc(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	switch (parameter->msg_type) {
	case GF_NETIO_GET_METHOD:
		parameter->name = "HEAD";
		break;
	default:
		return;
	}
}

/* relocate the given resource name (res_name) using registered relocators (e.g. locales, package folder)
   The result is the path of the file, possibly uncompressed, possibly localized.
   The parameter widget_path provides the path for converting relative resource paths into absolute paths.*/
static Bool wm_relocate_url(GF_WidgetManager *wm, const char *widget_path, const char *res_name, char *relocated_name, char *localized_res_name)
{
	Bool ok = 0;
	char *res_url;
	Bool result = gf_term_relocate_url(wm->term, res_name, widget_path, relocated_name, localized_res_name);
	if (result) return result;

	res_url = gf_url_concatenate(widget_path, res_name);

	/*try with HTTP HEAD */
	if (!strnicmp(widget_path, "http", 4)) {
		GF_Err e;
		/*fetch the remote widget manifest synchronously and load it */
		GF_DownloadSession *sess = gf_dm_sess_new(wm->term->downloader, (char *)res_url, GF_NETIO_SESSION_NOT_THREADED, wm_relocate_proc, NULL, &e);
		if (sess) {
			e = gf_dm_sess_process(sess);
			gf_dm_sess_del(sess);
			if (e==GF_OK) {
				const char *opt = gf_opts_get_key("core", "lang");
				strcpy(relocated_name, res_url);
				if (opt)
					sprintf(localized_res_name, "%s/%s", opt, res_name);
				else
					strcpy(localized_res_name, res_name);
				ok = 1;
			}
		}
	}
	gf_free(res_url);
	return ok;
}

/* function that checks if the default start file exists in the widget (package or folder)
   according to the default start files table from the W3C P&C spec.
   The widget path attribute is used to get the associated relocator from the registered relocators */
static void wm_set_default_start_file(GF_WidgetManager *wm, GF_WidgetContent *content, const char *widget_path) {
	Bool result;
	char localized_path[GF_MAX_PATH], relocated_path[GF_MAX_PATH];
	char *mimetype = "text/html";
	result = wm_relocate_url(wm, widget_path, "index.htm", relocated_path, localized_path);
	if (result) {
		mimetype = "text/html";
	} else {
		result = wm_relocate_url(wm, widget_path, "index.html", relocated_path, localized_path);
		if (result) {
			mimetype = "text/html";
		} else {
			result = wm_relocate_url(wm, widget_path, "index.svg", relocated_path, localized_path);
			if (result) {
				mimetype = "image/svg+xml";
			} else {
				result = wm_relocate_url(wm, widget_path, "index.xhtml", relocated_path, localized_path);
				if (result) {
					mimetype = "application/xhtml+xml";
				} else {
					result = wm_relocate_url(wm, widget_path, "index.xht", relocated_path, localized_path);
					if (result) mimetype = "application/xhtml+xml";
				}
			}
		}
	}
	if (content->src) gf_free(content->src);
	content->src = gf_strdup(localized_path);
	if (content->relocated_src) gf_free(content->relocated_src);
	content->relocated_src = gf_strdup(relocated_path);
	if (content->mimetype) gf_free(content->mimetype);
	content->mimetype = gf_strdup(mimetype);
	if (content->encoding) gf_free(content->encoding);
	content->encoding = gf_strdup("utf-8");
}

static GF_WidgetContent *wm_add_icon(GF_Widget *widget, const char *icon_relocated_path, const char *icon_localized_path, const char *uri_fragment)
{
	GF_WidgetContent *icon;
	u32 i, count;
	Bool already_in = 0;

	count = gf_list_count(widget->icons);
	for (i =0; i<count; i++) {
		GF_WidgetContent *in_icon = gf_list_get(widget->icons, i);
		if (!strcmp(icon_localized_path, in_icon->src)) {
			already_in = 1;
			break;
		}
	}
	if (already_in) return NULL;

	GF_SAFEALLOC(icon, GF_WidgetContent);
	if (!icon) return NULL;
	
	if (uri_fragment) {
		icon->src = gf_malloc(strlen(icon_localized_path) + strlen(uri_fragment) + 1);
		if (icon->src) {
			strcpy(icon->src, icon_localized_path);
			strcat(icon->src, uri_fragment);
		}
		icon->relocated_src = gf_malloc(strlen(icon_relocated_path) + strlen(uri_fragment) + 1);
		if (icon->relocated_src) {
			strcpy(icon->relocated_src, icon_relocated_path);
			strcat(icon->relocated_src, uri_fragment);
		}
	} else {
		icon->src = gf_strdup(icon_localized_path);
		icon->relocated_src = gf_strdup(icon_relocated_path);
	}
	icon->interfaces = gf_list_new();
	icon->components = gf_list_new();
	icon->preferences = gf_list_new();
	icon->widget = widget;
	gf_list_add(widget->icons, icon);
	return icon;
}

/* Scans the W3C default icons table and add each icon */
static void wm_set_default_icon_files(GF_WidgetManager *wm, const char *widget_path, GF_Widget *widget) {
	char relocated_path[GF_MAX_PATH], localized_path[GF_MAX_PATH];
	Bool result;

	result = wm_relocate_url(wm, widget_path, "icon.svg", relocated_path, localized_path);
	if (result) wm_add_icon(widget, relocated_path, localized_path, NULL);

	result = wm_relocate_url(wm, widget_path, "icon.ico", relocated_path, localized_path);
	if (result) wm_add_icon(widget, relocated_path, localized_path, NULL);

	result = wm_relocate_url(wm, widget_path, "icon.png", relocated_path, localized_path);
	if (result) wm_add_icon(widget, relocated_path, localized_path, NULL);

	result = wm_relocate_url(wm, widget_path, "icon.gif", relocated_path, localized_path);
	if (result) wm_add_icon(widget, relocated_path, localized_path, NULL);

	result = wm_relocate_url(wm, widget_path, "icon.jpg", relocated_path, localized_path);
	if (result) wm_add_icon(widget, relocated_path, localized_path, NULL);
}

GF_WidgetInstance *wm_load_widget(GF_WidgetManager *wm, const char *path, u32 InstanceID, Bool skip_context)
{
	char szName[GF_MAX_PATH];
	u32 i, count;
	GF_Widget *widget = NULL;
	GF_WidgetInstance *wi = NULL;
	GF_XMLNode *root, *icon, *nmain, *name, *xml_node;
	GF_Err e;
	GF_DOMParser *dom = NULL;
	GF_WidgetPackage *wpackage = NULL;

	GF_DownloadSession *sess = NULL;
	const char *widget_ns_prefix = NULL;
	const char *mpegu_ns_prefix = NULL;
	const char *user_locale = gf_opts_get_key("core", "lang");

	/* Try to see if this widget is already loaded */
	e = GF_OK;
	count = gf_list_count(wm->widgets);
	for (i=0; i<count; i++) {
		widget = gf_list_get(wm->widgets, i);
		if (!strcmp(widget->url, path)) break;
		widget = NULL;
	}

	/*not found, retrieve the widget (if http), check the package if needed and parse the configuration document/widget manifest*/
	if (!widget) {
		Bool isDownloadedPackage = 0;

		/* path used to locate the widget (config.xml if unpackaged or zip/isoff package), potentially after download */
		const char *szLocalPath = path;
		/* used to locate the config document/widget manifest potentially after unzipping */
		const char *szManifestPath = path;
		/* */
		const char *szWidgetPath = path;
		const char *desc;
		GF_WidgetPreference *pref;
		GF_List *global_prefs;
		GF_WidgetContent *content;
		Bool correct_ns = 0;

		if (strstr(path, "http://")) {
			/*fetch the remote widget manifest synchronously and load it */
			sess = gf_dm_sess_new(wm->term->downloader, (char *)path, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
			if (sess) {
				e = gf_dm_sess_process(sess);
				if (e==GF_OK) {
					szLocalPath = gf_dm_sess_get_cache_name(sess);

					if (gf_unzip_probe(szLocalPath)) {
						isDownloadedPackage = 1;
					} else {
						/* TODO ISOFF-based packaged widget */
					}
				}
			}
			if (!sess || (e!=GF_OK) || !szLocalPath) goto exit;
		}

		/* Check if the widget package is a valid package and if it contains a config.xml file */
		szManifestPath = szLocalPath;

		wpackage = widget_package_new(wm, szLocalPath);
		if (wpackage) {
			count = gf_list_count(wpackage->resources);
			for (i=0; i<count; i++) {
				GF_WidgetPackageResource *wu = gf_list_get(wpackage->resources, i);
				/* According to W3C WPC, the config.xml file (lower case) shall only be located
				   at the root of the package
				   see http://www.w3.org/TR/widgets/#ta-dxzVDWpaWg */
				if (!strcmp(wu->inner_path, "config.xml")) {
					szManifestPath = wu->extracted_path;
					break;
				}
			}
			szWidgetPath = szManifestPath;
		}


		/* Parse the Widget Config Document as a DOM */
		dom = gf_xml_dom_new();
		e = gf_xml_dom_parse(dom, szManifestPath, NULL, NULL);
		if (e) goto exit;

		root = gf_xml_dom_get_root(dom);
		if (!root) goto exit;

		correct_ns = 0;
		count = gf_list_count(root->attributes);
		for (i=0; i<count; i++) {
			GF_XMLAttribute *att = gf_list_get(root->attributes, i);
			if (att->name) {
				if (!strcmp(att->name, "xmlns")) {
					if (!strcmp(att->value, "http://www.w3.org/ns/widgets")) {
						correct_ns = 1;
					}
				} else if (!strnicmp(att->name, "xmlns:", 6)) {
					if (!strcmp(att->value, "http://www.w3.org/ns/widgets")) {
						widget_ns_prefix = att->name+6;
						correct_ns = 1;
					} else if (!strcmp(att->value, "urn:mpeg:mpegu:schema:widgets:manifest:2010")) {
						mpegu_ns_prefix = att->name+6;
					}
				}
			}
		}
		/* According to the spec, wrong or no namespace means invalid widget
		see http://www.w3.org/TR/widgets/#ta-ACCJfDGwDQ */
		if (!correct_ns) goto exit;

		/* see http://www.w3.org/TR/widgets/#ta-ACCJfDGwDQ */
		if ((root->ns && (!widget_ns_prefix || strcmp(root->ns, widget_ns_prefix) || strcmp(root->name, "widget"))) ||
		        (!root->ns && (widget_ns_prefix || strcmp(root->name, "widget"))))
			goto exit;


		/*pre-parse the root-level preference for use when parsing MPEG-U elements */
		global_prefs = gf_list_new();
		i=0;
		while ((nmain = gf_list_enum(root->content, &i))) {
			const char *pname, *pvalue;
			/* 'normalized' preference name and readonly*/
			char *npname, *npro;
			u32 i, count;
			Bool pref_exists = 0;
			Bool readOnly = 0;
			if (nmain->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(nmain->name, "preference")) continue;
			pname = wm_xml_get_attr(nmain, "name");
			npname = wm_get_single_attribute(pname);
			if (!npname || !strlen(npname)) continue;

			count = gf_list_count(global_prefs);
			for (i = 0; i < count; i++) {
				GF_WidgetPreference *tmp_pref = gf_list_get(global_prefs, i);
				if (!strcmp(tmp_pref->name, npname)) {
					pref_exists = 1;
					break;
				}
			}
			if (pref_exists) continue;

			pvalue = wm_xml_get_attr(nmain, "readonly");
			npro = wm_get_single_attribute(pvalue);
			if (npro && strlen(npro) && !strcmp(npro, "true")) readOnly=1;
			if (npro) gf_free(npro);

			pvalue = wm_xml_get_attr(nmain, "value");

			GF_SAFEALLOC(pref, GF_WidgetPreference);
			pref->name = npname;
			if (pvalue) pref->value = wm_get_single_attribute(pvalue);
			/*global preferences are save and migratable*/
			pref->flags = GF_WM_PREF_SAVE | GF_WM_PREF_MIGRATE;
			if (readOnly) pref->flags |= GF_WM_PREF_READONLY;
			gf_list_add(global_prefs, pref);
		}

		/* get the content element from the XML Config document */
		GF_SAFEALLOC(content, GF_WidgetContent);
		if (!content) {
			e = GF_OUT_OF_MEM;
			goto exit;
		}
		content->interfaces = gf_list_new();
		content->components = gf_list_new();
		content->preferences = gf_list_new();
		nmain = wm_xml_find(root, widget_ns_prefix, "content", NULL);
		if (!nmain) {
			/* if not found, use the default table of start files */
			wm_set_default_start_file(wm, content, szWidgetPath);
		} else {
			const char *src, *encoding, *mimetype;
			src = wm_xml_get_attr(nmain, "src");

			/*check the resource exists*/
			if (src) {
				Bool result;
				char relocated_path[GF_MAX_PATH], localized_path[GF_MAX_PATH];
				/*remove any existing fragment*/
				char *sep = strchr(src, '#');
				if (sep) sep[0] = 0;
				result = wm_relocate_url(wm, szWidgetPath, src, relocated_path, localized_path);
				if (sep) sep[0] = '#';
				if (result) {
					content->relocated_src = gf_strdup(relocated_path);
					content->src = gf_strdup(localized_path);
				}
			}

			encoding = wm_xml_get_attr(nmain, "encoding");
			if (encoding && strlen(encoding)) content->encoding = wm_get_single_attribute(encoding);
			else content->encoding = gf_strdup("utf-8");

			mimetype = wm_xml_get_attr(nmain, "type");
			if (mimetype && strlen(mimetype)) {
				char *sep = strchr(mimetype, ';');
				if (sep) sep[0] = 0;
				content->mimetype = wm_get_single_attribute(mimetype);
				if (sep) sep[0] = ';';
			}
			else content->mimetype = gf_strdup("text/html");

			if (!content->relocated_src) wm_set_default_start_file(wm, content, szWidgetPath);
		}
		if (strlen(content->relocated_src) == 0) {
			gf_list_del(content->interfaces);
			gf_list_del(content->components);
			gf_list_del(content->preferences);
			gf_free(content);
			content = NULL;
			goto exit;
		}
		/* We need to call the parse of the MPEG-U elements to clone the global preferences into widget preferences,
		   this should probably be changed to extract the clone from that function */
		wm_parse_mpegu_content_element(content, nmain, mpegu_ns_prefix, global_prefs);

		GF_SAFEALLOC(widget, GF_Widget);
		if (!widget) {
			e = GF_OUT_OF_MEM;
			goto exit;
		}
		widget->url = gf_strdup(path);
		widget->manifest_path = gf_strdup(szManifestPath);
		if (isDownloadedPackage) widget->local_path = gf_strdup(szLocalPath);
		widget->wm = wm;
		widget->main = content;
		content->widget = widget;
		widget->icons = gf_list_new();
		widget->features = gf_list_new();
		if (wpackage) {
			widget->wpack = wpackage;
			wpackage->widget = widget;
			/*attach downloader to our package to avoid destroying the cache file*/
			wpackage->sess = sess;
			sess = NULL;
		}

		/*check for icon - can be optional*/
		i=0;
		while ((icon = gf_list_enum(root->content, &i))) {
			if (icon->type==GF_XML_NODE_TYPE &&
			        icon->name && !strcmp(icon->name, "icon") &&
			        ((!widget_ns_prefix && !icon->ns) || !strcmp(widget_ns_prefix, icon->ns))) {
				char *sep;
				char relocated[GF_MAX_PATH], localized_path[GF_MAX_PATH];
				char *icon_width, *icon_height;
				GF_WidgetContent *iconic;
				Bool result;

				char *pname = (char *)wm_xml_get_attr(icon, "src");
				if (!pname || !strlen(pname)) continue;

				/*remove any existing fragment*/
				sep = strchr(pname, '#');
				if (sep) sep[0] = 0;
				result = wm_relocate_url(wm, szWidgetPath, pname, relocated, localized_path);
				if (sep) sep[0] = '#';
				if (!result) continue;

				iconic = wm_add_icon(widget, relocated, localized_path, sep);
				if (iconic) {
					wm_parse_mpegu_content_element(iconic, icon, mpegu_ns_prefix, global_prefs);
					icon_width = (char *)wm_xml_get_attr(icon, "width");
					if (icon_width) iconic->width = wm_parse_non_neg(icon_width);
					icon_height = (char *)wm_xml_get_attr(icon, "height");
					if (icon_height) iconic->height = wm_parse_non_neg(icon_height);
				}
			}
		}
		/* after processing the icon elements (whether there are valid icons or not), we use the default icon table
		  see http://dev.w3.org/2006/waf/widgets/test-suite/test-cases/ta-FAFYMEGELU/004/ */
		wm_set_default_icon_files(wm, szWidgetPath, widget);

		/*delete the root-level preference*/
		i=0;
		while ((pref = gf_list_enum(global_prefs , &i))) {
			if (pref->value) gf_free(pref->value);
			gf_free(pref->name);
			gf_free(pref);
		}
		gf_list_del(global_prefs);

		/*check for optional meta data*/
		name = wm_xml_find(root, widget_ns_prefix, "name", user_locale);
		if (name) {
			const char *shortname = wm_xml_get_attr(name, "short");
			widget->shortname = wm_get_single_attribute(shortname);

			widget->name = wm_get_normalized_text_content(name, NULL, user_locale);
		}

		desc = wm_xml_get_attr(root, "id");
		if (desc) {
			/* TODO check if this is a valid IRI, for the moment, just hack to pass the test, check for ':'
			  see http://dev.w3.org/2006/waf/widgets/test-suite/test-cases/ta-RawAIWHoMs/ */
			if (strchr(desc, ':'))
				widget->identifier = wm_get_single_attribute(desc);
		}

		desc = wm_xml_get_attr(root, "width");
		if (desc) {
			widget->width = wm_parse_non_neg(desc);
		}
		desc = wm_xml_get_attr(root, "height");
		if (desc) {
			widget->height = wm_parse_non_neg(desc);
		}

		name = wm_xml_find(root, widget_ns_prefix, "description", user_locale);
		if (name) {
			widget->description = wm_get_normalized_text_content(name, NULL, user_locale);
		}

		name = wm_xml_find(root, widget_ns_prefix, "license", user_locale);
		if (name) {
			const char *href = wm_xml_get_attr(name, "href");
			widget->licenseHref = wm_get_single_attribute(href);

			/* Warning the license text content should not be normalized */
			widget->license = wm_get_text_content(name, NULL, user_locale);
		}

		desc = wm_xml_get_attr(root, "version");
		if (desc) widget->version = wm_get_single_attribute(desc);

		desc = wm_xml_get_attr(root, "uuid");
		if (desc) widget->uuid = gf_strdup(desc);

		desc = wm_xml_get_attr(root, "discardable");
		if (desc) widget->discardable = !strcmp(desc, "true") ? 1 : 0;

		desc = wm_xml_get_attr(root, "multipleInstances");
		if (desc) widget->multipleInstance = !strcmp(desc, "true") ? 1 : 0;

		name = wm_xml_find(root, widget_ns_prefix, "author", NULL);
		if (name) {
			desc = wm_xml_get_attr(name, "href");
			if (desc && strchr(desc, ':')) widget->authorHref = wm_get_single_attribute(desc);

			desc = wm_xml_get_attr(name, "email");
			widget->authorEmail = wm_get_single_attribute(desc);

			widget->authorName = wm_get_normalized_text_content(name, NULL, user_locale);
		}

		i=0;
		while ((xml_node = gf_list_enum(root->content, &i))) {
			if (xml_node->type==GF_XML_NODE_TYPE &&
			        xml_node->name && !strcmp(xml_node->name, "feature") &&
			        ((!widget_ns_prefix && !xml_node->ns) || !strcmp(widget_ns_prefix, xml_node->ns))) {

				u32 i, count;
				Bool already_in = 0;
				GF_WidgetFeature *feat;
				const char *feature_name, *req_att;
				char *nfname;
				Bool required = 1;
				u32 j;
				GF_XMLNode *param_node;

				feature_name = (char *)wm_xml_get_attr(xml_node, "name");
				if (!feature_name || !strlen(feature_name) || !strchr(feature_name, ':')) continue;
				nfname = wm_get_single_attribute(feature_name);

				req_att = (char *)wm_xml_get_attr(xml_node, "required");
				if (req_att && !strcmp(req_att, "false")) required = 0;

				count = gf_list_count(widget->features);
				for (i = 0; i<count; i++) {
					GF_WidgetFeature *tmp = gf_list_get(widget->features, i);
					if (!strcmp(nfname, tmp->name)) {
						already_in = 1;
						break;
					}
				}
				if (already_in) continue;

				GF_SAFEALLOC(feat, GF_WidgetFeature);
				if (!feat) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				feat->name = nfname;
				feat->required = required;
				feat->params = gf_list_new();
				gf_list_add(widget->features, feat);

				j = 0;
				while ((param_node = gf_list_enum(xml_node->content, &j))) {
					if (param_node->type==GF_XML_NODE_TYPE &&
					        param_node->name && !strcmp(param_node->name, "param") &&
					        ((!widget_ns_prefix && !param_node->ns) || !strcmp(widget_ns_prefix, param_node->ns))) {
						GF_WidgetFeatureParam *wfp;
						const char *param_name, *param_value;
						char *npname, *npvalue;

						param_name = (char *)wm_xml_get_attr(param_node, "name");
						npname = wm_get_single_attribute(param_name);
						if (!strlen(npname)) continue;

						param_value = (char *)wm_xml_get_attr(param_node, "value");
						npvalue = wm_get_single_attribute(param_value);
						if (!strlen(npvalue)) {
							gf_free(npname);
							continue;
						}

						GF_SAFEALLOC(wfp, GF_WidgetFeatureParam);
						if (!wfp) {
							e = GF_OUT_OF_MEM;
							goto exit;
						}

						wfp->name = npname;
						wfp->value = npvalue;
						gf_list_add(feat->params, wfp);

					}
				}

			}
		}

		gf_list_add(wm->widgets, widget);
	}

	GF_SAFEALLOC(wi, GF_WidgetInstance);
	if (!wi) {
		e = GF_OUT_OF_MEM;
		goto exit;
	}
	
	wi->widget = widget;
	wi->bound_ifces = gf_list_new();
	wi->output_triggers = gf_list_new();
	wi->components = gf_list_new();
	widget->nb_instances++;
	wi->instance_id = InstanceID;
	wi->permanent = 1;

	if (!InstanceID) {
		char szInst[20];

		count = gf_list_count(wm->widget_instances);
		for (i=0; i<count; i++) {
			GF_WidgetInstance *awi = gf_list_get(wm->widget_instances, i);
			if (awi->widget == wi->widget)
				wi->instance_id = awi->instance_id;
		}
		wi->instance_id ++;

		sprintf(szName, "%s#%s#Instance%d", path, wi->widget->name, wi->instance_id);
		sprintf((char *)wi->secname, "Widget#%08X", gf_crc_32(szName, (u32) strlen(szName)));

		/*create section*/
		gf_cfg_set_key(wm->term->user->config, "Widgets", (const char *) wi->secname, " ");
		gf_cfg_set_key(wm->term->user->config, (const char *) wi->secname, "WM:Manifest", wi->widget->url);
		sprintf(szInst, "%d", wi->instance_id);
		gf_cfg_set_key(wm->term->user->config, (const char *) wi->secname, "WM:InstanceID", szInst);
	}
	gf_list_add(wm->widget_instances, wi);


	if (!skip_context && strstr(path, "http://")) {
		GF_XMLNode *context;
		GF_DownloadSession *ctx_sess = NULL;
		char *ctxPath;
		context = NULL;

		/*fetch the remote widget context synchronously and load it */
		ctxPath = gf_malloc(sizeof(char) * (strlen(path) + 1 + 15/*?mpeg-u-context*/));
		strcpy(ctxPath, path);
		if ((strchr(path, '?') == NULL) && (strstr(path, "%3f")==NULL) && (strstr(path, "%3F")==NULL) ) {
			strcat(ctxPath, "?mpeg-u-context");
		} else {
			strcat(ctxPath, "&mpeg-u-context");
		}

		/*try to fetch the associated context*/
		ctx_sess = gf_dm_sess_new(wm->term->downloader, (char *)ctxPath, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
		if (ctx_sess) {
			e = gf_dm_sess_process(ctx_sess);
			if (e==GF_OK) {
				wi->mpegu_context = gf_xml_dom_new();
				e = gf_xml_dom_parse(wi->mpegu_context , gf_dm_sess_get_cache_name(ctx_sess), NULL, NULL);
				if (!e) {
					context = gf_xml_dom_get_root(wi->mpegu_context);
					if (strcmp(context->name, "contextInformation")) context = NULL;
				}
			}
			gf_dm_sess_del(ctx_sess);
			e = GF_OK;
		}
		gf_free(ctxPath);
		ctxPath = NULL;

		if (!context && wi->mpegu_context) {
			gf_xml_dom_del(wi->mpegu_context);
			wi->mpegu_context = NULL;
		}

	}

exit:
	if (dom) gf_xml_dom_del(dom);
	if (sess) gf_dm_sess_del(sess);
	if (e || !wi) {
		if (wi) wm_delete_widget_instance(wm, wi);
		else {
			if (wpackage) widget_package_del(wm, wpackage);
		}
		return NULL;
	}
	return wi;
}


static Bool wm_enum_widget(void *cbk, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	GF_WidgetInstance *wid;
	GF_WidgetManager *wm = (GF_WidgetManager *)cbk;
	wid = wm_load_widget(wm, file_path, 0, 0);
	if (wid) {
		wm_widget_jsbind(wm, wid);
		/*remove section info*/
		gf_cfg_del_section(wm->term->user->config, (const char *) wid->secname);
		gf_cfg_set_key(wm->term->user->config, "Widgets", (const char *) wid->secname, NULL);
	}
	return 0;
}

static Bool wm_enum_dir(void *cbk, char *file_name, char *file_path, GF_FileEnumInfo *file_info)
{
	return (gf_enum_directory(file_path, 0, wm_enum_widget, cbk, "mgt")==GF_OK) ? GF_FALSE : GF_TRUE;
}


static JSBool SMJS_FUNCTION(wm_initialize)
{
	u32 i, count;
	const char*opt;
	SMJS_OBJ
	//SMJS_ARGS
	GF_WidgetManager *wm = (GF_WidgetManager *)SMJS_GET_PRIVATE(c, obj);

	count = gf_opts_get_key_count("Widgets");
	for (i=0; i<count; i++) {
		const char *name = gf_opts_get_key_name("Widgets", i);
		/*this is a previously loaded widgets - reload it*/
		if (!strnicmp(name, "Widget#", 7)) {
			const char *manifest = gf_opts_get_key(name, "WM:Manifest");
			if (manifest) {
				const char *ID = gf_opts_get_key(name, "WM:InstanceID");
				u32 instID = ID ? atoi(ID) : 0;
				GF_WidgetInstance *wi = wm_load_widget(wm, manifest, instID, 0);
				if (wi) {
					strcpy((char *)wi->secname, (const char *) name);
					wm_widget_jsbind(wm, wi);
				}
			}
		}
	}

	opt = gf_opts_get_key("Widgets", "WidgetStore");
	if (opt) gf_enum_directory(opt, 1, wm_enum_dir, wm, NULL);

	return JS_TRUE;
}

static void widgetmanager_load(GF_JSUserExtension *jsext, GF_SceneGraph *scene, JSContext *c, JSObject *global, Bool unload)
{
	GF_WidgetManager *wm;

	GF_JSAPIParam par;
	JSPropertySpec wmClassProps[] = {
		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec wmClassFuncs[] = {
		SMJS_FUNCTION_SPEC("initialize", wm_initialize, 0),
		SMJS_FUNCTION_SPEC("load", wm_load, 2),
		SMJS_FUNCTION_SPEC("unload", wm_unload, 1),
		SMJS_FUNCTION_SPEC("get", wm_get, 1),
		SMJS_FUNCTION_SPEC("findByInterface", wm_find_interface, 1),
		SMJS_FUNCTION_SPEC(0, 0, 0)
	};

	wm = jsext->udta;
	/*widget manager is only loaded once*/
	if (wm->ctx && (wm->ctx != c)) {
		/*load the 'Widget' object in the global scope*/
		widget_load(wm, scene, c, global, unload);
		return;
	}

	/*unload widgets*/
	if (unload) {
		if (wm->obj) {
			gf_js_remove_root(wm->ctx, &wm->obj, GF_JSGC_OBJECT);
			wm->obj = NULL;
		}

		while (gf_list_count(wm->widget_instances)) {
			GF_WidgetInstance *widg = gf_list_get(wm->widget_instances, 0);
			wm_delete_widget_instance(wm, widg);
		}
		wm->ctx = NULL;
		return;
	}
	wm->ctx = c;

	if (!scene) return;

	/*setup JS bindings*/
	JS_SETUP_CLASS(wm->widmanClass, "WIDGETMANAGER", JSCLASS_HAS_PRIVATE, wm_getProperty, wm_setProperty, JS_FinalizeStub);

	GF_JS_InitClass(c, global, 0, &wm->widmanClass, 0, 0, wmClassProps, wmClassFuncs, 0, 0);
	wm->obj = JS_DefineObject(c, global, "WidgetManager", &wm->widmanClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, wm->obj, wm);
	gf_js_add_root(c, &wm->obj, GF_JSGC_OBJECT);


	{
		JSPropertySpec wmWidgetClassProps[] = {
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec wmWidgetClassFuncs[] = {
			SMJS_FUNCTION_SPEC("activate", wm_widget_activate, 1),
			SMJS_FUNCTION_SPEC("deactivate", wm_widget_deactivate, 0),
			SMJS_FUNCTION_SPEC("get_interface", wm_widget_get_interface, 1),
			SMJS_FUNCTION_SPEC("bind_output_trigger", wm_widget_bind_output_trigger, 2),
			SMJS_FUNCTION_SPEC("set_input", wm_widget_set_input, 2),
			SMJS_FUNCTION_SPEC("bind_interface", wm_widget_bind_interface, 2),
			SMJS_FUNCTION_SPEC("unbind_interface", wm_widget_unbind_interface, 1),
			SMJS_FUNCTION_SPEC("call_input_action", wm_widget_call_input_action, 1),
			SMJS_FUNCTION_SPEC("call_input_script", wm_widget_call_input_script, 2),
			SMJS_FUNCTION_SPEC("is_interface_bound", wm_widget_is_interface_bound, 1),
			SMJS_FUNCTION_SPEC("get_param_value", wm_widget_get_param_value, 1),
			SMJS_FUNCTION_SPEC("get_context", wm_widget_get_context, 0),
			SMJS_FUNCTION_SPEC("get_component", wm_widget_get_component, 2),

			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		/*setup JS bindings*/
		JS_SETUP_CLASS(wm->wmWidgetClass, "WMWIDGET", JSCLASS_HAS_PRIVATE, wm_widget_getProperty, wm_widget_setProperty, JS_FinalizeStub);
		GF_JS_InitClass(c, global, 0, &wm->wmWidgetClass, 0, 0, wmWidgetClassProps, wmWidgetClassFuncs, 0, 0);

		JS_SETUP_CLASS(wm->widgetAnyClass, "WIDGETANY", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);
		GF_JS_InitClass(c, global, 0, &wm->widgetAnyClass, 0, 0, 0, 0, 0, 0);
	}

	JS_SETUP_CLASS(wm->widgetClass, "MPEGWidget", JSCLASS_HAS_PRIVATE, widget_getProperty, widget_setProperty, JS_FinalizeStub);

	if (scene->script_action) {
		if (!scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_TERM, scene->RootNode, &par))
			return;
		wm->term = par.term;
	}

}


static GF_JSUserExtension *gwm_new()
{
	GF_JSUserExtension *dr;
	GF_WidgetManager *wm;
	GF_SAFEALLOC(dr, GF_JSUserExtension);
	if (!dr) return NULL;
	GF_REGISTER_MODULE_INTERFACE(dr, GF_JS_USER_EXT_INTERFACE, "WidgetManager JavaScript Bindings", "gpac distribution");

	GF_SAFEALLOC(wm, GF_WidgetManager);
	if (!wm) {
		gf_free(dr);
		return NULL;
	}
	wm->widget_instances = gf_list_new();
	wm->widgets = gf_list_new();
	dr->load = widgetmanager_load;
	dr->udta = wm;
	return dr;
}


static void gwm_delete(GF_BaseInterface *ifce)
{
	GF_WidgetManager *wm;
	GF_JSUserExtension *dr = (GF_JSUserExtension *) ifce;
	if (!dr)
		return;
	wm = dr->udta;
	if (!wm)
		return;
	if (wm->widget_instances)
		gf_list_del(wm->widget_instances);
	wm->widget_instances = NULL;
	if (wm->widgets)
		gf_list_del(wm->widgets);
	wm->widgets = NULL;
	gf_free(wm);
	dr->udta = NULL;
	gf_free(dr);
}
#endif


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifdef GPAC_HAS_SPIDERMONKEY
		GF_JS_USER_EXT_INTERFACE,
#ifndef GPAC_DISABLE_SVG
		GF_SCENE_DECODER_INTERFACE,
#endif

#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (InterfaceType == GF_JS_USER_EXT_INTERFACE) return (GF_BaseInterface *)gwm_new();
#ifndef GPAC_DISABLE_SVG
	else if (InterfaceType == GF_SCENE_DECODER_INTERFACE) return (GF_BaseInterface *)LoadWidgetReader();
#endif
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifdef GPAC_HAS_SPIDERMONKEY
	case GF_JS_USER_EXT_INTERFACE:
		gwm_delete(ifce);
		break;
#ifndef GPAC_DISABLE_SVG
	case GF_SCENE_DECODER_INTERFACE:
		ShutdownWidgetReader(ifce);
		break;
#endif

#endif
	}
}


GPAC_MODULE_STATIC_DECLARATION( widgetman )
