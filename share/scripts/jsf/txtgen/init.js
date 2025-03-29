/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Deniz Ugur
 *			Copyright (c) Motion Spell 2025
 *					All rights reserved
 *
 *  This file is part of GPAC / Text Generator filter
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

import { Sys as sys } from "gpaccore";
import { File as File } from "gpaccore";

filter.pids = [];

filter.set_name("txtgen");
filter.set_class_hint(GF_FS_CLASS_MM_IO);
filter.set_desc("Text Generator");
filter.set_version("1.0");
filter.set_author("GPAC Team");
filter.set_help(
  "This filter generates text streams based on the provided [-src]() file. By default, the filter uses a lorem ipsum text file\n" +
    "The [-type]() parameter sets the text generation mode. If set to 'txt', the filter will generate text based on the source file\n" +
    "If set to 'utc', the filter will generate text based on the current UTC time. If set to 'ntp', the filter will generate text based on the current NTP time\n" +
    "When the [-unit]() is set to 'w', the filter will generate text based on words. When set to 'l', the filter will generate text based on lines\n" +
    "The [-udur]() parameter sets the frame duration of the text stream. " +
    "Total duration of the text stream is set by the [-dur]() parameter. If set to 0/0, the text stream will be infinite\n" +
    "The [-rollup]() parameter enables roll-up mode up to the specified number of lines. In roll-up mode, the filter will accumulate text until the specified number of lines is reached.\n" +
    "When the number of lines is reached, the filter will remove the first line and continue accumulating text\n" +
    "You would use [-rollup]() in combination with [-unit]() set to 'l' to create a roll-up subtitle effect. Or set [-unit]() to 'w' to create a roll-up text effect.\n" +
    "The [-lmax]() parameter sets the maximum number of characters in a line. If the line in the source file is longer than this, the excess text will be wrapped. 0 means no limit\n" +
    "When [-rt]() is set to true, the filter will generate text in real-time. If set to false, the filter will generate text as fast as possible"
);

filter.set_arg({
  name: "src",
  desc: "source of text. If not set, the filter will use lorem ipsum text",
  type: GF_PROP_STRING,
  def: filter.jspath + "/lipsum.txt",
});
filter.set_arg({
  name: "type",
  desc: "type of text to generate\n- txt: plain text (uses src option)\n- utc: UTC time\n- ntp: NTP time",
  type: GF_PROP_UINT,
  def: "txt",
  minmax_enum: "txt|utc|ntp",
});
filter.set_arg({
  name: "unit",
  desc: "minimum unit of text from the source\n- w: word\n- l: line",
  type: GF_PROP_UINT,
  def: "l",
  minmax_enum: "w|l",
});
filter.set_arg({
  name: "udur",
  desc: "duration of each text unit",
  type: GF_PROP_FRACTION,
  def: "1/1",
});
filter.set_arg({
  name: "lmax",
  desc: "maximum number of characters in a line. If the line in the source file is longer than this, the excess text will be wrapped. 0 means no limit",
  type: GF_PROP_UINT,
  def: 32,
});
filter.set_arg({
  name: "dur",
  desc: "duration of the text stream",
  type: GF_PROP_FRACTION,
  def: "0/0",
});
filter.set_arg({
  name: "rollup",
  desc: "enable roll-up mode up to the specified number of lines",
  type: GF_PROP_UINT,
  def: 0,
});
filter.set_arg({
  name: "lock",
  desc: "lock timing to text generation",
  type: GF_PROP_BOOL,
  def: "false",
});
filter.set_arg({
  name: "rt",
  desc: "real-time mode",
  type: GF_PROP_BOOL,
  def: "true",
});

let text_cts = 0;
let text_pid = null;

let text = [];
let text_idx = 0;
let text_playing = false;
let dyn_rp_text = "";

let start_date = 0;
let utc_init = 0;
let ntp_init = 0;
let last_utc = 0;

filter.frame_pending = 0;

filter.initialize = function () {
  this.set_cap({ id: "StreamType", value: "Text", output: true });
  this.set_cap({ id: "CodecID", value: "txt", output: true });

  let gpac_help = sys.get_opt("temp", "gpac-help");
  let gpac_doc = sys.get_opt("temp", "gendoc") == "yes" ? true : false;
  if (gpac_help || gpac_doc) return;

  if (filter.udur.n <= 0) {
    print(GF_LOG_ERROR, "Unit duration cannot be 0 or negative");
    return GF_BAD_PARAM;
  } else if (filter.dur.n < 0) {
    print(GF_LOG_ERROR, "Stream duration cannot be negative");
    return GF_BAD_PARAM;
  }

  //setup text
  text_pid = this.new_pid();
  text_pid.set_prop("StreamType", "Text");
  text_pid.set_prop("CodecID", "txt");
  text_pid.set_prop("Unframed", true);
  text_pid.set_prop("Cached", true);
  text_pid.set_prop("Timescale", 1000);
  text_pid.name = "text";
  text_pid.set_prop("ID", 1);
  text_pid.set_prop("Bitrate", 1);

  //using UTC or NTP
  if (filter.type != 0) {
    if (filter.unit == 1) {
      print(
        GF_LOG_DEBUG,
        "UTC/NTP mode does not support line unit, switching to word unit"
      );
      filter.unit = 0; //since "line" is the default, don't warn
    }
    return;
  }

  //load lipsum text
  let file = new File(filter.src, "r");
  let size = file.size;
  while (!file.eof) {
    let line = file.gets();
    if (line) {
      if (line.trim().length == 0) {
        if (filter.unit == 1) text.push(null);
        continue;
      }

      if (filter.unit == 0) {
        let words = line.trim().split(" ");
        for (let i = 0; i < words.length; i++) {
          text.push(words[i]);
        }
      } else if (filter.unit == 1) {
        text.push(line.trim());
      }
    }
  }
  text.push(null);
  file.close();

  if (filter.lmax > 0) {
    let newText = [];
    let prevWarped = false;
    for (let i = 0; i < text.length; i++) {
      let cur = text[i];
      if (cur) {
        let words = cur.split(" ");
        let line = "";
        if (prevWarped && newText.length) {
          if (newText[newText.length - 1] != null) {
            line = newText.pop();
          }
        }
        for (let j = 0; j < words.length; j++) {
          if (line.length + words[j].length > filter.lmax) {
            if (!line.length) {
              print(
                GF_LOG_WARNING,
                "Encountered a word longer than lmax (" +
                  words[j].length +
                  " > " +
                  filter.lmax +
                  "), adjusting lmax"
              );
              filter.lmax = words[j].length + 1;
            } else {
              newText.push(line);
              line = "";
              prevWarped = true;
            }
          }
          if (line.length) line += " ";
          line += words[j];
        }
        newText.push(line);
      } else {
        newText.push(null);
      }
    }
    text = newText;
  }

  //rollup algorithm
  if (filter.rollup > 0) {
    let newText = [];
    let accumulatedText = "";
    let lineCount = 0;
    for (let i = 0; i < text.length; i++) {
      let cur = text[i];

      if (cur) {
        if (filter.unit == 0) {
          if (accumulatedText.length > 0) accumulatedText += " ";
          else lineCount++;
        }
        let lastLine = accumulatedText.lastIndexOf("\n") + 1;
        let lastLineLength = accumulatedText.length - lastLine;
        if (lastLineLength + cur.length > filter.lmax) {
          lineCount++;
          accumulatedText += "\n";
        }
        accumulatedText += cur;
      }
      if (cur == null || filter.unit == 1) {
        lineCount++;
        accumulatedText += "\n";
      }

      while (lineCount > filter.rollup) {
        //remove first line
        let idx = accumulatedText.indexOf("\n");
        accumulatedText = accumulatedText.substring(idx + 1);
        lineCount--;
      }

      //clear leading newlines
      while (accumulatedText.startsWith("\n")) {
        accumulatedText = accumulatedText.substring(1);
        lineCount--;
      }

      //clear trailing newlines
      while (accumulatedText.endsWith("\n\n")) {
        accumulatedText = accumulatedText.substring(
          0,
          accumulatedText.length - 1
        );
        lineCount--;
      }

      if (text.length - 1 != i) {
        newText.push(accumulatedText.trim());
      }
    }
    newText.push(null);
    text = newText;
  }

  //subtitle at every N ms
  let bitrate = Math.floor(
    ((size / text.length) * 8) / (filter.udur.n / filter.udur.d)
  );
  text_pid.set_prop("Bitrate", bitrate);
};

filter.process_event = function (pid, evt) {
  if (evt.type == GF_FEVT_STOP) {
    if (pid === text_pid) text_playing = false;
  } else if (evt.type == GF_FEVT_PLAY) {
    if (pid === text_pid) text_playing = true;
    filter.reschedule();
  }
};

function getTimes() {
  let date = new Date();
  let utc, ntp;
  if (filter.lock) {
    let elapsed_us = 1000 * text_cts;
    let elapsed_ms = Math.floor(elapsed_us / 1000);
    date.setTime(start_date + elapsed_ms);
    utc = utc_init + elapsed_ms;
    ntp = sys.ntp_shift(ntp_init, elapsed_us);
  } else {
    utc = sys.get_utc();
    ntp = sys.get_ntp();
  }
  return [utc, ntp];
}

filter.process = function () {
  if (!text_playing) return GF_EOS;
  if (!text_pid || text_pid.would_block) return GF_OK;

  //init times
  if (!start_date) {
    start_date = new Date().getTime();
    utc_init = sys.get_utc();
    ntp_init = sys.get_ntp();
  }

  //regulate text generation
  let interval_ms = Math.floor((1000 * filter.udur.n) / filter.udur.d);
  if (filter.rt) {
    if (last_utc && sys.get_utc() - last_utc < interval_ms) return GF_OK;
    last_utc = sys.get_utc();
    filter.reschedule(interval_ms);
  }

  //decide on unit
  let unit;
  if (filter.type == 0) {
    unit = text[text_idx++ % text.length];
  } else {
    let times = getTimes();
    if (filter.type == 1) {
      unit = new Date(times[0]).toUTCString();
    } else {
      unit = times[1].n + "." + times[1].d.toString(16);
    }
  }

  //handle gaps
  if (filter.rollup && filter.type > 0) {
    let lineCount = dyn_rp_text.split("\n").length;
    if (lineCount >= filter.rollup) {
      dyn_rp_text = dyn_rp_text.substring(dyn_rp_text.indexOf("\n") + 1);
    }
    if (dyn_rp_text.length) dyn_rp_text += "\n";
    dyn_rp_text += unit;
  } else if (unit == null) {
    text_cts += interval_ms;
    return GF_OK;
  }

  //prepare packet
  let text_to_send = dyn_rp_text.length ? dyn_rp_text : unit;
  let pck = text_pid.new_packet(text_to_send.length);
  let farray = new Uint8Array(pck.data);
  for (let i = 0; i < text_to_send.length; i++) {
    farray[i] = text_to_send.charCodeAt(i);
  }
  let dur = interval_ms;
  pck.cts = text_cts;
  pck.dur = dur;
  pck.sap = GF_FILTER_SAP_1;

  //update text cts and reset acc
  text_cts += dur;

  //send packet
  pck.send();

  //check if we should stop
  if (
    filter.dur.d &&
    text_cts >= Math.floor((1000 * filter.dur.n) / filter.dur.d)
  ) {
    print("done playing, cts " + text_cts);
    text_playing = false;
    text_pid.eos = true;
  }

  return GF_OK;
};
