SAME BOY 1.0 - 4-channel PSG synthesizer (macOS, Universal: Apple Silicon + Intel)

EASY INSTALL
  Right-click "Same Boy 1.0 Installer.pkg" > Open (needed because the build is not notarized).
  It installs:
    VST3 -> /Library/Audio/Plug-Ins/VST3/Same Boy.vst3
    AU   -> /Library/Audio/Plug-Ins/Components/Same Boy.component

MANUAL INSTALL
  Copy "Same Boy.vst3" to ~/Library/Audio/Plug-Ins/VST3/
  Copy "Same Boy.component" to ~/Library/Audio/Plug-Ins/Components/
  Then run once in Terminal (clears the download quarantine flag):
    xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Same Boy.vst3" ~/Library/Audio/Plug-Ins/Components/"Same Boy.component"

"Same Boy.app" is a standalone version for quick testing.
