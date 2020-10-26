
/*dummy rate adaptation for DASH client */

import { Sys as sys } from 'gpaccore'

_gpac_log_name="";

print("Hello DASH custom algo !");

let groups = [];

dashin.rate_adaptation = function (group_idx, base_group_idx, force_lower_complexity, stats)
{
	print(`Getting called in custom algo ! group ${group_idx} base_group ${base_group_idx} force_lower_complexity ${force_lower_complexity}`);
	print('Stats: ' + JSON.stringify(stats));

	//always use lowest quality
	return 0;
}


dashin.new_group = function (group)
{
	print("New group: " + JSON.stringify(group));
	//remember the group (adaptation set in dash)
	groups.push(group);
}

dashin.period_reset = function (reset_type)
{
	print("Period reset type " + reset_type);
	if (!reset_type)
		groups = [];
}

dashin.download_monitor = function (group_idx, stats)
{
	print("Download info " + JSON.stringify(stats));
	return -1;
}
