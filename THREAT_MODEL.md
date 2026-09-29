# GPAC threat model

This document helps contributors reason about security impact in GPAC's media
processing and streaming workflows. [SECURITY.md](SECURITY.md) defines reporting
requirements and the supported version. The model describes potential trust
boundaries; each report still needs to establish its actual entry point,
deployment conditions, and consequence.

## Deployments and authority

`libgpac` powers `MP4Box`, the `gpac` filter tool, and embedded applications.
The [project overview](README.md) documents local files, network inputs,
packaging, playback, and configurable filter pipelines. Common situations to
consider are a person processing a file, an automated job processing supplied
media, and a client consuming a remote manifest and its segments. Embedded
applications may retain process state across operations and expose different
API call sequences. Establish the real workflow rather than assuming that a
library entry point has the same checks or exposure as a CLI command.

Operators and embedding applications choose commands, filters, paths, network
access, profiles, local scripts, modules, and DRM settings. Within those
choices, suppliers may control media payloads, references, manifest URLs, and
[scripts in supplied SVG](https://wiki.gpac.io/Player/SVG-Implementation-Status/);
remote endpoints and clients of enabled services control responses or requests.
Assets include process memory and control flow, media, local files and
configuration, DRM keys and application credentials, network services reachable
by the process, output integrity, and availability.

GPAC is not a process sandbox. Its [security discussion](https://gpac.io/2024/10/28/the-security-landscape-of-the-gpac-open-source-project-a-balanced-perspective/)
recommends sandboxing processing of untrusted media and keeping deployments
updated. Assess impact using the process's actual file permissions, isolation,
network reachability, and access to other users' data. A CVE label does not
establish deployment impact. An attacker-triggered crash establishes
process-level availability impact; assess whether it interrupts one invocation,
an automated job, or a shared service, and account for isolation and restart
behavior. A crash does not by itself establish code execution.

## Trust boundaries

| 3rd-party control | What GPAC uses | Possible consequence |
| --- | --- | --- |
| Supplier-controlled boxes, samples, item names, codec configuration, and subtitles | Native parsers and filters use process memory and produce files or streams | Memory corruption, unintended output, or disproportionate work |
| File or URL references in supplied media | The process resolves content-selected references with its file or network access | Unexpected local reads or outbound requests, data incorporated into output, or excess work, depending on deployment |
| Publisher-controlled MPD/HLS content and segment URLs; endpoint-controlled redirects and responses | The client uses network access, cache, credentials, and local output | Access beyond an established restriction, credential exposure, or output confusion |
| Supplier-controlled encryption metadata with operator-provided DRM configuration | Media processing associates tracks and samples with keys and protection settings | Clear output, wrong key association, or exposure of protected material |
| Remote connections or requests to enabled listener filters, including HTTP/RTSP | Connection handling reaches configured resources and service state | Unauthorized access or service disruption |
| Multicast objects accepted by an enabled gateway | Object handling reaches gateway state and configured outputs | Content substitution or service disruption |

Across these paths, supplied data can drive concurrent filters and error
teardown. Packet references and callbacks must remain valid across threads and
filter lifetimes; violations can corrupt state or stop a service.

Network services and gateways require explicit configuration; identify the
enabled mode, attacker access, and applicable permissions before claiming remote
exposure. Build options and optional dependencies change which parsers and
filters are reachable, so record the tested configuration and selected filter
graph. Revisit this model when a new input, authority boundary, or deployment
mode changes what untrusted parties can reach.

## Choosing and assessing findings

- Start proactive review with common MP4/ISOBMFF and DASH workflows. For an
  optional or older feature, show its enabled configuration and consequence;
  location or age alone does not make a feature unsupported.
- [SECURITY.md](SECURITY.md) says GPAC patches only reports confirmed on the
  current `master` HEAD. Provide the commit, input, and executable steps. For a
  library harness, explain production preconditions and any bypassed checks.
- For native memory defects, show the reachable violation and explain what it
  can affect. ASan is useful evidence; code execution need not be developed to
  justify a fix. For logic defects, demonstrate the forbidden access or output.
  For availability defects, measure input size, resource use, duration, and
  whether the effect is an isolated failure or disrupts a job or service.
- An isolated arithmetic warning or implausible value without a downstream
  consequence is not, by itself, a security finding. Arithmetic that causes
  unsafe allocation, memory access, or another observable failure remains
  relevant. Apply the same scrutiny to sanitizer labels and impact claims.
- In [issue #3558](https://github.com/gpac/gpac/issues/3558#issuecomment-4369657165),
  a maintainer declined one report of an external file read under ordinary
  process permissions. For content-selected file or network references, establish
  the process authority used, the operator's expected boundary, and whether
  referenced data or responses reach a lower-trust party.
