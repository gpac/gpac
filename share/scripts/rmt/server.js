// server/server.js
import { Sys as sys9 } from "gpaccore";

// server/JSClient/Messaging/MessageHandler.js
import { Sys as sys } from "gpaccore";

// server/config/live.config.js
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

// server/JSClient/config.js
var DEFAULT_FILTER_FIELDS = [
  "idx",
  "status",
  "bytes_done",
  "bytes_sent",
  "pck_sent",
  "pck_done",
  "time",
  "nb_ipid",
  "nb_opid",
  "errors",
  "current_errors",
  "last_task_time"
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
  "current_errors",
  "po"
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
  "current_errors",
  "last_task_time"
];

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

// server/history/HistoryFileReader.js
import * as std from "std";
import * as os from "os";
var ALLOWED_EXACT_FILES = ["snapshot.json", "events.jsonl", "manifest.json", "logs.jsonl", "journal_index.json"];
var VALID_SESSION_ID = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}$/;
var VALID_CHUNK_FILE = /^chunks\/chunk_\d{4}\.jsonl$/;
var VALID_LOG_CHUNK_FILE = /^logs\/logs_\d{4}\.jsonl$/;
var VALID_CHECKPOINT_FILE = /^checkpoints\/cp_\d{4}\.json$/;
function HistoryFileReader(historyDir) {
  const baseDir = historyDir || "history";
  this.listSessions = function() {
    const sessions = [];
    const [entries, err] = os.readdir(baseDir);
    if (err) return sessions;
    for (const entry of entries) {
      if (!VALID_SESSION_ID.test(entry)) continue;
      const dir = `${baseDir}/${entry}`;
      const hasSnapshot = fileExists(`${dir}/snapshot.json`);
      const hasManifest = fileExists(`${dir}/manifest.json`);
      const hasEvents = fileExists(`${dir}/events.jsonl`);
      if (!hasSnapshot && !hasManifest && !hasEvents) continue;
      const hasCheckpoints = dirHasFiles(`${dir}/checkpoints`, /^cp_\d{4}\.json$/);
      const sizeBytes = hasEvents ? fileSize(`${dir}/events.jsonl`) : 0;
      const isComplete = fileExists(`${dir}/done`);
      sessions.push({ sessionId: entry, hasSnapshot, hasEvents, hasManifest, hasCheckpoints, sizeBytes, isComplete });
    }
    sessions.sort((a, b) => a.sessionId < b.sessionId ? 1 : a.sessionId > b.sessionId ? -1 : 0);
    return sessions;
  };
  this.readEventsRange = function(sessionId, fromUs, toUs) {
    const manifestResult = this.readFile(sessionId, "manifest.json");
    if (manifestResult.ok) {
      return readEventsFromChunks(this, sessionId, manifestResult.content, fromUs, toUs);
    }
    const legacyResult = this.readFile(sessionId, "events.jsonl");
    if (!legacyResult.ok) return [];
    return parseAndFilterEvents(legacyResult.content, fromUs, toUs);
  };
  this.readFile = function(sessionId, fileName) {
    if (!VALID_SESSION_ID.test(sessionId)) {
      return { ok: false, error: "session_not_found", detail: `Invalid sessionId: ${sessionId}` };
    }
    const isAllowed = ALLOWED_EXACT_FILES.includes(fileName) || VALID_CHUNK_FILE.test(fileName) || VALID_LOG_CHUNK_FILE.test(fileName) || VALID_CHECKPOINT_FILE.test(fileName);
    if (!isAllowed) {
      return { ok: false, error: "file_not_allowed", detail: `File not allowed: ${fileName}` };
    }
    const path = `${baseDir}/${sessionId}/${fileName}`;
    const file = std.open(path, "r");
    if (!file) {
      return { ok: false, error: "file_not_found", detail: `${fileName} not found in session ${sessionId}` };
    }
    try {
      const content = file.readAsString();
      return { ok: true, content };
    } catch (e) {
      return { ok: false, error: "read_error", detail: String(e) };
    } finally {
      file.close();
    }
  };
}
function readEventsFromChunks(reader, sessionId, manifestContent, fromUs, toUs) {
  let manifest;
  try {
    manifest = JSON.parse(manifestContent);
  } catch (e) {
    print(`[HistoryFileReader] Failed to parse manifest for session ${sessionId}: ${e}`);
    return [];
  }
  const { startUs, chunkCount, chunkDurationUs } = manifest;
  const fromChunk = fromUs !== void 0 ? Math.max(0, Math.floor((fromUs - startUs) / chunkDurationUs)) : 0;
  const toChunk = toUs !== void 0 ? Math.min(chunkCount - 1, Math.floor((toUs - startUs) / chunkDurationUs)) : chunkCount - 1;
  let events = [];
  for (let i = fromChunk; i <= toChunk; i++) {
    const file = `chunks/chunk_${String(i).padStart(4, "0")}.jsonl`;
    const chunkResult = reader.readFile(sessionId, file);
    if (!chunkResult.ok) continue;
    events = events.concat(parseAndFilterEvents(chunkResult.content, fromUs, toUs));
  }
  return events;
}
function parseAndFilterEvents(content, fromUs, toUs) {
  return content.split("\n").filter((line) => line.trim()).map((line) => {
    try {
      return JSON.parse(line);
    } catch (_e) {
      return null;
    }
  }).filter((event) => event !== null).filter(
    (event) => (fromUs === void 0 || event.ts_us >= fromUs) && (toUs === void 0 || event.ts_us <= toUs)
  );
}
function fileExists(path) {
  const file = std.open(path, "r");
  if (!file) return false;
  file.close();
  return true;
}
function fileSize(path) {
  const [stat3, err] = os.stat(path);
  if (err) return 0;
  return stat3.size || 0;
}
function dirHasFiles(dir, pattern) {
  const [entries, err] = os.readdir(dir);
  if (err) return false;
  return entries.some((entry) => pattern.test(entry));
}

// server/JSClient/Messaging/MessageHandler.js
var historyReader = new HistoryFileReader(sys.get_opt("core", "rmt-log") || "history");
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
            this.client.ensureMonitoringLoop();
          },
          "unsubscribe_session": () => {
            this.client.sessionStatsManager.unsubscribe();
          },
          "subscribe_filter": () => {
            const idx = jtext.idx;
            let pidScope = jtext.pidScope || "both";
            if (!pidScope) {
              pidScope = "both";
            }
            this.client.filterManager.subscribeToFilter(idx, pidScope);
            this.client.ensureMonitoringLoop();
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
            this.client.ensureMonitoringLoop();
          },
          "unsubscribe_cpu_stats": () => {
            this.client.cpuStatsManager.unsubscribe();
          },
          "subscribe_logs": () => {
            const logLevel = jtext["logLevel"] || "all@warning";
            this.client.logManager.subscribe(logLevel);
            this.client.ensureMonitoringLoop();
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
          },
          "list_sessions": () => {
            const sessions = historyReader.listSessions();
            this.client.client.send(JSON.stringify({
              command: "list_sessions",
              status: "ok",
              sessions
            }));
          },
          "read_events_range": () => {
            const { sessionId, fromUs, toUs } = jtext;
            const events = historyReader.readEventsRange(sessionId, fromUs, toUs);
            this.client.client.send(JSON.stringify({
              command: "events_range",
              status: "ok",
              sessionId,
              events
            }));
          },
          "read_file": () => {
            const { sessionId, file } = jtext;
            const result = historyReader.readFile(sessionId, file);
            if (result.ok) {
              this.client.client.send(JSON.stringify({
                command: "read_file",
                status: "ok",
                sessionId,
                file,
                content: result.content
              }));
            } else {
              this.client.client.send(JSON.stringify({
                command: "read_file",
                status: "error",
                error: result.error,
                detail: result.detail
              }));
            }
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
  this.lastSentMetrics = "";
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
        ts_us: now,
        stats
      });
    });
    if (this.client.client) {
      this.client.client.send(serialized);
    }
    const sessionMetrics = session.session_metrics;
    if (sessionMetrics && sessionMetrics !== this.lastSentMetrics && this.client.client) {
      this.lastSentMetrics = sessionMetrics;
      this.client.client.send(JSON.stringify({
        message: "session_metrics",
        data: sessionMetrics
      }));
    }
  };
  this.cleanup = function() {
    this.isSubscribed = false;
    this.lastSentMetrics = "";
  };
  this.handleSessionEnd = function() {
    this.unsubscribe();
  };
}

// server/JSClient/Session/SessionManager.js
function SessionManager(client) {
  this.client = client;
  this.hasActiveSubscriptions = function() {
    return this.client.sessionStatsManager.isSubscribed || this.client.cpuStatsManager.isSubscribed || this.client.logManager.isSubscribed || Object.keys(this.client.filterManager.filterSubscriptions).length > 0;
  };
  this.getMinInterval = function() {
    let interval = 1e3;
    if (this.client.sessionStatsManager.isSubscribed) {
      interval = Math.min(interval, this.client.sessionStatsManager.interval);
    }
    if (this.client.cpuStatsManager.isSubscribed) {
      interval = Math.min(interval, this.client.cpuStatsManager.interval);
    }
    return interval;
  };
  this.tick = function(now) {
    this.client.cpuStatsManager.tick(now);
    this.client.logManager.tick(now);
    this.client.filterManager.tick(now);
    this.client.sessionStatsManager.tick(now);
  };
  this.handleSessionEnd = function(now) {
    this.tick(now);
    this.client.cpuStatsManager.handleSessionEnd();
    this.client.logManager.handleSessionEnd();
    this.client.filterManager.handleSessionEnd();
    this.client.sessionStatsManager.handleSessionEnd();
    try {
      this.client.client.send(JSON.stringify({
        message: "session_end",
        reason: "completed",
        timestamp: now
      }));
    } catch (e) {
      print("[SessionManager] Error sending session_end:", e);
    }
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
    ipid: [],
    opid: []
  };
  for (let i = 0; i < f.nb_ipid; i++) {
    minimalFilters.ipid.push({
      pid_index: i,
      name: f.ipid_props(i, "name"),
      source_idx: f.ipid_source(i).idx,
      stream_type: f.ipid_props(i, "StreamType")
    });
  }
  for (let o = 0; o < f.nb_opid; o++) {
    minimalFilters.opid.push({
      pid_index: o,
      name: f.opid_props(o, "name"),
      stream_type: f.opid_props(o, "StreamType")
    });
  }
  return minimalFilters;
}
function on_all_connected(cb) {
  session.post_task(() => {
    let all_filters_instances = [];
    session.lock_filters(true);
    for (let i = 0; i < session.nb_filters; i++) {
      const f = session.get_filter(i);
      if (!f.is_destroyed()) all_filters_instances.push(f);
    }
    session.lock_filters(false);
    cb(all_filters_instances);
    return false;
  });
}

// server/JSClient/Filters/PID/PidDataCollector.js
function _collectMediaProps(getProp) {
  return {
    timescale: getProp("Timescale"),
    codec: getProp("CodecID"),
    type: getProp("StreamType"),
    width: getProp("Width"),
    height: getProp("Height"),
    pixelformat: getProp("PixelFormat"),
    bitrate: getProp("Bitrate"),
    samplerate: getProp("SampleRate"),
    channels: getProp("Channels")
  };
}
function _collectStats(stats, filter) {
  if (!stats) return null;
  const result = {
    disconnected: stats.disconnected,
    average_process_rate: stats.average_process_rate,
    max_process_rate: stats.max_process_rate,
    average_bitrate: stats.average_bitrate,
    max_bitrate: stats.max_bitrate,
    nb_processed: stats.nb_processed,
    max_process_time: stats.max_process_time,
    total_process_time: stats.total_process_time
  };
  const lastTs = stats.last_ts_sent ?? filter.last_ts_sent;
  if (lastTs) result.last_ts_sent = lastTs;
  if (stats.last_process_time) result.last_process_time = stats.last_process_time;
  if (stats.buffer_time) result.buffer_time = stats.buffer_time;
  if (stats.nb_buffer_units) result.nb_buffer_units = stats.nb_buffer_units;
  if (stats.max_buffer_time) result.max_buffer_time = stats.max_buffer_time;
  if (stats.max_playout_time) result.max_playout_time = stats.max_playout_time;
  if (stats.min_playout_time) result.min_playout_time = stats.min_playout_time;
  if (stats.total_process_time > 0) result.average_process_time = stats.total_process_time / stats.nb_processed;
  if (stats.first_process_time) result.first_process_time = stats.first_process_time;
  return result;
}
function PidDataCollector() {
  this.collectInputPids = function(filter, withPidProperties, statsOnly) {
    const ipids = {};
    for (let i = 0; i < filter.nb_ipid; i++) {
      const originalName = filter.ipid_props(i, "name");
      const getProp = (name) => filter.ipid_props(i, name);
      const pid = {
        name: filter.nb_ipid > 1 && originalName ? `${originalName}_${i}` : originalName,
        buffer: getProp("buffer"),
        nb_pck_queued: getProp("nb_pck_queued"),
        would_block: getProp("would_block"),
        eos: getProp("eos"),
        playing: getProp("playing"),
        ..._collectMediaProps(getProp)
      };
      const source = filter.ipid_source(i);
      if (source) pid.source_idx = source.idx;
      const stats = _collectStats(filter.ipid_stats(i), filter);
      if (stats) pid.stats = stats;
      if (withPidProperties) {
        const allProps = {};
        filter.ipid_props(i, function(pname, ptype, pval) {
          allProps[pname] = { name: pname, type: ptype, value: pval };
        });
        pid.properties = allProps;
      }
      const key = pid.name || `ipid_${i}`;
      ipids[key] = statsOnly ? { buffer: pid.buffer, bitrate: pid.bitrate, ...pid.stats && { stats: pid.stats } } : pid;
    }
    return ipids;
  };
  this.collectOutputPids = function(filter, statsOnly) {
    const opids = {};
    for (let i = 0; i < filter.nb_opid; i++) {
      const originalName = filter.opid_props(i, "name");
      const getProp = (name) => filter.opid_props(i, name);
      const rawStats = filter.opid_stats(i);
      const pid = {
        name: filter.nb_opid > 1 && originalName ? `${originalName}_${i}` : originalName,
        buffer: getProp("buffer"),
        max_buffer: getProp("max_buffer"),
        nb_pck_queued: getProp("nb_pck_queued"),
        would_block: getProp("would_block"),
        eos_received: rawStats?.eos_received,
        playing: getProp("playing"),
        ..._collectMediaProps(getProp),
        id: getProp("ID"),
        trackNumber: getProp("TrackNumber"),
        serviceID: getProp("ServiceID"),
        language: getProp("Language"),
        role: getProp("Role")
      };
      const stats = _collectStats(rawStats, filter);
      if (stats) pid.stats = stats;
      const key = pid.name || `opid_${i}`;
      opids[key] = statsOnly ? { buffer: pid.buffer, max_buffer: pid.max_buffer, bitrate: pid.bitrate, ...pid.stats && { stats: pid.stats } } : pid;
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
      if (this.client.historyCollector) {
        this.client.historyCollector.recordFilterArgsUpdate(idx, argName, newValue);
      }
    } catch (e) {
      print("Error: Failed to update argument: " + e.toString());
    }
  };
}

// server/JSClient/Filters/FilterManager.js
function FilterManager(client) {
  this.client = client;
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
    });
  };
  this.requestDetails = function(idx) {
    this.details_needed[idx] = true;
    this.argumentHandler.sendDetails(idx);
  };
  this.stopDetails = function(idx) {
    this.details_needed[idx] = false;
  };
  this.subscribeToFilter = function(idx, pidScope) {
    this.filterSubscriptions[idx] = {
      interval: UPDATE_INTERVALS.FILTER_STATS,
      fields: FILTER_SUBSCRIPTION_FIELDS,
      pidScope: pidScope || "both"
    };
    this.lastSentByFilter[idx] = 0;
    this.client.ensureMonitoringLoop();
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
            payload.ipids = this.pidDataCollector.collectInputPids(fObj, true);
            break;
          case "opid":
            payload.opids = this.pidDataCollector.collectOutputPids(fObj);
            break;
          case "both":
            payload.ipids = this.pidDataCollector.collectInputPids(fObj, true);
            payload.opids = this.pidDataCollector.collectOutputPids(fObj);
            break;
          default:
            break;
        }
        return JSON.stringify({ message: "filter_stats", ts_us: now, ...payload });
      });
      if (serialized && this.client.client) {
        this.client.client.send(serialized);
        this.lastSentByFilter[idxStr] = now;
      }
    }
  };
  this.cleanup = function() {
    this.filterSubscriptions = {};
    this.lastSentByFilter = {};
  };
  this.handleSessionEnd = function() {
    this.filterSubscriptions = {};
    this.lastSentByFilter = {};
  };
  this.updateArgument = function(idx, name, argName, newValue) {
    this.argumentHandler.updateArgument(idx, name, argName, newValue);
  };
}

// server/JSClient/Sys/buildCpuStatsPayload.js
import { Sys as sys2 } from "gpaccore";
function buildCpuStatsPayload() {
  const stats = {
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
    stats.memory_usage_percent = (sys2.physical_memory - sys2.physical_memory_avail) / sys2.physical_memory * 100;
    stats.process_memory_percent = sys2.process_memory / sys2.physical_memory * 100;
    stats.gpac_memory_percent = sys2.gpac_memory / sys2.physical_memory * 100;
  }
  if (sys2.total_cpu_usage > 0) {
    stats.cpu_efficiency = sys2.process_cpu_usage / sys2.total_cpu_usage * 100;
  }
  return { stats };
}

// server/JSClient/Sys/CpuStatsManager.js
function CpuStatsManager(client) {
  this.client = client;
  this.isSubscribed = false;
  this.interval = UPDATE_INTERVALS.CPU_STATS;
  this.lastSent = 0;
  this.subscribe = function() {
    this.isSubscribed = true;
    this.interval = UPDATE_INTERVALS.CPU_STATS;
    this.lastSent = 0;
    this.client.ensureMonitoringLoop();
  };
  this.unsubscribe = function() {
    this.isSubscribed = false;
  };
  this.tick = function(now) {
    if (!this.isSubscribed) return;
    if (now - this.lastSent < this.interval) return;
    const serialized = cacheManager.getOrSet("cpu_stats", 50, () => {
      const { stats } = buildCpuStatsPayload();
      return JSON.stringify({ message: "cpu_stats", stats: { timestamp: now, ...stats } });
    });
    if (this.client.client) {
      this.client.client.send(serialized);
    }
    this.lastSent = now;
  };
  this.cleanup = function() {
    this.isSubscribed = false;
  };
  this.handleSessionEnd = function() {
    this.unsubscribe();
  };
}

// server/JSClient/Sys/LogManager.js
import { Sys as sys4 } from "gpaccore";

// server/JSClient/Sys/Utils/LogHub.js
import { Sys as sys3 } from "gpaccore";
var logHub = {
  subscribers: /* @__PURE__ */ new Map(),
  originalLogConfig: null,
  activeLogLevel: null,
  add(id, manager) {
    const wasEmpty = this.subscribers.size === 0;
    this.subscribers.set(id, manager);
    if (wasEmpty) {
      this.originalLogConfig = sys3.get_logs(true);
      sys3.use_logx = true;
      sys3.on_log = (tool, level, msg, tid, caller) => {
        for (const manager2 of this.subscribers.values()) manager2.handleLog(tool, level, msg, tid, caller);
      };
    }
  },
  remove(id) {
    this.subscribers.delete(id);
    if (this.subscribers.size === 0) this._teardown();
  },
  /** Force-clear everything (session end) — stops all log capture immediately */
  shutdown() {
    this.subscribers.clear();
    this._teardown();
  },
  setLogLevel(logLevel) {
    this.activeLogLevel = logLevel;
    sys3.set_logs(logLevel);
    for (const manager of this.subscribers.values()) {
      manager.logLevel = logLevel;
      manager.sendToClient({ message: "log_config_changed", logLevel });
    }
  },
  _teardown() {
    sys3.on_log = void 0;
    if (this.originalLogConfig) sys3.set_logs(this.originalLogConfig);
    this.originalLogConfig = null;
    this.activeLogLevel = null;
  }
};

// server/JSClient/Sys/LogManager.js
function LogManager(client) {
  this.client = client;
  this.isSubscribed = false;
  this.logLevel = "all@warning";
  this.pendingLogs = [];
  this.batchTimer = null;
  this.subscribe = function(logLevel) {
    if (this.isSubscribed) {
      this.updateLogLevel(logLevel);
      return;
    }
    this.logLevel = logLevel;
    this.isSubscribed = true;
    try {
      logHub.add(this.client.id, this);
      if (logHub.activeLogLevel) {
        this.logLevel = logHub.activeLogLevel;
        this.sendToClient({ message: "log_config_changed", logLevel: this.logLevel });
      } else {
        logHub.setLogLevel(this.logLevel);
      }
      this.client.ensureMonitoringLoop();
    } catch (error) {
      console.error("LogManager: Failed to start log capturing:", error);
      this.isSubscribed = false;
    }
  };
  this.unsubscribe = function() {
    if (!this.isSubscribed) return;
    try {
      this.flushPendingLogs();
      this.isSubscribed = false;
      logHub.remove(this.client.id);
      this.pendingLogs = [];
      this.batchTimer = null;
    } catch (error) {
      console.error("LogManager: Failed to stop log capturing:", error);
    }
  };
  this.handleLog = function(tool, level, message, thread_id, caller) {
    this.pendingLogs.push({
      timestamp: sys4.clock_us(),
      timestampMs: Date.now(),
      tool,
      level,
      message: message?.length > 500 ? message.substring(0, 500) + "..." : message,
      thread_id,
      caller: caller?.idx !== void 0 ? caller.idx : caller?.name || null
    });
    if (!this.batchTimer) {
      this.batchTimer = true;
      session.post_task(() => {
        if (!this.isSubscribed) return false;
        this.flushPendingLogs();
        return false;
      }, 50);
    }
  };
  this.tick = function(now) {
  };
  this.updateLogLevel = function(logLevel) {
    if (!this.isSubscribed) return;
    try {
      this.pendingLogs = [];
      logHub.setLogLevel(logLevel);
    } catch (error) {
      console.error("LogManager: Failed to update log level:", error);
    }
  };
  this.getStatus = function() {
    return {
      isSubscribed: this.isSubscribed,
      logLevel: this.logLevel,
      currentLogConfig: sys4.get_logs()
    };
  };
  this.flushPendingLogs = function() {
    if (this.pendingLogs.length === 0) {
      this.batchTimer = null;
      return;
    }
    this.sendToClient({ message: "log_batch", logs: this.pendingLogs });
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
      this.isSubscribed = false;
      logHub.remove(this.client.id);
      this.pendingLogs = [];
      this.batchTimer = null;
    } catch (error) {
      console.error("LogManager: Error during force cleanup:", error);
    }
  };
  this.handleSessionEnd = function() {
    logHub.shutdown();
    this.isSubscribed = false;
    this.pendingLogs = [];
    this.batchTimer = null;
  };
}

// server/JSClient/CommandLineManager.js
import { Sys as sys5 } from "gpaccore";
function CommandLineManager(client) {
  this.client = client;
  this.getCommandLine = function() {
    try {
      if (typeof sys5 !== "undefined" && sys5.args) {
        if (Array.isArray(sys5.args) && sys5.args.length > 0) {
          const commandLine = sys5.args.join(" ");
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
function JSClient(id, client, all_clients2, ensureMonitoringLoop2, historyCollector2) {
  this.id = id;
  this.client = client;
  this.ensureMonitoringLoop = ensureMonitoringLoop2;
  this.historyCollector = historyCollector2;
  this.messageHandler = new MessageHandler(this);
  this.sessionStatsManager = new SessionStatsManager(this);
  this.sessionManager = new SessionManager(this);
  this.filterManager = new FilterManager(this);
  this.cpuStatsManager = new CpuStatsManager(this);
  this.logManager = new LogManager(this);
  this.commandLineManager = new CommandLineManager(this);
  this.on_client_data = function(msg) {
    this.messageHandler.handleMessage(msg, all_clients2);
  };
  this.sendMonitorConfig = function() {
    this.client.send(JSON.stringify({
      message: "monitor_config",
      intervals: UPDATE_INTERVALS,
      logRetention: LOG_RETENTION
    }));
  };
  this.cleanup = function() {
    try {
      this.logManager.forceUnsubscribe();
      this.sessionStatsManager.cleanup();
      this.cpuStatsManager.cleanup();
      this.filterManager.cleanup();
    } catch (error) {
      console.error(`JSClient ${this.id}: Error during cleanup:`, error);
    }
  };
}

// server/history/HistoryCollector.js
import { Sys as sys6 } from "gpaccore";

// server/history/HistoryWriter.js
import * as std3 from "std";
import * as os2 from "os";

// server/history/helpers/ChunkStream.js
import * as std2 from "std";
function ChunkStream(dir, prefix, maxDuration, maxCount) {
  this._dir = dir;
  this._prefix = prefix;
  this._maxDuration = maxDuration;
  this._maxCount = maxCount || Infinity;
  this._file = null;
  this._index = 0;
  this._count = 0;
  this._startUs = null;
  this._lastUs = null;
  this._completed = [];
  this._path = function(index) {
    const padded = String(index).padStart(4, "0");
    return `${this._dir}/${this._prefix}_${padded}.jsonl`;
  };
  this._open = function() {
    if (this._file) {
      this._file.close();
      this._file = null;
    }
    const path = this._path(this._index);
    this._file = std2.open(path, "a");
    if (!this._file) {
      print(`[ChunkStream] Failed to open ${path}`);
    }
    this._count = 0;
    this._startUs = null;
  };
  this._finalizeCurrentChunk = function() {
    const dirName = this._dir.split("/").pop();
    return {
      file: `${dirName}/${this._prefix}_${String(this._index).padStart(4, "0")}.jsonl`,
      fromUs: this._startUs,
      toUs: this._lastUs,
      count: this._count
    };
  };
  this.write = function(line, tsUs) {
    if (!this._file) this._open();
    if (!this._file) return false;
    if (this._startUs === null) this._startUs = tsUs;
    this._file.puts(line + "\n");
    this._count++;
    this._lastUs = tsUs;
    const shouldRotate = tsUs - this._startUs >= this._maxDuration || this._count >= this._maxCount;
    if (shouldRotate) {
      this._completed.push(this._finalizeCurrentChunk());
      this._index++;
      this._open();
      return true;
    }
    return false;
  };
  this.getChunkCount = function() {
    return this._index + 1;
  };
  this.getAllChunks = function() {
    return this._completed.slice();
  };
  this.getAllChunksIncludingOpen = function() {
    const all = this._completed.slice();
    if (this._file && this._count > 0) all.push(this._finalizeCurrentChunk());
    return all;
  };
  this.close = function() {
    if (this._file) {
      if (this._count > 0) this._completed.push(this._finalizeCurrentChunk());
      this._file.close();
      this._file = null;
    }
  };
  this._open();
}

// server/config/history.config.js
var RATE_LIMIT_US = 1e3 * 1e3;
var CHUNK_DURATION_US = 10 * 1e3 * 1e3;
var MAX_LOG_PER_CHUNK = 5e3;
var MAX_LOG_MESSAGE_LENGTH = 500;
var MANIFEST_REFRESH_US = 2 * 1e3 * 1e3;

// server/history/HistoryWriter.js
function writeFileAtomic(path, content) {
  const tmpPath = `${path}.tmp`;
  const tmpFile = std3.open(tmpPath, "w");
  if (!tmpFile) {
    print(`[HistoryWriter] Failed to write ${tmpPath}`);
    return;
  }
  tmpFile.puts(content);
  tmpFile.close();
  const err = os2.rename(tmpPath, path);
  if (err !== 0) {
    print(`[HistoryWriter] Failed to rename ${tmpPath} to ${path} (errno ${err})`);
  }
}
function formatSessionId(date) {
  const pad = (value) => String(value).padStart(2, "0");
  const datePart = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
  const timePart = `${pad(date.getHours())}-${pad(date.getMinutes())}-${pad(date.getSeconds())}`;
  return `${datePart}_${timePart}`;
}
function HistoryWriter(historyDir, sessionId) {
  const baseDir = historyDir || "history";
  const id = sessionId || formatSessionId(/* @__PURE__ */ new Date());
  const dir = `${baseDir}/${id}`;
  const chunksDir = `${dir}/chunks`;
  const logsDir = `${dir}/logs`;
  const checkpointsDir = `${dir}/checkpoints`;
  this.sessionId = id;
  this.sessionDir = dir;
  this.snapshotPath = `${dir}/snapshot.json`;
  this._initialized = false;
  this._events = null;
  this._logs = null;
  this._sessionStartUs = null;
  this._lastEventUs = null;
  this._checkpoints = [];
  this._eventsIndex = [];
  this._journalTsUs = [];
  this._journalTypes = [];
  this._lastManifestUs = null;
  this._init = function() {
    if (this._initialized) return;
    this._initialized = true;
    try {
      os2.mkdir(baseDir);
    } catch (_e) {
    }
    try {
      os2.mkdir(dir);
    } catch (_e) {
    }
    try {
      os2.mkdir(chunksDir);
    } catch (_e) {
    }
    try {
      os2.mkdir(logsDir);
    } catch (_e) {
    }
    try {
      os2.mkdir(checkpointsDir);
    } catch (_e) {
    }
    const [, statErr] = os2.stat(dir);
    if (statErr !== 0) print(`[HistoryWriter] Cannot create ${dir} \u2014 check the -rmt-log path and permissions`);
    else print(`[HistoryWriter] Recording session ${id} to ${dir}/`);
    this._events = new ChunkStream(chunksDir, "chunk", CHUNK_DURATION_US);
    this._logs = new ChunkStream(logsDir, "logs", CHUNK_DURATION_US, MAX_LOG_PER_CHUNK);
  };
  this._updateTimestamps = function(tsUs) {
    if (this._sessionStartUs === null) this._sessionStartUs = tsUs;
    this._lastEventUs = tsUs;
  };
  this.addEventIndex = function(tsUs, type, metadata) {
    if (!Number.isFinite(tsUs)) return;
    this._eventsIndex.push(metadata ? { ts_us: tsUs, type, ...metadata } : { ts_us: tsUs, type });
  };
  this.recordJournalFact = function(tsUs, type) {
    if (!Number.isFinite(tsUs)) return;
    this._journalTsUs.push(tsUs);
    this._journalTypes.push(type);
  };
  this._writeJournalIndex = function() {
    if (this._journalTsUs.length === 0) return;
    const baseTsUs = this._journalTsUs[0];
    const journalIndex = {
      baseTsUs,
      tsDeltaUs: this._journalTsUs.map((tsUs) => tsUs - baseTsUs),
      types: this._journalTypes
    };
    writeFileAtomic(`${dir}/journal_index.json`, JSON.stringify(journalIndex) + "\n");
  };
  this._getJournalPointer = function() {
    if (this._journalTsUs.length === 0) return void 0;
    const errorCount = this._journalTypes.filter((type) => type === 1).length;
    const warningCount = this._journalTypes.filter((type) => type === 2).length;
    return {
      file: "journal_index.json",
      format: "columnar-delta-v1",
      eventCount: this._journalTsUs.length,
      errorCount,
      warningCount
    };
  };
  this._writeManifest = function() {
    const manifest = {
      version: 1,
      startUs: this._sessionStartUs,
      endUs: this._lastEventUs,
      chunkDurationUs: CHUNK_DURATION_US,
      chunkCount: this._events ? this._events.getChunkCount() : 1,
      snapshot: "snapshot.json",
      checkpoints: this._checkpoints,
      eventChunks: this._events ? this._events.getAllChunksIncludingOpen() : [],
      logChunks: this._logs ? this._logs.getAllChunksIncludingOpen() : [],
      eventsIndex: [...this._eventsIndex].sort((a, b) => a.ts_us - b.ts_us),
      journalIndex: this._getJournalPointer()
    };
    this._writeJournalIndex();
    writeFileAtomic(`${dir}/manifest.json`, JSON.stringify(manifest) + "\n");
    this._lastManifestUs = this._lastEventUs;
  };
  this._maybeWriteManifest = function() {
    if (this.getCurrentChunkIndex() !== 0) return;
    if (this._lastManifestUs !== null && this._lastEventUs - this._lastManifestUs < MANIFEST_REFRESH_US) return;
    this._writeManifest();
  };
  this.writeSnapshot = function(obj) {
    this._init();
    const snapshotFile = std3.open(this.snapshotPath, "w");
    if (!snapshotFile) {
      print(`[HistoryWriter] Failed to write snapshot`);
      return;
    }
    snapshotFile.puts(JSON.stringify(obj) + "\n");
    snapshotFile.close();
  };
  this.writeLog = function(jsonString, tsUs) {
    this._init();
    this._updateTimestamps(tsUs);
    if (this._logs.write(jsonString, tsUs)) this._writeManifest();
    else this._maybeWriteManifest();
  };
  this.writeEvent = function(jsonString, tsUs) {
    this._init();
    this._updateTimestamps(tsUs);
    const rotated = this._events.write(jsonString, tsUs);
    if (rotated) this._writeManifest();
    else this._maybeWriteManifest();
    return rotated;
  };
  this.writeCheckpoint = function(chunkIndex, obj) {
    const padded = String(chunkIndex).padStart(4, "0");
    const file = `checkpoints/cp_${padded}.json`;
    const cpFile = std3.open(`${dir}/${file}`, "w");
    if (!cpFile) {
      print(`[HistoryWriter] Failed to write checkpoint ${file}`);
      return;
    }
    cpFile.puts(JSON.stringify(obj) + "\n");
    cpFile.close();
    this._checkpoints.push({ chunkIndex, file });
    this._writeManifest();
  };
  this.getCurrentChunkIndex = function() {
    return this._events ? this._events._index : 0;
  };
  this.getCurrentLogChunkIndex = function() {
    return this._logs ? this._logs._index : 0;
  };
  this.close = function() {
    if (this._events) this._events.close();
    if (this._logs) this._logs.close();
    this._writeManifest();
    const doneFile = std3.open(`${dir}/done`, "w");
    if (doneFile) doneFile.close();
  };
}

// server/history/HistoryCollector.js
var EVENT_VERSION = 1;
var LOG_ID = "_hist_";
var LOG_LEVEL_ERROR = 1;
var LOG_LEVEL_WARNING = 2;
function HistoryCollector(historyDir) {
  this.enabled = !!historyDir;
  this.writer = new HistoryWriter(historyDir);
  this.pidCollector = new PidDataCollector();
  this.snapshotWritten = false;
  this.lastRecordUs = 0;
  this.lastCpuRecordUs = 0;
  this.pendingLogs = [];
  this.logBatchTimer = null;
  this._latestStructural = null;
  this._currentPidState = null;
  this._currentArgState = {};
  this._chunkNeedsCheckpoint = false;
  this._lastEventTsUs = 0;
  this._writeCheckpointIfNeeded = function(chunkIndex, tsUs) {
    if (!this._latestStructural) return;
    if (!this._chunkNeedsCheckpoint) return;
    if (chunkIndex <= 0) {
      this._chunkNeedsCheckpoint = false;
      return;
    }
    const checkpoint = {
      version: this._latestStructural.version,
      ts_us: tsUs,
      graph_v: this._latestStructural.graph_v,
      filters: this._latestStructural.filters,
      pid_state: this._currentPidState,
      metric_defs: session.session_metrics || null
    };
    if (Object.keys(this._currentArgState).length > 0) {
      checkpoint.arg_state = this._currentArgState;
      this._currentArgState = {};
    }
    this.writer.writeCheckpoint(chunkIndex, checkpoint);
    this._chunkNeedsCheckpoint = false;
  };
  this._onChunkRotated = function() {
    const newChunkIndex = this.writer.getCurrentChunkIndex();
    this._writeCheckpointIfNeeded(newChunkIndex, this._lastEventTsUs);
  };
  this.startLogCapture = function() {
    this.initialLogConfig = sys6.get_logs(true);
    logHub.add(LOG_ID, this);
  };
  this.writeSnapshot = function(data) {
    if (this.snapshotWritten) return;
    data.log_config = this.initialLogConfig || null;
    this.writer.writeSnapshot(data);
    this.snapshotWritten = true;
  };
  this.recordGraph = function(filters, filterInstances, graphVersion) {
    const pidCollector2 = new PidDataCollector();
    const eventFilters = filters.map((filter, index) => {
      const { ipid, opid, gpac_args, ...rest } = filter;
      const filterInstance = filterInstances[index];
      return {
        ...rest,
        ipids: ipid ?? [],
        opids: opid ?? [],
        gpac_args: filterInstance.all_args(true).filter(Boolean),
        properties: {
          ipids: pidCollector2.collectInputPids(filterInstance, true),
          opids: pidCollector2.collectOutputPids(filterInstance)
        }
      };
    });
    const checkpointFilters = eventFilters.map((filter) => {
      const { gpac_args, ...rest } = filter;
      const strippedIpids = Object.fromEntries(
        Object.entries(filter.properties.ipids).map(([k, v]) => {
          const { properties, ...pidRest } = v;
          return [k, pidRest];
        })
      );
      return {
        ...rest,
        properties: { ...filter.properties, ipids: strippedIpids }
      };
    });
    const filtersTsUs = sys6.clock_us();
    this.writer.addEventIndex(filtersTsUs, "graph-change");
    if (!this.snapshotWritten) {
      this.writeSnapshot({
        version: EVENT_VERSION,
        ts_us: filtersTsUs,
        command_line: null,
        graph_v: graphVersion,
        filters: eventFilters
      });
    }
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "filters",
      ts_us: filtersTsUs,
      graph_v: graphVersion,
      filters: eventFilters
    }), filtersTsUs);
    this._lastEventTsUs = filtersTsUs;
    this._latestStructural = {
      version: EVENT_VERSION,
      graph_v: graphVersion,
      filters: checkpointFilters
    };
    this._currentPidState = eventFilters.reduce((acc, filter) => {
      const allPidProperties = {};
      for (const [key, pid] of Object.entries(filter.properties.ipids)) {
        if (pid.properties) allPidProperties[key] = pid.properties;
      }
      if (Object.keys(allPidProperties).length > 0) acc[filter.idx] = allPidProperties;
      return acc;
    }, {});
    this._currentArgState = {};
    this._chunkNeedsCheckpoint = true;
    if (rotated) this._onChunkRotated(filtersTsUs);
  };
  this.recordSessionStats = function(payload, force) {
    const ts_us = sys6.clock_us();
    if (!force && ts_us - this.lastRecordUs < RATE_LIMIT_US) return;
    this.lastRecordUs = ts_us;
    const filterMap = {};
    session.lock_filters(true);
    for (let i = 0; i < session.nb_filters; i++) {
      const f = session.get_filter(i);
      if (!f.is_destroyed()) filterMap[f.idx] = f;
    }
    const enrichedStats = payload.stats.map((stat3) => {
      const f = filterMap[stat3.idx];
      if (!f) return stat3;
      const entry = { ...stat3 };
      if (f.nb_ipid > 0) entry.ipids = this.pidCollector.collectInputPids(f, false, true);
      if (f.nb_opid > 0) entry.opids = this.pidCollector.collectOutputPids(f, true);
      return entry;
    });
    session.lock_filters(false);
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "session_stats",
      ts_us,
      all_packets_done: payload.all_packets_done,
      stats: enrichedStats
    }), ts_us);
    this._lastEventTsUs = ts_us;
    if (rotated) this._onChunkRotated(ts_us);
  };
  this.recordCpuStats = function(payload) {
    const cpuTsUs = sys6.clock_us();
    if (cpuTsUs - this.lastCpuRecordUs < RATE_LIMIT_US) return;
    this.lastCpuRecordUs = cpuTsUs;
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "cpu_stats",
      ts_us: cpuTsUs,
      ...payload
    }), cpuTsUs);
    this._lastEventTsUs = cpuTsUs;
    if (rotated) this._onChunkRotated(cpuTsUs);
  };
  this.recordPidReconfigured = function(indexes, pidsByFilter) {
    const tsUs = sys6.clock_us();
    this.writer.addEventIndex(tsUs, "pid-reconfig");
    if (!this._currentPidState) {
      this._currentPidState = {};
    }
    for (const idx of indexes) {
      if (pidsByFilter[idx]) {
        const allPidProperties = {};
        for (const [key, pid] of Object.entries(pidsByFilter[idx])) {
          if (pid.properties) allPidProperties[key] = pid.properties;
        }
        if (Object.keys(allPidProperties).length > 0) this._currentPidState[idx] = allPidProperties;
      }
    }
    if (indexes.length > 0) {
      this._chunkNeedsCheckpoint = true;
    }
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "filter_pid_reconfigured",
      ts_us: tsUs,
      indexes,
      pidsByFilter
    }), tsUs);
    this._lastEventTsUs = tsUs;
    if (rotated) this._onChunkRotated(tsUs);
  };
  this.recordArgUpdated = function(indexes, argsByFilter) {
    const tsUs = sys6.clock_us();
    this.writer.addEventIndex(tsUs, "args-change");
    for (const idx of indexes) {
      if (argsByFilter[idx]) {
        this._currentArgState[idx] = argsByFilter[idx];
      }
    }
    if (indexes.length > 0) {
      this._chunkNeedsCheckpoint = true;
    }
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "filter_arg_updated",
      ts_us: tsUs,
      indexes,
      argsByFilter
    }), tsUs);
    this._lastEventTsUs = tsUs;
    if (rotated) this._onChunkRotated(tsUs);
  };
  this.recordFilterArgsUpdate = function(filterIdx, argName, newValue) {
    const argsTsUs = sys6.clock_us();
    const rotated = this.writer.writeEvent(JSON.stringify({
      version: EVENT_VERSION,
      message: "filter_args_update",
      ts_us: argsTsUs,
      payload: { filter_idx: filterIdx, arg_name: argName, value: newValue }
    }), argsTsUs);
    this._lastEventTsUs = argsTsUs;
    if (rotated) this._onChunkRotated(argsTsUs);
  };
  this.recordLogConfigChanged = function(logLevel) {
    const tsUs = sys6.clock_us();
    this.writer.writeLog(JSON.stringify({
      version: EVENT_VERSION,
      message: "log_config_changed",
      ts_us: tsUs,
      logLevel
    }), tsUs);
  };
  this.handleLog = function(tool, level, message, thread_id, caller) {
    this.pendingLogs.push({
      timestamp: sys6.clock_us(),
      tool,
      level,
      message: message?.length > MAX_LOG_MESSAGE_LENGTH ? message.substring(0, MAX_LOG_MESSAGE_LENGTH) + "..." : message,
      thread_id,
      caller: caller?.idx !== void 0 ? caller.idx : caller?.name || null
    });
    if (!this.logBatchTimer) {
      this.logBatchTimer = true;
      session.post_task(() => {
        this.flushLogs();
        return false;
      });
    }
  };
  this.flushLogs = function() {
    if (this.pendingLogs.length) {
      const tsUs = sys6.clock_us();
      this.writer.writeLog(JSON.stringify({
        version: EVENT_VERSION,
        message: "log_batch",
        ts_us: tsUs,
        logs: this.pendingLogs
      }), tsUs);
      this.pendingLogs.forEach((log) => {
        if (log.level !== LOG_LEVEL_ERROR && log.level !== LOG_LEVEL_WARNING) return;
        this.writer.recordJournalFact(log.timestamp, log.level);
      });
      this.pendingLogs = [];
    }
    this.logBatchTimer = null;
  };
  this.sendToClient = function(data) {
    if (data.message === "log_config_changed") {
      this.recordLogConfigChanged(data.logLevel);
    }
  };
  this.close = function() {
    logHub.remove(LOG_ID);
    this.flushLogs();
    this.writer.close();
  };
  if (!this.enabled) {
    this.snapshotWritten = true;
    for (const key in this) {
      if (typeof this[key] === "function") this[key] = () => {
      };
    }
  }
}

// server/GraphManager.js
import { Sys as sys8 } from "gpaccore";

// server/history/SnapshotBuilder.js
import { Sys as sys7 } from "gpaccore";
function SnapshotBuilder() {
  this.pidCollector = new PidDataCollector();
  this.buildFilterEntry = function(f) {
    const entry = gpac_filter_to_object(f, true);
    delete entry.ipid;
    delete entry.opid;
    const minimal = gpac_filter_to_minimal_object(f);
    entry.ipids = minimal.ipid;
    entry.opids = minimal.opid;
    entry.properties = {
      ipids: this.pidCollector.collectInputPids(f, true),
      opids: this.pidCollector.collectOutputPids(f)
    };
    return entry;
  };
  this.build = function(graphVersion, commandLine) {
    const filters = [];
    session.lock_filters(true);
    for (let i = 0; i < session.nb_filters; i++) {
      const f = session.get_filter(i);
      if (!f.is_destroyed()) filters.push(this.buildFilterEntry(f));
    }
    session.lock_filters(false);
    return {
      version: 1,
      ts_us: sys7.clock_us(),
      command_line: commandLine,
      graph_v: graphVersion,
      filters,
      session_metrics: session.session_metrics || null
    };
  };
}

// server/GraphManager.js
var GRAPH_DEBOUNCE_US = 500 * 1e3;
var GRAPH_MAX_WAIT_US = 3e3 * 1e3;
function GraphManager(deps) {
  const { getClients, historyCollector: historyCollector2, ensureMonitoringLoop: ensureMonitoringLoop2 } = deps;
  let graphVersion = 0;
  let graphDirty = false;
  let lastGraphEventTime = 0;
  let graphBuildStartTime = 0;
  let debounceRunning = false;
  const snapshotBuilder = new SnapshotBuilder();
  this.getGraphVersion = function() {
    return graphVersion;
  };
  this.onGraphEvent = function() {
    const now = sys8.clock_us();
    graphDirty = true;
    lastGraphEventTime = now;
    if (!graphBuildStartTime) graphBuildStartTime = now;
    ensureMonitoringLoop2();
    if (debounceRunning) return;
    debounceRunning = true;
    session.post_task(() => {
      const now2 = sys8.clock_us();
      const sinceLast = now2 - lastGraphEventTime;
      const sinceBuildStart = now2 - graphBuildStartTime;
      const graphStable = sinceLast >= GRAPH_DEBOUNCE_US;
      const maxBuildWaitReached = sinceBuildStart >= GRAPH_MAX_WAIT_US;
      if (graphStable || maxBuildWaitReached) {
        this._stabilize();
        debounceRunning = false;
        graphBuildStartTime = 0;
        return false;
      }
      return 100;
    });
  };
  this._stabilize = function() {
    graphDirty = false;
    graphVersion++;
    on_all_connected((allFilterInstances) => {
      session.lock_filters(true);
      const filters = allFilterInstances.map((f) => gpac_filter_to_minimal_object(f));
      session.lock_filters(false);
      if (!historyCollector2.snapshotWritten) {
        let commandLine = null;
        try {
          commandLine = sys8.args ? sys8.args.join(" ") : null;
        } catch (_e) {
        }
        const snapshot = snapshotBuilder.build(graphVersion, commandLine);
        historyCollector2.writeSnapshot(snapshot);
      }
      historyCollector2.recordGraph(filters, allFilterInstances, graphVersion);
      const filtersMsg = JSON.stringify({ message: "filters", filters });
      const notifMsg = JSON.stringify({
        message: "notification",
        type: "graph_changed",
        graphVersion
      });
      for (const client of getClients()) {
        if (client.client) {
          client.client.send(filtersMsg);
          client.client.send(notifMsg);
        }
      }
    });
  };
}

// server/JSClient/Session/buildSessionStatsPayload.js
function buildSessionStatsPayload(session2, fields) {
  const resolvedFields = fields || DEFAULT_FILTER_FIELDS;
  const stats = [];
  const filters = [];
  session2.lock_filters(true);
  for (let i = 0; i < session2.nb_filters; i++) {
    const f = session2.get_filter(i);
    if (f.is_destroyed()) continue;
    filters.push(f);
    const obj = {};
    for (const field of resolvedFields) obj[field] = f[field];
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
  let allFiltersEos = filters.length > 0;
  for (const f of filters) {
    if (f.nb_ipid === 0) continue;
    for (let i = 0; i < f.nb_ipid; i++) {
      if (!f.ipid_props(i, "eos")) {
        allFiltersEos = false;
        break;
      }
    }
  }
  const all_packets_done = session2.last_task && allFiltersEos;
  session2.lock_filters(false);
  return { all_packets_done, stats };
}

// server/server.js
var recordPath = sys9.get_opt("core", "rmt-log");
var historyCollector = new HistoryCollector(recordPath);
print(recordPath ? `[History] Recording enabled -> ${recordPath}` : "[History] Recording disabled (no -rmt-log)");
var all_clients = [];
var cid = 0;
var filter_uid = 0;
var all_filters = [];
var graphManager = new GraphManager({
  getClients: () => all_clients,
  historyCollector,
  ensureMonitoringLoop
});
var monitoringRunning = false;
function ensureMonitoringLoop() {
  if (monitoringRunning) return;
  monitoringRunning = true;
  session.post_task(() => {
    const now = sys9.clock_us();
    if (session.last_task) {
      for (const client of all_clients) client.sessionManager.handleSessionEnd(now);
      historyCollector.recordSessionStats(buildSessionStatsPayload(session), true);
      historyCollector.recordCpuStats(buildCpuStatsPayload());
      historyCollector.close();
      monitoringRunning = false;
      return false;
    }
    let active = false;
    let interval = 1e3;
    for (const client of all_clients) {
      client.sessionManager.tick(now);
      if (client.sessionManager.hasActiveSubscriptions()) {
        active = true;
        interval = Math.min(interval, client.sessionManager.getMinInterval());
      }
    }
    historyCollector.recordSessionStats(buildSessionStatsPayload(session));
    historyCollector.recordCpuStats(buildCpuStatsPayload());
    return active ? interval : 1e3;
  });
}
session.reporting(true);
historyCollector.startLogCapture();
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
  if (f.itag == "NODISPLAY") return;
  graphManager.onGraphEvent();
});
session.set_del_filter_fun((f) => {
  let idx = all_filters.indexOf(f);
  if (idx >= 0) all_filters.splice(idx, 1);
  if (f.itag == "NODISPLAY") return;
  graphManager.onGraphEvent();
});
var pidCollector = new PidDataCollector();
var pidReconfigured = /* @__PURE__ */ new Set();
session.set_filter_pid_modified_fun((f) => {
  pidReconfigured.add(f.idx);
  if (pidReconfigured.size > 1) return;
  session.post_task(() => {
    const indexes = [...pidReconfigured];
    pidReconfigured.clear();
    const pidsByFilter = {};
    for (const idx of indexes) {
      const filter = all_filters.find((f2) => f2.idx === idx);
      if (filter) pidsByFilter[idx] = pidCollector.collectInputPids(filter, true);
    }
    const msg = JSON.stringify({ message: "filter_pid_reconfigured", indexes });
    for (const c of all_clients) if (c.client) c.client.send(msg);
    historyCollector.recordPidReconfigured(indexes, pidsByFilter);
    return false;
  });
});
var argUpdated = /* @__PURE__ */ new Set();
session.set_filter_arg_updated_fun((f) => {
  argUpdated.add(f.idx);
  if (argUpdated.size > 1) return;
  session.post_task(() => {
    const indexes = [...argUpdated];
    argUpdated.clear();
    const argsByFilter = {};
    for (const idx of indexes) {
      const filter = all_filters.find((f2) => f2.idx === idx);
      if (filter) argsByFilter[idx] = filter.all_args(true).filter(Boolean);
    }
    const msg = JSON.stringify({ message: "filter_arg_updated", indexes });
    for (const c of all_clients) if (c.client) c.client.send(msg);
    historyCollector.recordArgUpdated(indexes, argsByFilter);
    return false;
  });
});
sys9.rmt_on_new_client = function(client) {
  let js_client = new JSClient(++cid, client, all_clients, ensureMonitoringLoop, historyCollector);
  all_clients.push(js_client);
  js_client.sendMonitorConfig();
  js_client.client.on_data = (msg) => {
    if (typeof msg == "string")
      js_client.on_client_data(msg);
  };
  js_client.client.on_close = function() {
    js_client.cleanup();
    remove_client(js_client.id);
    js_client.client = null;
  };
};
