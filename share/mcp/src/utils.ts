import { spawn } from "child_process";
import { mkdtempSync, readFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { CallToolResult } from "@modelcontextprotocol/sdk/types.js";

interface GPACResponse {
  exitCode: number;
  stderr: string;
  stdout: string;
}

export async function runCommand(command: string): Promise<GPACResponse> {
  const tmpDir = mkdtempSync(join(tmpdir(), "gpac-"));
  const outPath = join(tmpDir, "stdout.txt");
  const errPath = join(tmpDir, "stderr.txt");

  return new Promise<GPACResponse>((resolve, reject) => {
    const shellCommand = `(${command}) >"${outPath}" 2>"${errPath}"`;

    const child = spawn("sh", ["-c", shellCommand], {
      detached: true,
      stdio: "ignore",
    });

    child.on("error", (err) => {
      rmSync(tmpDir, { recursive: true, force: true });
      reject({ exitCode: 1, stdout: "", stderr: err.message });
    });

    child.on("exit", (code) => {
      let stdout = "";
      let stderr = "";
      try {
        stdout = readFileSync(outPath, "utf-8");
      } catch {}
      try {
        stderr = readFileSync(errPath, "utf-8");
      } catch {}

      rmSync(tmpDir, { recursive: true, force: true });

      resolve({
        exitCode: code ?? 1,
        stdout,
        stderr,
      });
    });

    child.unref(); // allow parent to exit independently
  });
}

interface ResponseCommon {
  title: string;
  stdout: { type: string; text: string };
  stderr: { type: string; text: string };
}

export function successfullResponse({
  title,
  stdout,
  stderr,
}: ResponseCommon): string {
  const response = `
  ${title} Result (DO NOT RETRY):

  The command has completed successfully. As a result, gpac has produced the following output:

  Standard Output:
  \`\`\`${stdout.type}
  ${stdout.text}
  \`\`\`

  Standard Error:
  \`\`\`${stderr.type}
  ${stderr.text}
  \`\`\`

  Please do not retry this command, as it has already been executed successfully. If you need to run the command again, please ensure that the input parameters are correct and that the environment is properly set up.
  `;
  return response;
}

export function errorResponse({
  title,
  stdout,
  stderr,
}: ResponseCommon): string {
  const response = `
  ${title} Error (DO NOT RETRY):

  The command has encountered an error. As a result, gpac or the execution environment has produced the following output:

  Standard Output:
  \`\`\`${stdout.type}
  ${stdout.text}
  \`\`\`

  Standard Error:
  \`\`\`${stderr.type}
  ${stderr.text}
  \`\`\`

  Please do not retry this command, as it has already been executed and resulted in an error. If you need to run the command again, please ensure that the input parameters are correct and that the environment is properly set up.
  `;
  return response;
}

export async function runGpacCommand(
  title: string,
  command: string,
  type: "text" | "xml" = "text"
): Promise<CallToolResult> {
  try {
    const response = await runCommand(command);
    if (response.exitCode !== 0) {
      return {
        content: [
          {
            type: "text",
            text: errorResponse({
              title,
              stdout: { type: "text", text: response.stdout },
              stderr: { type: "text", text: response.stderr },
            }),
          },
        ],
      };
    }
    return {
      content: [
        {
          type: "text",
          text: successfullResponse({
            title,
            stdout: { type, text: response.stdout },
            stderr: { type, text: response.stderr },
          }),
        },
      ],
    };
  } catch (error) {
    return {
      content: [
        {
          type: "text",
          text:
            error instanceof Error
              ? error.message
              : "An unknown error occurred.",
        },
      ],
    };
  }
}
