# GPAC MCP Server

An MCP (Model Context Protocol) server that provides tools for analyzing and processing multimedia content using GPAC.

## Building the Server

You can build the MCP server either using Docker or manually with Node.js.

### Option 1: Docker Build (Recommended)

Build the Docker image from the project root:

```bash
cd /path/to/gpac_public
docker build -t gpac-mcp-server -f share/mcp/Dockerfile share/mcp
```

The Docker image includes both the MCP server and GPAC binaries (MP4Box).

### Option 2: Manual Build

**Prerequisites:**

- Node.js (v20 or higher)
- GPAC installed and available in your PATH (MP4Box command)

**Build steps:**

1. Install dependencies:

```bash
cd share/mcp
npm install
```

2. Build the TypeScript code:

```bash
npm run build
```

This will compile the TypeScript files to the `dist/` directory.

## Configuration

### VS Code (Copilot/Claude)

Add this configuration to your VS Code settings. For workspace-specific settings, add to `.vscode/mcp.json`:

#### Docker-based configuration:

```json
{
  "github.copilot.chat.mcp": {
    "servers": {
      "gpac-mcp-server": {
        "type": "stdio",
        "command": "docker",
        "args": [
          "run",
          "--rm",
          "-i",
          "-v",
          "${workspaceFolder}:${workspaceFolder}",
          "gpac-mcp-server"
        ]
      }
    }
  }
}
```

#### Manual build configuration:

```json
{
  "github.copilot.chat.mcp": {
    "servers": {
      "gpac-mcp-server": {
        "type": "stdio",
        "command": "node",
        "args": ["${workspaceFolder}/share/mcp/dist/index.js"],
        "dev": {
          "watch": "${workspaceFolder}/share/mcp/dist/index.js",
          "debug": { "type": "node" }
        }
      }
    }
  }
}
```

## Available Tools

Once configured, the MCP server provides tools for:

- Analyzing ISOBMFF (MP4/MOV) file structures
- Inspecting media files for errors and format details
- Processing multimedia content with GPAC

Refer to the tool descriptions in your LLM client for specific capabilities and usage.
