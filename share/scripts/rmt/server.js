// server/server.js
import { Sys as sys5 } from "gpaccore";

// server/JSClient/config.js
var DEFAULT_FILTER_FIELDS = [
  "idx",
  "bytes_done",
  "bytes_sent",
  "pck_sent",
  "pck_done",
  "time",
  "nb_ipid",
  "nb_opid",
  "errors",
  "current_errors"
];
var CPU_STATS_FIELDS = [
  "total_cpu_usage",
  "process_cpu_usage",
  "process_memory",
  "physical_memory",
  "physical_memory_avail",
  "gpac_memory",
  "thread_count"
];
var FILTER_PROPS_LITE = [
  "name",
  "status",
  "bytes_done",
  "type",
  "ID",
  "nb_ipid",
  "nb_opid",
  "idx",
  "itag",
  "pck_sent",
  "pck_done",
  "time",
  "current_errors"
];
var FILTER_ARGS_LITE = [];
var PID_PROPS_LITE = [];
var FILTER_SUBSCRIPTION_FIELDS = [
  "status",
  "bytes_done",
  "bytes_sent",
  "pck_done",
  "pck_sent",
  "time",
  "nb_ipid",
  "nb_opid",
  "errors",
  "current_errors"
];
var UPDATE_INTERVALS = {
  SESSION_STATS: 1e3,
  FILTER_STATS: 1e3,
  CPU_STATS: 500
};
var LOG_RETENTION = {
  maxHistorySize: 500,
  maxHistorySizeVerbose: 2e3,
  keepRatio: {
    error: 1,
    warning: 0.8,
    info: 0.2,
    debug: 0.05
  }
};

// server/JSClient/Cache/CacheManager.js
function CacheManager() {
  this.cache = /* @__PURE__ */ Object.create(null);
  this.hits = 0;
  this.misses = 0;
  this.get = function(key, maxAgeMs) {
    const entry = this.cache[key];
    if (!entry) {
      this.misses++;
      return null;
    }
    const now = Date.now();
    if (now - entry.ts > maxAgeMs) {
      delete this.cache[key];
      this.misses++;
      return null;
    }
    this.hits++;
    return entry.data;
  };
  this.set = function(key, data) {
    this.cache[key] = { data, ts: Date.now() };
  };
  this.getOrSet = function(key, ttlMs, computeStringFn) {
    const cached = this.get(key, ttlMs);
    if (cached) return cached;
    const value = computeStringFn();
    this.set(key, value);
    return value;
  };
  this.stats = function() {
    return {
      size: Object.keys(this.cache).length,
      hits: this.hits,
      misses: this.misses,
      keys: Object.keys(this.cache)
    };
  };
  this.clear = function(key) {
    if (key) {
      delete this.cache[key];
    } else {
      this.cache = /* @__PURE__ */ Object.create(null);
    }
  };
}
var cacheManager = new CacheManager();

// server/JSClient/Messaging/MessageHandler.js
function MessageHandler(client) {
  this.client = client;
  this.handleMessage = function(msg, all_clients2) {
    let text = msg;
    if (text.startsWith("json:")) {
      try {
        let jtext = JSON.parse(text.substr(5));
        if (!("message" in jtext)) return;
        const handlers = {
          "get_all_filters": () => {
            this.client.filterManager.sendAllFilters();
          },
          "filter_args_details": () => {
            let idx = jtext["idx"];
            this.client.filterManager.requestDetails(idx);
          },
          "stop_filter_args": () => {
            let idx = jtext["idx"];
            this.client.filterManager.stopDetails(idx);
          },
          "subscribe_session": () => {
            const interval = jtext["interval"] || UPDATE_INTERVALS.SESSION_STATS;
            const fields = jtext["fields"] || DEFAULT_FILTER_FIELDS;
            this.client.sessionStatsManager.subscribe(interval, fields);
            this.client.sessionManager.startMonitoringLoop();
          },
          "unsubscribe_session": () => {
            this.client.sessionStatsManager.unsubscribe();
          },
          "subscribe_filter": () => {
            const idx = jtext.idx;
            let interval = jtext.interval || UPDATE_INTERVALS.FILTER_STATS;
            let pidScope = jtext.pidScope || "both";
            if (!pidScope) {
              pidScope = "both";
            }
            this.client.filterManager.subscribeToFilter(idx, interval, pidScope);
            this.client.sessionManager.startMonitoringLoop();
          },
          "unsubscribe_filter": () => {
            const idx = jtext.idx;
            this.client.filterManager.unsubscribeFromFilter(idx);
          },
          "update_arg": () => {
            this.client.filterManager.updateArgument(jtext["idx"], jtext["name"], jtext["argName"], jtext["newValue"]);
          },
          "get_png": () => {
            this.client.filterManager.addPngProbe(jtext["idx"], jtext["name"]);
          },
          "subscribe_cpu_stats": () => {
            const interval = jtext["interval"] || UPDATE_INTERVALS.CPU_STATS;
            const fields = jtext["fields"] || [];
            this.client.cpuStatsManager.subscribe(interval, fields);
            this.client.sessionManager.startMonitoringLoop();
          },
          "unsubscribe_cpu_stats": () => {
            this.client.cpuStatsManager.unsubscribe();
          },
          "subscribe_logs": () => {
            const logLevel = jtext["logLevel"] || "all@warning";
            this.client.logManager.subscribe(logLevel);
            this.client.sessionManager.startMonitoringLoop();
          },
          "unsubscribe_logs": () => {
            this.client.logManager.unsubscribe();
          },
          "update_log_level": () => {
            const logLevel = jtext["logLevel"] || "all@warning";
            this.client.logManager.updateLogLevel(logLevel);
          },
          "get_log_status": () => {
            const status = this.client.logManager.getStatus();
            this.client.client.send(JSON.stringify({
              message: "log_status",
              status
            }));
          },
          "get_ipid_props": () => {
            const props = this.client.pidPropsCollector.collectIpidProps(
              jtext["filterIdx"],
              jtext["ipidIdx"]
            );
            this.client.client.send(JSON.stringify({
              message: "ipid_props_response",
              filterIdx: jtext["filterIdx"],
              ipidIdx: jtext["ipidIdx"],
              properties: props
            }));
          },
          "get_command_line": () => {
            this.client.commandLineManager.sendCommandLine();
          },
          "get_cache_stats": () => {
            const stats = cacheManager.stats();
            this.client.client.send(JSON.stringify({
              message: "cache_stats",
              stats
            }));
          }
        };
        const handler = handlers[jtext["message"]];
        if (handler) {
          handler();
        }
      } catch (e) {
        console.error("MessageHandler: Error handling message:", e);
      }
    }
  };
}

// server/JSClient/Session/SessionStatsManager.js
function SessionStatsManager(client) {
  this.client = client;
  this.isSubscribed = false;
  this.interval = UPDATE_INTERVALS.SESSION_STATS;
  this.fields = [];
  this.subscribe = function(interval, fields) {
    this.isSubscribed = true;
    this.interval = interval || UPDATE_INTERVALS.SESSION_STATS;
    this.fields = fields || DEFAULT_FILTER_FIELDS;
  };
  this.unsubscribe = function() {
    this.isSubscribed = false;
  };
  this.computeAllPacketsDone = function(filters) {
    if (filters.length === 0) return false;
    for (const f of filters) {
      if (f.nb_ipid === 0) continue;
      for (let i = 0; i < f.nb_ipid; i++) {
        const eos = f.ipid_props(i, "eos");
        if (!eos) {
          return false;
        }
      }
    }
    return true;
  };
  this.tick = function(now) {
    if (!this.isSubscribed) return;
    const serialized = cacheManager.getOrSet("session_stats", 50, () => {
      const stats = [];
      const filters = [];
      session.lock_filters(true);
      for (let i = 0; i < session.nb_filters; i++) {
        const f = session.get_filter(i);
        if (f.is_destroyed()) continue;
        filters.push(f);
        const obj = {};
        for (const field of this.fields) {
          obj[field] = f[field];
        }
        let allInputsEos = f.nb_ipid > 0;
        for (let j = 0; j < f.nb_ipid; j++) {
          if (!f.ipid_props(j, "eos")) {
            allInputsEos = false;
            break;
          }
        }
        obj.is_eos = allInputsEos;
        obj.last_ts_sent = f.last_ts_sent || null;
        stats.push(obj);
      }
      const allFiltersEos = this.computeAllPacketsDone(filters);
      const all_packets_done = session.last_task && allFiltersEos;
      session.lock_filters(false);
      return JSON.stringify({
        message: "session_stats",
        all_packets_done,
        stats
      });
    });
    if (this.client.client) {
      this.client.client.send(serialized);
    }
  };
  this.handleSessionEnd = function() {
    this.unsubscribe();
  };
}

// server/JSClient/Session/SessionManager.js
import { Sys as sys } from "gpaccore";
function SessionManager(client) {
  this.client = client;
  this.isMonitoringLoopRunning = false;
  this.hasActiveSubscriptions = function() {
    return this.client.sessionStatsManager.isSubscribed || this.client.cpuStatsManager.isSubscribed || this.client.logManager.isSubscribed || Object.keys(this.client.filterManager.filterSubscriptions).length > 0;
  };
  this.startMonitoringLoop = function() {
    if (this.isMonitoringLoopRunning) return;
    this.isMonitoringLoopRunning = true;
    session.post_task(() => {
      const now = sys.clock_us();
      if (session.last_task) {
        this.client.cpuStatsManager.tick(now);
        this.client.logManager.tick(now);
        this.client.filterManager.tick(now);
        this.client.sessionStatsManager.tick(now);
        try {
          this.client.client.send(JSON.stringify({
            message: "session_end",
            reason: "completed",
            timestamp: now
          }));
        } catch (e) {
          print("[SessionManager] Error sending session_end message:", e);
        }
        this.client.cpuStatsManager.handleSessionEnd();
        this.client.logManager.handleSessionEnd();
        this.client.filterManager.handleSessionEnd();
        this.client.sessionStatsManager.handleSessionEnd();
        this.isMonitoringLoopRunning = false;
        return false;
      }
      this.client.cpuStatsManager.tick(now);
      this.client.logManager.tick(now);
      this.client.filterManager.tick(now);
      this.client.sessionStatsManager.tick(now);
      const shouldContinue = this.hasActiveSubscriptions();
      if (!shouldContinue) this.isMonitoringLoopRunning = false;
      let interval = 1e3;
      if (this.client.sessionStatsManager.isSubscribed) {
        interval = Math.min(interval, this.client.sessionStatsManager.interval);
      }
      if (this.client.cpuStatsManager.isSubscribed) {
        interval = Math.min(interval, this.client.cpuStatsManager.interval);
      }
      return shouldContinue ? interval : false;
    });
  };
}

// server/JSClient/filterUtils.js
function gpac_filter_to_object(f, full = false) {
  let jsf = {};
  for (let prop in f) {
    if (full || FILTER_PROPS_LITE.includes(prop))
      jsf[prop] = f[prop];
  }
  jsf["gpac_args"] = [];
  if (full) {
    let all_args = f.all_args(true);
    for (let arg of all_args) {
      if (arg && (full || FILTER_ARGS_LITE.includes(arg.name)))
        jsf["gpac_args"].push(arg);
    }
  }
  jsf["ipid"] = {};
  jsf["opid"] = {};
  for (let d = 0; d < f.nb_ipid; d++) {
    let pidname = f.ipid_props(d, "name");
    let jspid = {};
    f.ipid_props(d, (name, type, val) => {
      if (full || PID_PROPS_LITE.includes(name))
        jspid[name] = { "type": type, "val": val };
    });
    jspid["buffer"] = f.ipid_props(d, "buffer");
    jspid["buffer_total"] = f.ipid_props(d, "buffer_total");
    jspid["source_idx"] = f.ipid_source(d).idx;
    jsf["ipid"][pidname] = jspid;
  }
  for (let d = 0; d < f.nb_opid; d++) {
    let pidname = f.opid_props(d, "name");
    let jspid = {};
    f.opid_props(d, (name, type, val) => {
      if (full || PID_PROPS_LITE.includes(name))
        jspid[name] = { "type": type, "val": val };
    });
    jspid["buffer"] = f.opid_props(d, "buffer");
    jspid["buffer_total"] = f.opid_props(d, "buffer_total");
    jsf["opid"][pidname] = jspid;
  }
  return jsf;
}
function gpac_filter_to_minimal_object(f) {
  const minimalFilters = {
    idx: f.idx,
    name: f.name,
    type: f.type,
    status: f.status,
    itag: f.itag || null,
    ID: f.ID || null,
    nb_ipid: f.nb_ipid,
    nb_opid: f.nb_opid,
    ipid: {},
    opid: {}
  };
  for (let i = 0; i < f.nb_ipid; i++) {
    const pidName = f.ipid_props(i, "name");
    const streamType = f.ipid_props(i, "StreamType");
    minimalFilters.ipid[pidName] = {
      source_idx: f.ipid_source(i).idx,
      stream_type: streamType
    };
  }
  for (let o = 0; o < f.nb_opid; o++) {
    const pidName = f.opid_props(o, "name");
    const streamType = f.opid_props(o, "StreamType");
    minimalFilters.opid[pidName] = {
      stream_type: streamType
    };
  }
  return minimalFilters;
}
function on_all_connected(cb, draned_once_ref) {
  session.post_task(() => {
    let local_connected = true;
    let all_filters_instances = [];
    session.lock_filters(true);
    for (let i = 0; i < session.nb_filters; i++) {
      const f = session.get_filter(i);
      if (f.is_destroyed()) continue;
      all_filters_instances.push(f);
    }
    session.lock_filters(false);
    if (local_connected) {
      cb(all_filters_instances);
      if (draned_once_ref) draned_once_ref.value = true;
      return false;
    }
    return 2e3;
  });
}

// server/JSClient/Filters/PID/PidDataCollector.js
function PidDataCollector() {
  this.collectInputPids = function(filter) {
    const ipids = {};
    for (let i = 0; i < filter.nb_ipid; i++) {
      const pid = {};
      const originalName = filter.ipid_props(i, "name");
      pid.name = filter.nb_ipid > 1 && originalName ? `${originalName}_${i}` : originalName;
      pid.buffer = filter.ipid_props(i, "buffer");
      pid.nb_pck_queued = filter.ipid_props(i, "nb_pck_queued");
      pid.would_block = filter.ipid_props(i, "would_block");
      pid.eos = filter.ipid_props(i, "eos");
      pid.playing = filter.ipid_props(i, "playing");
      pid.timescale = filter.ipid_props(i, "Timescale");
      pid.codec = filter.ipid_props(i, "CodecID");
      pid.type = filter.ipid_props(i, "StreamType");
      pid.width = filter.ipid_props(i, "Width");
      pid.height = filter.ipid_props(i, "Height");
      pid.pixelformat = filter.ipid_props(i, "PixelFormat");
      pid.bitrate = filter.ipid_props(i, "Bitrate");
      pid.samplerate = filter.ipid_props(i, "SampleRate");
      pid.channels = filter.ipid_props(i, "Channels");
      const source = filter.ipid_source(i);
      if (source) {
        pid.source_idx = source.idx;
      }
      const stats = filter.ipid_stats(i);
      if (stats) {
        pid.stats = {};
        pid.stats.disconnected = stats.disconnected;
        pid.stats.average_process_rate = stats.average_process_rate;
        pid.stats.max_process_rate = stats.max_process_rate;
        pid.stats.average_bitrate = stats.average_bitrate;
        pid.stats.max_bitrate = stats.max_bitrate;
        pid.stats.nb_processed = stats.nb_processed;
        pid.stats.max_process_time = stats.max_process_time;
        pid.stats.total_process_time = stats.total_process_time;
      }
      const key = pid.name || `ipid_${i}`;
      ipids[key] = pid;
    }
    return ipids;
  };
  this.collectOutputPids = function(filter) {
    const opids = {};
    for (let i = 0; i < filter.nb_opid; i++) {
      const pid = {};
      const originalName = filter.opid_props(i, "name");
      pid.name = filter.nb_opid > 1 && originalName ? `${originalName}_${i}` : originalName;
      pid.buffer = filter.opid_props(i, "buffer");
      pid.max_buffer = filter.opid_props(i, "max_buffer");
      pid.nb_pck_queued = filter.opid_props(i, "nb_pck_queued");
      pid.would_block = filter.opid_props(i, "would_block");
      const statsEos = filter.opid_stats(i);
      pid.eos_received = statsEos?.eos_received;
      pid.playing = filter.opid_props(i, "playing");
      pid.timescale = filter.opid_props(i, "Timescale");
      pid.codec = filter.opid_props(i, "CodecID");
      pid.type = filter.opid_props(i, "StreamType");
      pid.width = filter.opid_props(i, "Width");
      pid.height = filter.opid_props(i, "Height");
      pid.pixelformat = filter.opid_props(i, "PixelFormat");
      pid.samplerate = filter.opid_props(i, "SampleRate");
      pid.channels = filter.opid_props(i, "Channels");
      pid.id = filter.opid_props(i, "ID");
      pid.trackNumber = filter.opid_props(i, "TrackNumber");
      pid.serviceID = filter.opid_props(i, "ServiceID");
      pid.language = filter.opid_props(i, "Language");
      pid.role = filter.opid_props(i, "Role");
      const stats = filter.opid_stats(i);
      if (stats) {
        pid.stats = {};
        pid.stats.disconnected = stats.disconnected;
        pid.stats.average_process_rate = stats.average_process_rate;
        pid.stats.max_process_rate = stats.max_process_rate;
        pid.stats.average_bitrate = stats.average_bitrate;
        pid.stats.max_bitrate = stats.max_bitrate;
        pid.stats.nb_processed = stats.nb_processed;
        pid.stats.max_process_time = stats.max_process_time;
        pid.stats.total_process_time = stats.total_process_time;
        pid.stats.last_ts_sent = stats.last_ts_sent;
        pid.stats.first_process_time = stats.first_process_time;
      }
      const key = pid.name || `opid_${i}`;
      opids[key] = pid;
    }
    return opids;
  };
}

// server/JSClient/Filters/ArgumentHandler.js
function ArgumentHandler(client) {
  this.client = client;
  this.sendDetails = function(idx) {
    session.post_task(() => {
      let Args = [];
      session.lock_filters(true);
      for (let i = 0; i < session.nb_filters; i++) {
        let f = session.get_filter(i);
        if (f.idx == idx) {
          const fullObj = gpac_filter_to_object(f, true);
          Args = fullObj.gpac_args;
          break;
        }
      }
      session.lock_filters(false);
      if (this.client.client) {
        this.client.client.send(JSON.stringify({
          message: "details",
          filter: {
            idx,
            gpac_args: Args
          }
        }));
      }
      return false;
    });
  };
  this.updateArgument = function(idx, name, argName, newValue) {
    let filter = session.get_filter("" + idx);
    if (!filter) {
      print("Error: Filter with idx " + idx + " not found");
      return;
    }
    try {
      filter.update(argName, newValue);
    } catch (e) {
      print("Error: Failed to update argument: " + e.toString());
    }
  };
}

// server/JSClient/Filters/FilterManager.js
function FilterManager(client, draned_once_ref) {
  this.client = client;
  this.draned_once_ref = draned_once_ref;
  this.details_needed = {};
  this.filterSubscriptions = {};
  this.lastSentByFilter = {};
  this.pidDataCollector = new PidDataCollector();
  this.argumentHandler = new ArgumentHandler(client);
  this.sendAllFilters = function() {
    on_all_connected((all_js_filters) => {
      const serialized = cacheManager.getOrSet("all_filters", 100, () => {
        const minimalFiltersList = all_js_filters.map((f) => {
          return gpac_filter_to_minimal_object(f);
        });
        return JSON.stringify({
          "message": "filters",
          "filters": minimalFiltersList
        });
      });
      if (this.client.client) {
        this.client.client.send(serialized);
      }
      session.post_task(() => {
        let js_filters = [];
        session.lock_filters(true);
        for (let i = 0; i < session.nb_filters; i++) {
          let f = session.get_filter(i);
          js_filters.push(gpac_filter_to_object(f));
        }
        session.lock_filters(false);
        return false;
      });
    }, this.draned_once_ref);
  };
  this.requestDetails = function(idx) {
    this.details_needed[idx] = true;
    this.argumentHandler.sendDetails(idx);
  };
  this.stopDetails = function(idx) {
    this.details_needed[idx] = false;
  };
  this.subscribeToFilter = function(idx, interval, pidScope) {
    this.filterSubscriptions[idx] = {
      interval: interval || UPDATE_INTERVALS.FILTER_STATS,
      fields: FILTER_SUBSCRIPTION_FIELDS,
      pidScope: pidScope || "both"
    };
    this.lastSentByFilter[idx] = 0;
    this.client.sessionManager.startMonitoringLoop();
  };
  this.unsubscribeFromFilter = function(idx) {
    delete this.filterSubscriptions[idx];
    delete this.lastSentByFilter[idx];
  };
  this.tick = function(now) {
    for (const idxStr in this.filterSubscriptions) {
      const idx = parseInt(idxStr);
      const sub = this.filterSubscriptions[idxStr];
      const lastSent = this.lastSentByFilter[idxStr] || 0;
      if (now - lastSent < sub.interval) continue;
      const cacheKey = `filter_stats_${idx}`;
      const serialized = cacheManager.getOrSet(cacheKey, 50, () => {
        session.lock_filters(true);
        let fObj = null;
        for (let i = 0; i < session.nb_filters; i++) {
          const f = session.get_filter(i);
          if (f.is_destroyed()) continue;
          if (f.idx === idx) {
            fObj = f;
            break;
          }
        }
        session.lock_filters(false);
        if (!fObj) return null;
        const payload = { idx };
        for (const field of sub.fields) {
          payload[field] = fObj[field];
        }
        switch (sub.pidScope) {
          case "ipid":
            payload.ipids = this.pidDataCollector.collectInputPids(fObj);
            break;
          case "opid":
            payload.opids = this.pidDataCollector.collectOutputPids(fObj);
            break;
          case "both":
            payload.ipids = this.pidDataCollector.collectInputPids(fObj);
            payload.opids = this.pidDataCollector.collectOutputPids(fObj);
            break;
          default:
            break;
        }
        return JSON.stringify({
          message: "filter_stats",
          ...payload
        });
      });
      if (serialized && this.client.client) {
        this.client.client.send(serialized);
        this.lastSentByFilter[idxStr] = now;
      }
    }
  };
  this.handleSessionEnd = function() {
    this.filterSubscriptions = {};
    this.lastSentByFilter = {};
  };
  this.updateArgument = function(idx, name, argName, newValue) {
    this.argumentHandler.updateArgument(idx, name, argName, newValue);
  };
}

// server/JSClient/Sys/CpuStatsManager.js
import { Sys as sys2 } from "gpaccore";
function CpuStatsManager(client) {
  this.client = client;
  this.isSubscribed = false;
  this.interval = UPDATE_INTERVALS.CPU_STATS;
  this.fields = CPU_STATS_FIELDS;
  this.lastSent = 0;
  this.subscribe = function(interval, fields) {
    this.isSubscribed = true;
    this.interval = interval || UPDATE_INTERVALS.CPU_STATS;
    this.fields = fields || CPU_STATS_FIELDS;
    this.lastSent = 0;
    this.client.sessionManager.startMonitoringLoop();
  };
  this.unsubscribe = function() {
    this.isSubscribed = false;
  };
  this.tick = function(now) {
    if (!this.isSubscribed) return;
    if (now - this.lastSent < this.interval) return;
    const serialized = cacheManager.getOrSet("cpu_stats", 50, () => {
      const cpuStats = {
        timestamp: now,
        total_cpu_usage: sys2.total_cpu_usage,
        process_cpu_usage: sys2.process_cpu_usage,
        process_memory: sys2.process_memory,
        physical_memory: sys2.physical_memory,
        physical_memory_avail: sys2.physical_memory_avail,
        gpac_memory: sys2.gpac_memory,
        nb_cores: sys2.nb_cores,
        thread_count: sys2.thread_count,
        memory_usage_percent: 0,
        process_memory_percent: 0,
        gpac_memory_percent: 0,
        cpu_efficiency: 0
      };
      if (sys2.physical_memory > 0) {
        cpuStats.memory_usage_percent = (sys2.physical_memory - sys2.physical_memory_avail) / sys2.physical_memory * 100;
        cpuStats.process_memory_percent = sys2.process_memory / sys2.physical_memory * 100;
        cpuStats.gpac_memory_percent = sys2.gpac_memory / sys2.physical_memory * 100;
      }
      if (sys2.total_cpu_usage > 0) {
        cpuStats.cpu_efficiency = sys2.process_cpu_usage / sys2.total_cpu_usage * 100;
      }
      return JSON.stringify({
        message: "cpu_stats",
        stats: cpuStats
      });
    });
    if (this.client.client) {
      this.client.client.send(serialized);
    }
    this.lastSent = now;
  };
  this.handleSessionEnd = function() {
    this.unsubscribe();
  };
}

// server/JSClient/Sys/LogManager.js
import { Sys as sys3 } from "gpaccore";

// server/JSClient/Sys/Utils/logs.js
function cleanupLogs(logs, maxSize) {
  if (logs.length <= maxSize) return logs;
  const byLevel = {
    error: logs.filter((log) => log.level === "error"),
    warning: logs.filter((log) => log.level === "warning"),
    info: logs.filter((log) => log.level === "info"),
    debug: logs.filter((log) => log.level === "debug")
  };
  const toKeep = {
    error: Math.ceil(byLevel.error.length * LOG_RETENTION.keepRatio.error),
    warning: Math.ceil(byLevel.warning.length * LOG_RETENTION.keepRatio.warning),
    info: Math.ceil(byLevel.info.length * LOG_RETENTION.keepRatio.info),
    debug: Math.ceil(byLevel.debug.length * LOG_RETENTION.keepRatio.debug)
  };
  const kept = [
    ...byLevel.error.slice(-toKeep.error),
    ...byLevel.warning.slice(-toKeep.warning),
    ...byLevel.info.slice(-toKeep.info),
    ...byLevel.debug.slice(-toKeep.debug)
  ];
  kept.sort((a, b) => a.timestamp - b.timestamp);
  if (kept.length > maxSize) {
    return kept.slice(-maxSize);
  }
  return kept;
}

// server/JSClient/Sys/LogManager.js
function LogManager(client) {
  this.client = client;
  this.isSubscribed = false;
  this.logLevel = "all@quiet";
  this.logs = [];
  this.maxHistorySize = LOG_RETENTION.maxHistorySize;
  this.originalLogConfig = null;
  this.pendingLogs = [];
  this.incomingBuffer = [];
  this.batchTimer = null;
  this.subscribe = function(logLevel) {
    if (this.isSubscribed) {
      this.updateLogLevel(logLevel);
      return;
    }
    this.logLevel = logLevel;
    this.isSubscribed = true;
    try {
      this.originalLogConfig = sys3.get_logs(true);
      sys3.use_logx = true;
      sys3.on_log = (tool, level, message, thread_id, caller) => {
        this.handleLog(tool, level, message, thread_id, caller);
      };
      sys3.set_logs(this.logLevel);
      this.client.sessionManager.startMonitoringLoop();
    } catch (error) {
      console.error("LogManager: Failed to start log capturing:", error);
      this.isSubscribed = false;
    }
  };
  this.unsubscribe = function() {
    if (!this.isSubscribed) return;
    try {
      this.flushPendingLogs();
      sys3.on_log = void 0;
      if (this.originalLogConfig) {
        sys3.set_logs(this.originalLogConfig);
      }
      this.isSubscribed = false;
      this.logs = [];
      this.pendingLogs = [];
      this.batchTimer = null;
    } catch (error) {
      console.error("LogManager: Failed to stop log capturing:", error);
    }
  };
  this.handleLog = function(tool, level, message, thread_id, caller) {
    const log = {
      timestamp: sys3.clock_us(),
      timestampMs: Date.now(),
      tool,
      level,
      message: message?.length > 500 ? message.substring(0, 500) + "..." : message,
      thread_id,
      caller: this.serializeCaller(caller)
    };
    this.incomingBuffer.push(log);
  };
  this.serializeCaller = function(caller) {
    if (!caller || typeof caller !== "object") {
      return null;
    }
    return caller.idx !== void 0 ? caller.idx : caller.name || null;
  };
  this.tick = function(now) {
    if (!this.isSubscribed) return;
    if (this.incomingBuffer.length > 0) {
      this.processIncomingLogs();
    }
  };
  this.processIncomingLogs = function() {
    if (!this.isSubscribed || this.incomingBuffer.length === 0) {
      return;
    }
    const logsToProcess = this.incomingBuffer.splice(0);
    for (const log of logsToProcess) {
      if (this.logs.length >= this.maxHistorySize) {
        this.logs = cleanupLogs(this.logs, this.maxHistorySize);
      }
      this.logs.push(log);
      this.pendingLogs.push(log);
    }
    const hasDebugLogs = logsToProcess.some((log) => log.level === "debug" || log.level === "info");
    const maxPending = hasDebugLogs ? 20 : 50;
    const delay = hasDebugLogs ? 100 : 250;
    if (this.pendingLogs.length >= maxPending) {
      this.flushPendingLogs();
      return;
    }
    if (!this.batchTimer) {
      this.batchTimer = true;
      session.post_task(() => {
        if (!this.isSubscribed) return false;
        this.flushPendingLogs();
        return false;
      }, delay);
    }
  };
  this.updateLogLevel = function(logLevel) {
    if (!this.isSubscribed) return;
    const isVerbose = logLevel.includes("debug") || logLevel.includes("info");
    this.maxHistorySize = isVerbose ? LOG_RETENTION.maxHistorySizeVerbose : LOG_RETENTION.maxHistorySize;
    try {
      this.logs = [];
      this.pendingLogs = [];
      this.logLevel = logLevel;
      sys3.set_logs(logLevel);
      this.sendToClient({
        message: "log_config_changed",
        logLevel
      });
    } catch (error) {
      console.error("LogManager: Failed to update log level:", error);
    }
  };
  this.getStatus = function() {
    return {
      isSubscribed: this.isSubscribed,
      logLevel: this.logLevel,
      logCount: this.logs.length,
      currentLogConfig: sys3.get_logs()
    };
  };
  this.flushPendingLogs = function() {
    if (this.pendingLogs.length === 0) {
      this.batchTimer = null;
      return;
    }
    this.sendToClient({
      message: "log_batch",
      logs: this.pendingLogs
    });
    this.pendingLogs = [];
    this.batchTimer = null;
  };
  this.sendToClient = function(data) {
    if (this.client.client && typeof this.client.client.send === "function") {
      this.client.client.send(JSON.stringify(data));
    }
  };
  this.forceUnsubscribe = function() {
    try {
      this.flushPendingLogs();
      sys3.on_log = void 0;
      if (this.originalLogConfig) {
        sys3.set_logs(this.originalLogConfig);
      }
      this.isSubscribed = false;
      this.logs = [];
      this.pendingLogs = [];
      this.incomingBuffer = [];
      this.batchTimer = null;
    } catch (error) {
      console.error("LogManager: Error during force cleanup:", error);
    }
  };
  this.handleSessionEnd = function() {
    this.forceUnsubscribe();
  };
}

// server/JSClient/Filters/PID/PidPropsCollector.js
function PidPropsCollector(client) {
  this.client = client;
  this.collectIpidProps = function(filterIdx, ipidIdx) {
    session.lock_filters(true);
    try {
      let collectProperty2 = function(prop_name, prop_type, prop_val) {
        properties[prop_name] = {
          name: prop_name,
          type: prop_type,
          value: prop_val
        };
      };
      var collectProperty = collectProperty2;
      const filter = session.get_filter(filterIdx);
      if (!filter) {
        session.lock_filters(false);
        return { error: `Filter ${filterIdx} not found` };
      }
      const properties = {};
      filter.ipid_props(ipidIdx, collectProperty2);
      session.lock_filters(false);
      return properties;
    } catch (e) {
      session.lock_filters(false);
      return { error: `Failed to enumerate IPID ${ipidIdx}: ${e.message}` };
    }
  };
}

// server/JSClient/CommandLineManager.js
import { Sys as sys4 } from "gpaccore";
function CommandLineManager(client) {
  this.client = client;
  this.getCommandLine = function() {
    try {
      if (typeof sys4 !== "undefined" && sys4.args) {
        if (Array.isArray(sys4.args) && sys4.args.length > 0) {
          const commandLine = sys4.args.join(" ");
          return commandLine;
        }
      }
    } catch (e) {
      print("[CommandLineManager] Error getting command line: " + e);
      return null;
    }
  };
  this.sendCommandLine = function() {
    const commandLine = this.getCommandLine();
    if (commandLine) {
      this.client.client.send(JSON.stringify({
        message: "command_line_response",
        commandLine,
        timestamp: Date.now()
      }));
    } else {
      this.client.client.send(JSON.stringify({
        message: "command_line_response",
        commandLine: null,
        error: "Could not retrieve command line",
        timestamp: Date.now()
      }));
    }
  };
}

// server/JSClient/index.js
function JSClient(id, client, all_clients2, draned_once_ref) {
  this.id = id;
  this.client = client;
  this.messageHandler = new MessageHandler(this);
  this.sessionStatsManager = new SessionStatsManager(this);
  this.sessionManager = new SessionManager(this);
  this.filterManager = new FilterManager(this, draned_once_ref);
  this.cpuStatsManager = new CpuStatsManager(this);
  this.logManager = new LogManager(this);
  this.pidPropsCollector = new PidPropsCollector(this);
  this.commandLineManager = new CommandLineManager(this);
  this.on_client_data = function(msg) {
    this.messageHandler.handleMessage(msg, all_clients2);
  };
  this.cleanup = function() {
    try {
      if (this.logManager) {
        this.logManager.forceUnsubscribe();
      }
      if (this.sessionManager && typeof this.sessionManager.cleanup === "function") {
        this.sessionManager.cleanup();
      }
      if (this.cpuStatsManager && typeof this.cpuStatsManager.cleanup === "function") {
        this.cpuStatsManager.cleanup();
      }
    } catch (error) {
      console.error(`JSClient ${this.id}: Error during cleanup:`, error);
    }
  };
}

// server/server.js
var all_clients = [];
var cid = 0;
var filter_uid = 0;
var draned_once = false;
var all_filters = [];
session.reporting(true);
var remove_client = function(client_id) {
  for (let i = 0; i < all_clients.length; i++) {
    if (all_clients[i].id == client_id) {
      all_clients.splice(i, 1);
      return;
    }
  }
};
session.set_new_filter_fun((f) => {
  f.idx = filter_uid++;
  f.iname = "" + f.idx;
  all_filters.push(f);
  if (f.itag == "NODISPLAY")
    return;
  if (draned_once) {
    sys5.sleep(100);
  }
});
session.set_del_filter_fun((f) => {
  let idx = all_filters.indexOf(f);
  if (idx >= 0)
    all_filters.splice(idx, 1);
  if (f.itag == "NODISPLAY")
    return;
  if (draned_once) {
    sys5.sleep(100);
  }
});
sys5.rmt_on_new_client = function(client) {
  let draned_once_ref = { value: draned_once };
  let js_client = new JSClient(++cid, client, all_clients, draned_once_ref);
  all_clients.push(js_client);
  js_client.client.on_data = (msg) => {
    if (typeof msg == "string")
      js_client.on_client_data(msg);
  };
  js_client.client.on_close = function() {
    js_client.cleanup();
    remove_client(js_client.id);
    js_client.client = null;
  };
  draned_once = draned_once_ref.value;
};
