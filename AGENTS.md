# Project guardrails

- Access files only within this project folder unless the user has explicitly approved a specific exception in the conversation. Do not inspect parent folders or external skills/settings without that approval.
- Obtain approval before acquiring any files or code from the internet. Download approved material into this project before using it.
- Keep project outputs, scripts, backups, and reports inside this folder.
- Approval granted for Step 1: read/execute the installed UE 5.8, Visual Studio/MSVC, and Windows SDK; allow their normal settings/cache/temp access for build and launch checks. This does not authorize internet downloads or unrelated external files.
- Approval also granted to locate and run installed Git. Keep the repository local, do not configure remotes or make remote connections, and do not read user Git configuration or other repositories.
- Obtain separate approval for additional external tools. Record approved exceptions in `doc/step1-setup.md`.
