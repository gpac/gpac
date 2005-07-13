#include "safe_include.h"
#include <gpac/modules/service.h>

typedef struct
{
	u32 ESID;
	LPNETCHANNEL ch;
	u32 start, end;
} V4Channel;


class V4Service {
public:

	V4Service(const char*path);
	~V4Service();

	void SetService(GF_ClientService *s) { m_pService = s; }
	GF_ClientService *GetService() { return m_pService; }

	void SetPath(char *path) { m_path = path; }
	char *GetPath() { return m_path; }

	GF_List* GetChannels() { return channels; }
	V4Channel *V4_GetChannel(V4Service *v4service, LPNETCHANNEL ch);
	Bool V4_RemoveChannel(V4Service *v4service, LPNETCHANNEL ch);

	GF_InputService *GetServiceInterface() { return m_pNetClient; }

protected:

	/* the interface given to the GPAC Core */
	GF_InputService *m_pNetClient;

	/* The actual GPAC Core service returned by the interface */
	GF_ClientService *m_pService;
	/* All the channels of the current service */
	GF_List *channels;

	/* The path to the currently load file */
	char *m_path;
};