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
  "This filter generates text streams based on the provided [-src]() file. By default, the filter uses a lipsum text file\n" +
    "When the [-unit]() is set to 'w', the filter will generate text based on words. When set to 'l', the filter will generate text based on lines\n" +
    "The [-fps]() parameter sets the frame rate of the text stream. As in 1/2, the duration of each text frame is 2 seconds. " +
    "Total duration of the text stream is set by the [-dur]() parameter. If set to 0/0, the text stream will be infinite\n" +
    "The [-acc]() parameter accumulates text until a line is composed (ignored if unit is line). If set, the filter will accumulate text until a line is composed or an empty line is seen in the source\n" +
    "The [-rollup]() parameter enables roll-up mode up to the specified number of lines. In roll-up mode, the filter will accumulate text until the specified number of lines is reached.\n" +
    "When the number of lines is reached, the filter will remove the first line and continue accumulating text\n" +
    "You would use [-rollup]() in combination with [-unit]() set to 'l' to create a roll-up subtitle effect. Or set [-unit]() to 'w' to create a roll-up text effect"
);

filter.set_arg({
  name: "src",
  desc: "source of text. If not set, the filter will use lorem ipsum text",
  type: GF_PROP_STRING,
  def: filter.jspath + "/lipsum.txt",
});
filter.set_arg({
  name: "unit",
  desc: "minimum unit of text from the source\n- w: word\n- l: line",
  type: GF_PROP_UINT,
  def: "l",
  minmax_enum: "w|l",
});
filter.set_arg({
  name: "fps",
  desc: "frame rate of the text stream",
  type: GF_PROP_FRACTION,
  def: "1/2",
});
filter.set_arg({
  name: "dur",
  desc: "duration of the text stream",
  type: GF_PROP_FRACTION,
  def: "0/0",
});
filter.set_arg({
  name: "acc",
  desc: "accumulate text until a line is composed (ignored if unit is line). If set, the filter will accumulate text until a line is composed or an empty line is seen in the source",
  type: GF_PROP_BOOL,
  def: false,
});
filter.set_arg({
  name: "rollup",
  desc: "enables roll-up mode up to the specified number of lines",
  type: GF_PROP_UINT,
  def: 0,
});

let text_cts = 0;
let text_pid = null;

let text = [];
let text_idx = 0;
let text_playing = false;

filter.frame_pending = 0;

filter.initialize = function () {
  this.set_cap({ id: "StreamType", value: "Text", output: true });
  this.set_cap({ id: "CodecID", value: "txt", output: true });

  let gpac_help = sys.get_opt("temp", "gpac-help");
  let gpac_doc = sys.get_opt("temp", "gendoc") == "yes" ? true : false;
  if (gpac_help || gpac_doc) return;

  if (filter.fps.n <= 0) {
    print(GF_LOG_ERROR, "Text duration cannot be 0 or negative");
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

  //load lipsum text
  let file = new File(filter.src, "r");
  let size = file.size;
  while (!file.eof) {
    let line = file.gets();
    if (line) {
      if (line.trim().length == 0) {
        text.push(null);
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
        print(accumulatedText);
        newText.push(accumulatedText.trim());
      }
    }
    newText.push(null);
    text = newText;
  }

  //subtitle at every N ms
  let bitrate = Math.floor(
    ((size / text.length) * 8) / (filter.fps.d / filter.fps.n)
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

let acc_text = "";
let acc_cts = 0;

filter.process = function () {
  if (!text_playing) return GF_EOS;
  if (!text_pid || text_pid.would_block) return GF_OK;

  // handle accumulation
  let unit_dur = Math.floor((1000 * filter.fps.d) / filter.fps.n);
  let unit = text[text_idx++ % text.length];
  if (filter.acc && filter.unit == 0) {
    if (unit != null) {
      if (acc_text.length) acc_text += " ";
      acc_text += unit;
      acc_cts += unit_dur;
      return GF_OK;
    }
  } else if (unit == null) {
    text_cts += unit_dur;
    return GF_OK;
  }

  //prepare packet
  let text_to_send = acc_text.length ? acc_text : unit;
  let pck = text_pid.new_packet(text_to_send.length);
  let farray = new Uint8Array(pck.data);
  for (let i = 0; i < text_to_send.length; i++) {
    farray[i] = text_to_send.charCodeAt(i);
  }
  let dur = acc_cts || unit_dur;
  pck.cts = text_cts;
  pck.dur = dur;
  pck.sap = GF_FILTER_SAP_1;

  //update text cts and reset acc
  text_cts += dur;
  acc_text = "";
  acc_cts = 0;

  //send packet
  pck.send();

  //put a pause during acc
  if (filter.acc) text_cts += unit_dur;

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
