# Local Codex Rules

- On Windows, never open or write files under paths that may contain Chinese or other non-ASCII characters with narrow `char*` filesystem APIs. This includes `std::ofstream`, `std::ifstream`, `fopen`, and shell snippets that assume ANSI paths.
- For C++ file I/O to plugin paths such as `D:\UG...`, use wide-character Windows-compatible APIs, for example `_wfopen`, `CreateFileW`, `CreateDirectoryW`, or `std::filesystem::path` constructed from `std::wstring`.
- When adding logs, configs, manifests, or deployment writes, verify the file is actually created/read at the non-ASCII path before considering the task complete.
- If a file path is stored as UTF-8 text but passed to Win32 or CRT file APIs, convert it to UTF-16 first.
