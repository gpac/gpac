
/*dummy rate adaptation for DASH client */

import { Sys as sys } from 'gpaccore'

_gpac_log_name="";

print("Hello DASH custom algo !");

let groups = [];

function get_group(group_idx) 
{
	for (let i=0; i < groups.length; i++) {
		if (groups[i].idx == group_idx)
			return groups[i];
	}

	return null;
}

let nb_adapt=0;

dashin.rate_adaptation = function (group_idx, base_group_idx, force_lower_complexity, stats)
{
//	print(`Getting called in custom algo ! group ${group_idx} base_group ${base_group_idx} force_lower_complexity ${force_lower_complexity}`);
//	print('Stats: ' + JSON.stringify(stats));

	let g = get_group(group_idx);
	if (!g) return -1;

	if (g.SRD && !g.SRD.w) {
		nb_adapt++;
		print('\n\nnb_adapt ' + nb_adapt);
	}
	else if (g.SRD && g.SRD.w) {
		//if (!g.SRD.x || !g.SRD.y) {
		if (stats.degradation_hint) {
			print('disabling hidden tile ' + g.SRD.x + 'x' + g.SRD.y);
			return -2;
		}
		print('use high quality for tile ' + g.SRD.x + 'x' + g.SRD.y);
		return 1;
	}
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
