namespace ALTViewer
{
    public static class Utilities
    {
        public static bool UnsavedChanges(string reason, Action saveAction, FormClosingEventArgs? e = null)
        {
            var result = MessageBox.Show(
                $"You have unsaved changes. Do you want to save before {reason}?",
                "Unsaved Changes",
                MessageBoxButtons.YesNoCancel,
                MessageBoxIcon.Warning);

            if (result == DialogResult.Cancel)
            {
                if (e != null) { e.Cancel = true; }
                return true; // signal cancellation
            }
            else if (result == DialogResult.Yes) { saveAction(); } // delegate call to save

            return false;
        }
        public static string CheckDirectory()
        {
            if (File.Exists("Run.exe")) { return "HDD\\TRILOGY\\CD\\"; }
            else if (File.Exists("TRILOGY.EXE")) { return "CD\\"; }
            else if (File.Exists("ALTEngine.exe")) { return EngineGameDirectory(); }
            return "";
        }
        // Beside ALTEngine: the game files ALTEngine installed and patches. It keeps their folder (the one holding CD) in
        // altengine.cfg as GameDirectory - GameData beside it, unless the files were already an ALTEngine install
        // somewhere else - so that is read first, and GameData\CD is the fallback.
        private static string EngineGameDirectory()
        {
            if (File.Exists("altengine.cfg"))
            {
                foreach (string line in File.ReadAllLines("altengine.cfg"))
                {
                    if (!line.StartsWith("GameDirectory=")) { continue; }
                    string cd = Path.Combine(line.Substring("GameDirectory=".Length).Trim(), "CD");
                    if (Directory.Exists(Path.Combine(cd, "SECT11"))) { return cd + Path.DirectorySeparatorChar; }
                }
            }
            return "GameData\\CD\\";
        }
        // True when ALTViewer sits beside ALTEngine but ALTEngine has not installed the game files yet: ALTEngine has
        // to run first (it copies the game from the disc or an installation, rips the music and patches the files),
        // and ALTViewer then reads what it made.
        public static bool EngineNotInstalledYet(string gameDirectory)
        {
            if (!File.Exists("ALTEngine.exe") || File.Exists("Run.exe") || File.Exists("TRILOGY.EXE")) { return false; }
            return !Directory.Exists(Path.Combine(gameDirectory, "SECT11")) || !Directory.Exists(Path.Combine(gameDirectory, "GFX"));
        }
    }
}
