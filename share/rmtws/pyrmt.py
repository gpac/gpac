import libgpac as gpac
import sys


print("Welcome to GPAC Python !\nVersion: " + gpac.version + "\n" + gpac.copyright_cite)



gpac.init(1, b"0")

gpac_args = []
FILTERS = []

for arg in sys.argv:
    if arg.startswith("-f="):
        FILTERS.append(arg[3:])
    else:
        gpac_args.append(arg)

gpac.set_args(gpac_args)

fid = 0

#define a custom filter session
class MyFilterSession(gpac.FilterSession):

    def on_filter_new(self, f):
        print("new filter " + f.name)
        global fid
        fid+=1
        f.idx = fid

    def on_filter_del(self, f):
        print("del filter " + f.name)

fs = MyFilterSession()

for filter in FILTERS:
    print("loading filter ", filter)
    fs.load(filter)

fs.reporting(True)



###############

gpac.enable_rmtws()

import pprint
import json

class MyRMTHandler():

    def __init__(self):
        self.clients = []

    def on_new_client(self, client):
        print(f"new client {client} {client.peer_address()}")
        self.clients.append(client)
        pprint.pprint(self.clients)

    def on_client_close(self, client):
        print(f"del client {client} {client.peer_address()}")
        self.clients.remove(client)
        pprint.pprint(self.clients)

    def on_client_data(self, client, data):
        print(f"client {client.peer_address()} got data: {data}")
        pprint.pprint(self.clients)

        if (type(data) == str and data.startswith("json:")):
            jtext = json.loads(data[5:])
            pprint.pprint(jtext)

            if 'message' in jtext and jtext['message'] == "get_all_filters":

                all_filters = get_all_filters()
                client.send(json.dumps({'message': 'filters', 'filters': all_filters}))

                fs.post(UpdateTask(client, full=True))


filter_props_lite = ['name', 'status', 'bytes_done', 'type', 'ID', 'nb_ipid', 'nb_opid', 'idx', 'itag']

def filter_to_dict(f, full=False):
    jsf = {}

    for prop in filter_props_lite:
        if hasattr(f, prop):
            jsf[prop] = getattr(f, prop)

    jsf["gpac_args"] = []

    if full:
        jsf["gpac_args"] = f.all_args_value()


    jsf['ipid'] = {}
    jsf['opid'] = {}

    for i in range(f.nb_ipid):

        pidname = f.ipid_prop(i, "name")

        pid_enum = PropsEnum(full)
        f.ipid_enum_props(i, pid_enum)
        pid_obj = pid_enum.pid_props

        pid_obj['buffer'] = f.ipid_prop(i, "buffer")
        pid_obj['eos'] = f.ipid_prop(i, "eos")

        pid_obj['source_idx'] = f.ipid_source(i).idx

        jsf['ipid'][pidname] = pid_obj


    for i in range(f.nb_opid):

        pidname = f.opid_prop(i, "name")

        pid_enum = PropsEnum(full)
        f.opid_enum_props(i, pid_enum)
        pid_obj = pid_enum.pid_props

        pid_obj['buffer'] = f.opid_prop(i, "buffer")

        pid_obj['dest_idx'] = [fsink.idx for fsink in f.opid_sinks(i)]

        jsf['opid'][pidname] = pid_obj


    return jsf


class PropsEnum:
    def __init__(self, full=False, pid_props_lite=[]):
        self.pid_props = {}
        self.full = full
        self.pid_props_lite = []

    def on_prop_enum(self, pname, pval):
        if self.full or pname in self.pid_props_lite:
            self.pid_props[pname] = str(pval)


def get_all_filters(full=False):
    all_filters = []
    for i in range(fs.nb_filters):
        f = fs.get_filter(i)
        all_filters.append(filter_to_dict(f, full))
    return all_filters


class UpdateTask(gpac.FilterTask):

    def __init__(self, client, full=False):
        super().__init__("updatefilters")
        self.client = client
        self.full = full

    def execute(self):

        all_filters = get_all_filters(self.full)
        if self.client:
            self.client.send(json.dumps({'message': 'update', 'filters': all_filters}))
            return 1000
        else:
            return None



rmt_handler = MyRMTHandler()

gpac.set_rmt_handler(rmt_handler)
##############




#run the session in blocking mode
fs.run()

fs.print_stats()
fs.print_graph()

fs.delete()
gpac.close()
