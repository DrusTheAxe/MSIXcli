# Security Policy

## Supported Versions

Security fixes are provided for the latest release and the current default branch.

| Version | Supported |
| ------- | --------- |
| Latest release | Yes |
| Default branch | Yes |
| Older releases | No |

Users should reproduce reported issues with the latest available version before submitting a
report.

## Reporting a Vulnerability

Do not report security vulnerabilities through public GitHub issues, discussions, or pull
requests.

Use GitHub private vulnerability reporting:

1. Open the repository's **Security** tab.
2. Select **Advisories**.
3. Select **Report a vulnerability**.

Include as much of the following information as possible:

* The affected version or commit
* The affected executable or component
* Windows version and system architecture
* Steps required to reproduce the issue
* A proof of concept, logs, or relevant diagnostic information
* The potential security impact
* Any known mitigations or workarounds

Remove credentials, personal information, signing keys, package contents, and other sensitive data
from submitted diagnostics.

## Response Process

Reports will be reviewed on a best-effort basis. You should normally receive an acknowledgment
within seven days.

After confirming a vulnerability, the project maintainer will:

* Assess its severity and affected versions
* Develop and test an appropriate correction
* Coordinate disclosure with the reporter
* Publish a security advisory when appropriate
* Credit the reporter unless anonymity is requested

Please allow reasonable time for investigation and remediation before publicly disclosing the
vulnerability.

## Scope

Examples of relevant security issues include:

* Unintended privilege escalation
* Unsafe handling of MSIX packages or package metadata
* Arbitrary code execution
* Command or argument injection
* Path traversal or unsafe file operations
* Improper permission or access-control handling
* Loading untrusted libraries or executable content
* Disclosure of sensitive information

The following are generally outside the scope of this policy:

* Vulnerabilities in unsupported versions
* Issues that require an already-compromised administrator account
* Social engineering or physical attacks
* Denial-of-service reports without meaningful security impact
* Vulnerabilities exclusively within third-party dependencies that are already publicly known
* Reports produced only by automated scanners without a reproducible security impact

## Safe Harbor

Security research conducted in good faith is welcome. Researchers must:

* Avoid accessing, modifying, or deleting data belonging to others
* Avoid disrupting systems or services
* Access only the minimum information necessary to demonstrate the issue
* Report findings privately and provide reasonable remediation time
* Comply with applicable laws

Good-faith research that follows this policy will not result in legal action initiated by this
project.
