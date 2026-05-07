#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { runCommand, runGpacCommand } from "./utils.js";

// Create an MCP server
const server = new McpServer({
  name: "gpac-mcp-server",
  version: "0.1.0",
});

server.registerTool(
  "inspect",
  {
    title: "File Inspection Tool",
    description:
      "Inspects a media file (such as MP4 or ISOBMFF) for possible issues, including format errors, missing boxes, or warnings. If no issues are found, returns detailed technical information about the file's structure, codecs, and metadata. Useful for validating file integrity and understanding file contents before further processing.",
    inputSchema: {
      file: z
        .string()
        .describe(
          "Absolute or relative path to the media file to inspect (e.g., '/path/to/file.mp4')."
        ),
    },
  },
  async ({ file }) =>
    runGpacCommand(
      "File Inspection Tool",
      `MP4Box -info -logs all@warning:ncl "${file}"`
    )
);

server.registerTool(
  "analyze-boxes",
  {
    title: "Box Analysis Tool",
    description:
      "Analyzes the box structure of an ISOBMFF (ISO Base Media File Format) file, such as MP4 or MOV. This tool parses the file and returns a detailed, structured report of all top-level and nested boxes (atoms), including their types, sizes, hierarchy, and selected contents. It helps developers and analysts understand the internal organization of the file, debug issues related to box structure, and verify compliance with ISOBMFF specifications. The output includes a hierarchical breakdown of boxes, their attributes, and any warnings or errors encountered during parsing.",
    inputSchema: {
      file: z
        .string()
        .describe(
          "Absolute or relative path to the ISOBMFF file (e.g., MP4, MOV) to analyze. Example: '/path/to/video.mp4'"
        ),
    },
  },
  async ({ file }) =>
    await runGpacCommand(
      "Box Analysis Tool",
      `MP4Box -disox -check-xml -std -logs all@warning:ncl "${file}"`,
      "xml"
    )
);

// Start receiving messages on stdin and sending messages on stdout
const transport = new StdioServerTransport();
await server.connect(transport);
