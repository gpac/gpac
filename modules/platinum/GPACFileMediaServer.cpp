/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2009-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Platinum UPnP module 
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
 *
 *	----------------------------------------------------------------------------------
 *		PLATINUM IS LICENSED UNDER GPL or commercial agreement - cf platinum license
 *	----------------------------------------------------------------------------------
 *
 */

#include "PltUPnP.h"
#include "GPACFileMediaServer.h"
#include "PltMediaItem.h"
#include "PltService.h"
#include "PltTaskManager.h"
#include "PltHttpServer.h"
#include "PltDidl.h"
#include "PltMetadataHandler.h"
#include "PltVersion.h"

#include <gpac/network.h>

NPT_SET_LOCAL_LOGGER("gpac.media.server.file")

GPAC_FileMediaServer::GPAC_FileMediaServer(const char*  friendly_name,
                        bool         show_ip,
                        const char*  uuid,
						NPT_UInt16   port) : PLT_FileMediaServer("", friendly_name, show_ip, uuid, port) 
{
}

void GPAC_FileMediaServer::AddSharedDirectory(const char *path, const char *alias, Bool is_hidden)
{
	u8 buf[10];
	if (!alias) {
			sprintf((char*)buf, "%08X", gf_crc_32((char*) path, (u32) strlen(path)));
			alias = (const char *)buf;
	}
	m_Directories.Add(GPAC_MediaDirectory(alias, path, is_hidden));
}



NPT_Result
GPAC_FileMediaServer::OnBrowseDirectChildren(PLT_ActionReference&          action, 
												const char*                   object_id, 
												const char*                   filter,
												NPT_UInt32                    start_index,
												NPT_UInt32                    req_count,
												const char*   sort_criteria,
												const PLT_HttpRequestContext& context)
{
	/*not the root of our server*/
	if (strcmp(object_id, "/") && strcmp(object_id, "\\") && strcmp(object_id, "0")) {
		return PLT_MediaServer::OnBrowseDirectChildren(action, object_id, filter, start_index, req_count, sort_criteria, context);
	}

	unsigned long cur_index = 0;
    unsigned long num_returned = 0;
    unsigned long total_matches = 0;
    NPT_String didl = didl_header;

    PLT_MediaObjectReference item;
	for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
		GPAC_MediaDirectory *alias;
		m_Directories.Get(i, alias);
		if (alias->m_Hide) continue;

        item = BuildFromFilePath(alias->m_Path, context, true, false);

        if (!item.IsNull()) {
            if ((cur_index >= start_index) && ((num_returned < req_count) || (req_count == 0))) {
                NPT_String tmp;
                NPT_CHECK_SEVERE(PLT_Didl::ToDidl(*item.AsPointer(), filter, tmp));

                didl += tmp;
                num_returned++;
            }
            cur_index++;
            total_matches++;        
        }
    };

    didl += didl_footer;

    NPT_CHECK_SEVERE(action->SetArgumentValue("Result", didl));
    NPT_CHECK_SEVERE(action->SetArgumentValue("NumberReturned", NPT_String::FromInteger(num_returned)));
    NPT_CHECK_SEVERE(action->SetArgumentValue("TotalMatches", NPT_String::FromInteger(total_matches))); // 0 means we don't know how many we have but most browsers don't like that!!
    NPT_CHECK_SEVERE(action->SetArgumentValue("UpdateId", "1"));

    return NPT_SUCCESS;
}

NPT_Result 
GPAC_FileMediaServer::ServeFile(NPT_HttpRequest&              request, 
                                 const NPT_HttpRequestContext& context,
                                 NPT_HttpResponse&             response,
                                 const NPT_String&             _file_path)
{
    NPT_COMPILER_UNUSED(context);

	NPT_String file_path = _file_path;
    NPT_String uri_path = NPT_Uri::PercentDecode(request.GetUrl().GetPath());
	NPT_String query = request.GetUrl().GetQuery();
	if (! query.IsEmpty()) {
		uri_path += "?";
		uri_path += query;
	}

    // prevent hackers from accessing files outside of our root
    if ((file_path.Find("/..") >= 0) || (file_path.Find("\\..") >= 0)) {
        return NPT_FAILURE;
    }

	NPT_String file_id = (const char *) file_path + ((file_path[0]=='0') ? 2:1);
	for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
		GPAC_MediaDirectory *dir;
		m_Directories.Get(i, dir);
		if (file_id.StartsWith(dir->m_Alias)) {
			file_path = dir->m_Path + ((const char *) file_id + strlen(dir->m_Alias)+1);
			break;
		}
	}

	//Virtual File request
	for (u32 i=0; i<m_VirtualFiles.GetItemCount(); i++) {
		GPAC_VirtualFile vfile;
		m_VirtualFiles.Get(i, vfile);

		NPT_String path = uri_path;
		path.Replace('/', NPT_FilePath::Separator);

		if (path == vfile.m_URI) {
			NPT_Result res;
			NPT_Position start, end;
#if 0
			PLT_HttpHelper::GetRange(request, start, end);
#else
			start = end = -1;
#endif
			res = ServeVirtualFile(response, &vfile, start, end, !request.GetMethod().Compare("HEAD"));
			if (vfile.m_temporary) {
				m_VirtualFiles.Remove(vfile);
			}
			return res;
		}
	}

    // File requested
    NPT_String path = m_FileRoot;
    if (path.Compare(uri_path.Left(path.GetLength()), true) == 0) {
        NPT_Position start, end;
#if 0
        PLT_HttpHelper::GetRange(request, start, end);
#else
			start = end = -1;
#endif
        
#if 0
			return PLT_HttpServer::ServeFile(response,
                                         file_path, 
                                         start, 
                                         end, 
                                         !request.GetMethod().Compare("HEAD"));
#else
			return PLT_HttpServer::ServeFile(request, context, response, NPT_FilePath::Create(m_FileRoot, file_path) );
#endif
	} 

    
    return NPT_FAILURE;
}

NPT_Result
GPAC_FileMediaServer::GetFilePath(const char* object_id, 
                                 NPT_String& filepath) 
{
    if (!object_id) return NPT_ERROR_INVALID_PARAMETERS;

	NPT_String obj_id = object_id + (object_id[0]=='0' ? 2 : 1);
    filepath = "";
	for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
		GPAC_MediaDirectory *dir;
		m_Directories.Get(i, dir);
		if (obj_id.StartsWith(dir->m_Alias)) {
			filepath = dir->m_Path;
			object_id += (object_id[0]=='0' ? 2 : 1);
			if (!strcmp(object_id, dir->m_Alias)) {
				object_id = "";
			} else {
				object_id += strlen(dir->m_Alias) + 1;
			}
			break;
		}
	}

    if (NPT_StringLength(object_id) > 2 || object_id[0]!='0') {
        filepath += (const char*)object_id + (object_id[0]=='0'?1:0);
    }

    return NPT_SUCCESS;
}

PLT_MediaObject*
GPAC_FileMediaServer::BuildFromFilePath(const NPT_String&        filepath, 
                                       const PLT_HttpRequestContext &context,
                                       bool                     with_count,
                                       bool                     keep_extension_in_title)
{
	return BuildFromFilePathAndHost(filepath, &context, with_count, keep_extension_in_title, NULL);

}

PLT_MediaObject*
GPAC_FileMediaServer::BuildFromFilePathAndHost(const NPT_String&        __filepath, 
                                       const PLT_HttpRequestContext *context,
                                       bool                     with_count /* = true */,
                                       bool                     keep_extension_in_title /* = false */,
									   const char *host)
{
    PLT_MediaItemResource resource;
    PLT_MediaObject*      object = NULL;
	NPT_String filepath = __filepath;
	unsigned int len;

    /* retrieve the entry type (directory or file) */
    NPT_FileInfo info; 
    NPT_CHECK_LABEL_FATAL(NPT_File::GetInfo(__filepath, &info), failure);

	len = 0;
	for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
		GPAC_MediaDirectory *dir;
		m_Directories.Get(i, dir);
		if (__filepath.StartsWith(dir->m_Path) && (dir->m_Path.GetLength() > len) ) {
			char *fp = __filepath;
			filepath = NPT_FilePath::Separator + dir->m_Alias + NPT_String( fp + strlen(dir->m_Path) - 1);
			len = dir->m_Path.GetLength();
		}
	}


    if (info.m_Type == NPT_FileInfo::FILE_TYPE_REGULAR) {
		NPT_IpAddress ip;

        object = new PLT_MediaItem();

        /* Set the title using the filename for now */
        object->m_Title = NPT_FilePath::BaseName(filepath, keep_extension_in_title);
        if (object->m_Title.GetLength() == 0) goto failure;

        /* Set the protocol Info from the extension */
        resource.m_ProtocolInfo = PLT_ProtocolInfo(PLT_ProtocolInfo::GetProtocolInfo(filepath, true, context));
        if (!resource.m_ProtocolInfo.IsValid())  goto failure;

        /* Set the resource file size */
        resource.m_Size = info.m_Size;
 
        /* format the resource URI */
        NPT_String url = "0" + filepath;


		if (host) {
			ip.Parse(host);

			NPT_List<NPT_NetworkInterface*> if_list;
			NPT_List<NPT_NetworkInterface*>::Iterator net_if;
			NPT_List<NPT_NetworkInterfaceAddress>::Iterator net_if_addr;

			NPT_CHECK_LABEL_SEVERE(PLT_UPnPMessageHelper::GetNetworkInterfaces(if_list, true), failure);

			for (net_if = if_list.GetFirstItem(); net_if; net_if++) {
				if ( (*net_if)->IsAddressInNetwork(ip) ) {
					ip = (*net_if)->GetAddresses().GetFirstItem()->GetPrimaryAddress();
					break;
				}
			}

		} else if (context) {
			ip = context->GetLocalAddress().GetIpAddress();
		} else {
			// get list of ip addresses
			NPT_List<NPT_IpAddress> ips;
			NPT_CHECK_LABEL_SEVERE(PLT_UPnPMessageHelper::GetIPAddresses(ips), failure);

			// iterate through list and build list of resources
			NPT_List<NPT_IpAddress>::Iterator ipi = ips.GetFirstItem();

			ip = *ipi;
		}

#if 0
		/* Look to see if a metadatahandler exists for this extension */
        PLT_MetadataHandler* handler = NULL;
        NPT_Result res = NPT_ContainerFind(
            m_MetadataHandlers, 
            PLT_MetadataHandlerFinder(NPT_FilePath::FileExtension(filepath)), 
            handler);
        if (NPT_SUCCEEDED(res) && handler) {
            /* if it failed loading data, reset the metadatahandler so we don't use it */
            if (NPT_SUCCEEDED(handler->Load(filepath))) {
                /* replace the title with the one from the Metadata */
                NPT_String newTitle;
                if (handler->GetTitle(newTitle) != NULL) {
                    object->m_Title = newTitle;
                }

                /* assign description */
                handler->GetDescription(object->m_Description.long_description);

                /* assign album art uri if we haven't yet */
                /* prepend the album art base URI and url encode it */ 
                if (object->m_ExtraInfo.album_art_uri.GetLength() == 0) {
                    object->m_ExtraInfo.album_art_uri = 
						NPT_Uri::PercentEncode(BuildResourceUri(m_AlbumArtBaseUri, ip.ToString(), url), 
                                               NPT_Uri::UnsafeCharsToEncode);
                }

                /* duration */
                handler->GetDuration(resource.m_Duration);

                /* protection */
                handler->GetProtection(resource.m_Protection);
            }
		}
#endif
		object->m_ObjectClass.type = PLT_MediaItem::GetUPnPClass(filepath, context);

		NPT_HttpUrl base_uri(m_FileRoot);
		resource.m_Uri = BuildResourceUri(base_uri/*m_FileBaseUri*/, ip.ToString(), url);
		object->m_Resources.Add(resource);

	} else {
        object = new PLT_MediaContainer;

        /* Assign a title for this container */
        if ((filepath.Compare("/", true) == 0) || (filepath.Compare("\\", true) == 0)) {
            object->m_Title = "Root";
        } else {
			filepath.TrimRight("/\\");
            object->m_Title = NPT_FilePath::BaseName(filepath, keep_extension_in_title);
            if (object->m_Title.GetLength() == 0) goto failure;
        }

#ifndef _WIN32_WCE
		/* Get the number of children for this container */
        NPT_Cardinal count = 0;

		// reset output params
		count = 0;
        if (with_count ) {
			NPT_List<NPT_String> entries;
			NPT_File::ListDir(__filepath, entries);    
			count = entries.GetItemCount();
            ((PLT_MediaContainer*)object)->m_ChildrenCount = count;
        }
#endif	//_WIN32_WCE

        object->m_ObjectClass.type = "object.container";
    }

    /* is it the root? */
    if ((filepath.Compare("/", true) == 0) || (filepath.Compare("\\", true) == 0)) {
        object->m_ParentID = "-1";
        object->m_ObjectID = "0";
    } else {
        NPT_String directory = NPT_FilePath::DirName(filepath);
        /* is the parent path the root? */
		if ((directory.Compare("/", true) == 0) || (directory.Compare("\\", true) == 0)) {
            object->m_ParentID = "0";
        } else {
            object->m_ParentID = "0" + directory;
        }
        object->m_ObjectID = "0" + filepath;
    }
    return object;

failure:
    delete object;
    return NULL;
}


NPT_String GPAC_FileMediaServer::GetResourceURI(const char *url, const char *for_host)
{
	char *abs_url;
	NPT_String res = url, path;

	abs_url = NULL;
	for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
		GPAC_MediaDirectory *dir;
		m_Directories.Get(i, dir);
		abs_url = gf_url_concatenate(dir->m_Path, url);
		FILE *f = gf_f64_open(abs_url, "rt");
		if (f) {
			fclose(f);
			break;
		}
		gf_free((void*)abs_url);
		abs_url = NULL;
	}
	if (!abs_url ) return res;

	/*url was absolute, add its root directory*/
	if (!strcmp(abs_url, url)) {
		Bool found = GF_FALSE;
		NPT_String newdir;
		/*if the path is /my/example/path/test.ext, we want to share the parent directory
			/my/example/, otherwise we will loose the ability to browse for resource in the parent dir
		*/
		u32 nb_sep=0;
		u32 len = (u32) strlen(abs_url);
		u32 i=0;
		while (i<=len) {
			if ((abs_url[len-i]=='\\') || (abs_url[len-i]=='/') ) {
				nb_sep++;
				if (nb_sep==2) break;
			}
			i++;
		}
		/*if no parent directory, we don't allow sharing of the resource*/
		if (nb_sep!=2) return "";
		
		char sep = abs_url[len-i];
		abs_url[len-i] = 0;
		newdir = abs_url;
		abs_url[len-i] = sep;

		newdir.Replace('/', NPT_FilePath::Separator);
		newdir += NPT_FilePath::Separator;

		for (u32 i=0; i<m_Directories.GetItemCount(); i++) {
			GPAC_MediaDirectory *dir;
			m_Directories.Get(i, dir);
			if (!strcmp(newdir, dir->m_Path)) {
				found = GF_TRUE;
			}
		}
		if (!found)
			AddSharedDirectory(newdir, NULL, GF_TRUE);
	}

	path = abs_url;
	/*replace all '/' with neptune file path separator otherwise file functions get screwed up ...*/
	path.Replace('/', NPT_FilePath::Separator);

	gf_free((void*)abs_url);

	PLT_MediaObject*mobj = BuildFromFilePathAndHost(path, NULL, true, false, for_host);
	if (mobj) {
		res = mobj->m_Resources[0].m_Uri;
		delete mobj;
	}
	return res;
}

void GPAC_FileMediaServer::ShareVirtualResource(const char *res_uri, const char *res_val, const char *res_mime, Bool temporary)
{
	NPT_String the_uri;
	char *uri = (char *)res_uri;
	char *sep = (char *) strstr(res_uri, "://");
	if (sep) {
		sep = strchr(sep+3, '/');
		if (sep) uri = sep;
		else uri = (char*)"/";
	}
	the_uri = "";
	while (1) {
		sep = strchr(uri, '%');
		if (!sep) break;

		if (!strnicmp(sep, "%3f", 3) || !strnicmp(sep, "%5c", 3) || !strnicmp(sep, "%2f", 3)) {
			sep[0]=0;
			the_uri += uri;
			sep[0]='%';

			if (!strnicmp(sep, "%3f", 3))
				the_uri += "?";
			else if (!strnicmp(sep, "%5c", 3))
				the_uri += "/";
			else 
				the_uri += " ";
			uri= sep+3;
			continue;
		} else {
			the_uri += "%";
			uri= sep+1;
		}
	}
	the_uri += uri;
	the_uri.Replace('/', NPT_FilePath::Separator);

	GPAC_VirtualFile vres(the_uri, res_val, res_mime, temporary);

	m_VirtualFiles.Add(vres);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[UPnP] sharing virtual file %s as %s\n", res_uri, (const char *)the_uri));
}

NPT_Result 
GPAC_FileMediaServer::ServeVirtualFile(NPT_HttpResponse& response,
                          GPAC_VirtualFile  *vfile, 
                          NPT_Position      start,
                          NPT_Position      end,
                          bool              request_is_head) 
{
    NPT_LargeSize            total_len;
    NPT_Result               result;

	NPT_MemoryStream* memory_stream = new NPT_MemoryStream(vfile->m_Content.GetChars(), vfile->m_Content.GetLength() );
    NPT_InputStreamReference stream(memory_stream);

	if (NPT_FAILED(result = stream->GetSize(total_len)) ) {
        response.SetStatus(404, "File Not Found");
        return NPT_SUCCESS;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[UPnP] Serving virtual file %s\n", vfile->m_Content.GetChars()));

	NPT_HttpEntity* entity = new NPT_HttpEntity();
	entity->SetContentLength(total_len);
	response.SetEntity(entity);
	entity->SetContentType(vfile->m_MIME);

    // request is HEAD, returns without setting a body
    if (request_is_head) return NPT_SUCCESS;

    // see if it was a byte range request
    if (start != (NPT_Position)-1 || end != (NPT_Position)-1) {
        // we can only support a range from an offset to the end of the resource for now
        // due to the fact we can't limit how much to read from a stream yet
        NPT_Position start_offset = (NPT_Position)-1, end_offset = total_len - 1, len;
        if (start == (NPT_Position)-1 && end != (NPT_Position)-1) {
            // we are asked for the last N=end bytes
            // adjust according to total length
            if (end >= total_len) {
                start_offset = 0;
            } else {
                start_offset = total_len-end;
            }
        } else if (start != (NPT_Position)-1) {
            start_offset = start;
            // if the end is specified but incorrect
            // set the end_offset in order to generate a bad response
            if (end != (NPT_Position)-1 && end < start) {
                end_offset = (NPT_Position)-1;
            }
        }

        // in case the range request was invalid or we can't seek then respond appropriately
        if (start_offset == (NPT_Position)-1 || end_offset == (NPT_Position)-1 || 
            start_offset > end_offset || NPT_FAILED(stream->Seek(start_offset))) {
            response.SetStatus(416, "Requested range not satisfiable");
        } else {
            len = end_offset - start_offset + 1;
            response.SetStatus(206, "Partial Content");
            entity->SetInputStream(stream);
            entity->SetContentLength(len);
        }
    } else {
        entity->SetInputStream(stream);
    }
    return NPT_SUCCESS;
}

