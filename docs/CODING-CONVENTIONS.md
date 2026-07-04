# Coding Conventions & Policies

Conventions and policies followed by the projects in this repository
(`MSIXPropertySheet`, a shell property-sheet extension DLL, and `MSIXAdmin`, an
elevated console helper). New code should match these.

## Language & toolchain

- **C++20** (`/std:c++20`), Visual C++ / MSBuild. Build via the VS toolchain
  (locate MSBuild with `vswhere`).
- **Warnings are errors.** Code must compile clean; do not introduce warnings
  (e.g. `wprintf` format mismatches, `MAX_PATH` truncation).
- **No C++ exceptions.** No `try/catch`, no throwing constructors. Errors flow
  through return values.
- **No STL.** Do not use `std::string`, `std::vector`, `std::move`, etc. Use
  `wistd::move`, raw fixed-size arrays, and `memcpy`/`memcmp`/`strcmp`
  (allowed; not STL containers). `std::ignore` and `<cstdint>` types
  (`std::uint64_t`) are used where convenient.
- **WIL-based, RAII-heavy.** Prefer Windows Implementation Library types over
  manual cleanup.
- **COM** is assumed initialized before signing/packaging APIs are called.

## Error handling

- Functions return `HRESULT`; success is `S_OK`. `S_FALSE` is used as a benign
  "nothing to do / not found / treat as absent" success sentinel (it passes
  `RETURN_IF_FAILED`). Win32-style helpers may return `ERROR_SUCCESS`/`LONG`.
- Use WIL macros instead of hand-rolled checks: `RETURN_IF_FAILED`,
  `RETURN_HR_IF`, `RETURN_HR_IF_NULL`, `RETURN_IF_NULL_ALLOC`,
  `RETURN_LAST_ERROR_IF`, `RETURN_LAST_ERROR_IF_NULL`,
  `RETURN_IF_WIN32_BOOL_FALSE`. Log-and-continue: `LOG_IF_FAILED`,
  `SUCCEEDED_LOG`, paired with `std::ignore` when discarding.
- For HRESULT→text use a nothrow `FormatMessageW` wrapper returning
  `wil::unique_hlocal_string` (`ALLOCATE_BUFFER | FROM_SYSTEM | IGNORE_INSERTS`).
  This WIL version has no `wil::GetErrorMessage`. Never `wistd::move` a returned
  prvalue (kills copy elision).

## WIL usage

- RAII: `wil::unique_cotaskmem_ptr<BYTE[]>`, `wil::make_unique_cotaskmem_nothrow`,
  `wil::unique_cotaskmem_string`, `wil::unique_hlocal_string`,
  `wil::com_ptr_nothrow`, `wil::unique_process_heap_ptr`, and crypto types
  `unique_hcertstore`, `unique_hcryptmsg`, `unique_cert_context`,
  `unique_cert_chain_context`.
- Strings: `wil::str_printf_nothrow<wil::unique_cotaskmem_string>(...)`,
  `wil::out_param` / `wil::out_param_ptr`, `wil::GetModuleFileNameW` (handles
  long paths > `MAX_PATH`).

## Style

- Small, single-purpose helper functions; orchestrators delegate to them.
- Header-only classes in `namespace MSIX` (e.g. `MSIX::Package`,
  `MSIX::Signing`); private static helpers below the public surface.
- Naming: file-scope/static constants `c_camelCase`; members `m_camelCase`;
  static lookup tables `s_name`; brace-init with explicit types
  (`const HRESULT hr{ ... }`).
- Comment only what needs clarification; keep error/sentinel intent noted at
  the `return`. Cite external sources (e.g. microsoft/msix-packaging) for
  hard-coded constants. Every file starts with the MIT copyright header.
- Verify changes by compiling. The DLL is loaded by `explorer.exe` (lock), so
  validate with `msbuild /t:ClCompile` and `rc.exe` rather than a full link.

## Security & elevation

- Never fabricate constants (e.g. cert root hashes); only verified values, and
  rely on `CERT_CHAIN_POLICY_MICROSOFT_ROOT` for broad coverage.
- LocalMachine cert-store writes require admin: gate with an `IsRunningAsAdmin`
  check (`CheckTokenMembership` vs `WinBuiltinAdministratorsSid`) returning
  `E_ACCESSDENIED`. `MSIXAdmin` ships `requireAdministrator` via the linker
  `<UACExecutionLevel>` (not a hand-written manifest, which conflicts with the
  generated one). Default leaf installs to `LocalMachine\TrustedPeople`; do not
  auto-trust new CAs.

## Linking

- `AdditionalDependencies` = `onecore.lib;onecoreuap.lib` (covers crypt32 /
  bcrypt / version). Avoid adding redundant per-API `.lib`s.
