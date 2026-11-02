# DyslexiReader

A native C++17 Windows desktop PDF viewer with free, offline text-to-speech.

## Run

For a standard installation, run dist/DyslexiReader-Setup-1.1.0-x64.exe. It installs for your Windows account, adds Start menu shortcuts, optionally adds a desktop shortcut, and registers PDF Open with support. Administrator access is unnecessary. Uninstall through Windows Settings > Apps > Installed apps > DyslexiReader. Windows lets you make the final choice of default PDF app.

To rebuild the installer, run build-installer.ps1. It builds the app and compiles installer/DyslexiReader.iss using Inno Setup 6.3 or newer. Pass -CompilerPath with the path to ISCC.exe if it is installed elsewhere. The installer is unsigned.

Launch build/DyslexiReader.exe. Keep pdfium.dll, PDFium-LICENSE.txt, and the licenses directory beside the executable when copying the application.

Open a PDF using **Open PDF**, **File > Open PDF**, Ctrl+O, drag and drop, or a PDF file association. The PDF is rendered in its original layout, as a continuous vertical document with a scrollbar. Multiple pages can be visible together.

- **Play** reads the entire document from the beginning when nothing is selected.
- Drag over text on the displayed page to select a passage; **Play** reads only that selection.
- **Pause** pauses the voice. **Play** resumes from the paused position.
- **Stop** cancels reading. The next Play starts the current selection or full document again.
- The spoken word is highlighted and the document scrolls automatically to keep it visible. Highlight updates use cached page images, partial invalidation, and an off-screen buffer to avoid flashing.
- **Read aloud > Slower voice / Faster voice** adjusts speech speed.
- Space toggles playback/pause; Esc clears the selection.
- The mouse wheel, scrollbar, arrow keys, and Page Up/Page Down scroll through the document. Previous/Next jumps to a page. Home/End jumps to the beginning/end.
- To change the passage during playback, Stop, select the new passage, and Play.

## Associate PDFs in Windows

First place the application folder in a permanent location. Run **File > Register PDF Open with option**. This registers the executable for the current Windows user without administrator privileges.

Right-click a PDF, choose **Open with > Choose another app**, select **DyslexiReader**, and choose **Always**. If Windows does not list it immediately, browse to DyslexiReader.exe. Windows controls the final choice of default app. Registration does not overwrite your current PDF default.

Command-line opening supports quoted paths with spaces:

    DyslexiReader.exe "C:\Documents\My document.pdf"

To remove the optional Open with registration, run unregister.ps1 in PowerShell. This removes only DyslexiReader's own per-user registration; choose a different PDF default in Windows if needed.

## Build with Visual Studio

Open DyslexiReader.sln in Visual Studio 2022 with **Desktop development with C++** installed. Select **Release / x64** and Build Solution. Or run:

    powershell -ExecutionPolicy Bypass -File .\build.ps1

The project uses the Windows SDK, Win32, SAPI, and bundled PDFium. ATL, Qt, paid services, API keys, and a network connection at runtime are unnecessary. The C++ runtime is statically linked.

Bundled PDFium: bblanchon/pdfium-binaries release chromium/8125, Windows x64. Original LICENSE and third-party notices are preserved under third_party/pdfium and copied to the output. [Binary source](https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F8125). Update this dependency for future PDFium security fixes.

## Checks

Build the integration executable using the same MSBuild command as build.ps1, adding /p:SelfTest=true. Run build/DyslexiReader-tests.exe from the project directory. It writes build/test-results.txt and generates build/sample.pdf. Checks exercise PDF creation/loading/rendering, character hit testing, selection extraction and position mapping, installed SAPI speech, pause/resume/stop, actual word events, completion, and following speech across pages. Speech is muted during these checks. Exit code 0 means success.

## Current limits

- Scanned image-only PDFs need OCR first; OCR is not included. In mixed image/text documents, only extractable text is spoken.
- Selection can span pages. Drag toward the top or bottom of the document area to scroll while selecting.
- Reading order follows the PDF text stream. Complex columns, tables, ligatures, unusual font encodings, and rotated text can affect reading order, selection, or highlighting.
- Highlight timing depends on the installed SAPI voice. The system's default SAPI voice is used; additional Windows voices may require installation.
- Password-protected files must be unlocked before opening.
- Use View > Zoom in / Zoom out or Ctrl+mouse wheel to change zoom. Zooming out shows more pages together. Wide pages fit the window.
- Text is collected before speech begins; very large documents may take time and memory during preparation.
- This initial application provides viewing and reading, without PDF editing or form interaction.

API references: [PDFium text API](https://pdfium.googlesource.com/pdfium/+/refs/heads/main/public/fpdf_text.h), [Windows SAPI voice](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ms720570%28v%3Dvs.85%29).


