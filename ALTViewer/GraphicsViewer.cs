using System.Diagnostics;
using System.Drawing.Imaging;

namespace ALTViewer
{
    public partial class GraphicsViewer : Form
    {
        private string gameDirectory = ""; // default directories
        private string gfxDirectory = "";
        private string paletteDirectory = "";
        private string enemyDirectory = "";
        private string languageDirectory = "";
        private string levelPath1 = "";
        private string levelPath2 = "";
        private string levelPath3 = "";
        private string levelPath4 = "";
        private string levelPath5 = "";
        private string levelPath6 = "";
        private string levelPath7 = "";
        private string[] levels = null!; // level directories
        private string lastSelectedFile = ""; // last selected file name for rendering and exporting
        private string lastSelectedPalette = ""; // last selected palette file path for rendering and exporting
        private string lastSelectedFilePath = ""; // last selected file path for rendering and exporting
        private int lastSelectedFrame = -1; // for reselecting the section after export
        private int lastSelectedSub = -1; // for reselecting the subframe after export
        private int lastSelectedSubFrame = -1; // to prevent selecting the same subframe and rendering twice
        private int lastSelectedSection = -1; // to prevent selecting the same section and rendering twice
        private string outputPath = ""; // output path for exported files
        private List<BndSection> currentSections = null!; // current sections for the selected file
        private byte[]? currentPalette; // current palette data for the selected file
        private byte[]? currentFrame; // current frame data for compressed files
        private bool palfile; // true if .PAL file is used ( BONESHIP, COLONY, LEGAL, LOGOSGFX, PRISHOLD )
        private bool compressed; // true if the file is compressed (e.g. enemies & weapons )
        private bool refresh; // set to true when entering the palette editor so that the image is refreshed when returning
        private bool exporting; // set to true when exporting everything
        private string exception = ""; // exception message for failed exports
        private static string[] removal = { "DEMO111", "DEMO211", "DEMO311", "PICKMOD", "OPTOBJ", "OBJ3D", "PANEL3GF", "PANELGFX" }; // unused demo files and models
        private static string[] duplicate = { "EXPLGFX", "FLAME", "MM9", "OPTGFX", "PULSE", "SHOTGUN", "SMART" }; // remove duplicate entries & check for weapons
        private static string[] weapons = { "FLAME", "MM9", "PULSE", "SHOTGUN", "SMART" }; // check for weapons
        private static string[] excluded = { "LEV", "GUNPALS", "SPRITES", "WSELECT", "PANEL", "NEWFONT", "MBRF_PAL" }; // excluded palettes
                                                                                    // PANEL & NEWFONT are used by the game but not required for rendering
        private int w = 256; // WIDTH
        private int h = 256; // HEIGHT
        private bool trimmed; // trim 96 bytes from the beginning of the palette for some files (e.g. PRISHOLD, COLONY, BONESHIP)
        private Point originalLocation = new Point(); // used to move the picture box during export as it is not needed during export and can cause flickering
                                                      // note that setting its visibility instead causes an artifact in the listbox
        public int[] transparentValues = null!; // transparent value ranges
        public bool bitsPerPixel = false; // true if 32bpp transparency is enabled
        public GraphicsViewer()
        {
            InitializeComponent();
            SetupDirectories();
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(ListBox), typeof(NumericUpDown) });
            string[] palFiles = Directory.GetFiles(paletteDirectory, "*" + ".PAL"); // Load palettes from the palette directory
            foreach (string palFile in palFiles)
            {
                string name = Path.GetFileNameWithoutExtension(palFile);
                if (!excluded.Any(e => name.Contains(e))) { listBox2.Items.Add(name); } // exclude unused palettes
            }
            ListFiles(gfxDirectory); // Load graphics files by default on startup
        }
        public void SetupDirectories()
        {
            gameDirectory = Utilities.CheckDirectory();     // File types used
            gfxDirectory = gameDirectory + "GFX";           // .BND + .B16
            paletteDirectory = gameDirectory + "PALS";      // .PAL
            enemyDirectory = gameDirectory + "NME";         // .B16
            languageDirectory = gameDirectory + "LANGUAGE"; // .16
            levelPath1 = gameDirectory + "SECT11";          // .B16
            levelPath2 = gameDirectory + "SECT12";          // .B16
            levelPath3 = gameDirectory + "SECT21";          // .B16
            levelPath4 = gameDirectory + "SECT22";          // .B16
            levelPath5 = gameDirectory + "SECT31";          // .B16
            levelPath6 = gameDirectory + "SECT32";          // .B16
            levelPath7 = gameDirectory + "SECT90";          // .B16
            levels = new string[] { levelPath1, levelPath2, levelPath3, levelPath4, levelPath5, levelPath6, levelPath7 };
        }
        // graphics GFX // .BND and .B16 files exist in the GFX folder which are used
        private void radioButton1_CheckedChanged(object sender, EventArgs e)
        {
            listBox1.Items.Clear();
            ListFiles(gfxDirectory, ".BND", ".B16", true);
        }
        // enemies NME // enemies are all compressed .B16 files
        private void radioButton2_CheckedChanged(object sender, EventArgs e)
        {
            listBox1.Items.Clear();
            ListFiles(enemyDirectory, ".B16", ".NOPE");
        }
        // levels SECT## // level graphics are all .B16 files
        private void radioButton3_CheckedChanged(object sender, EventArgs e)
        {
            listBox1.Items.Clear();
            foreach (string level in levels) { ListFiles(level); }
        }
        // panels LANGUAGE // .NOPE ignores the unused .BND files in the LANGUAGE folder
        private void radioButton4_CheckedChanged(object sender, EventArgs e)
        {
            listBox1.Items.Clear();
            ListFiles(languageDirectory, ".NOPE", ".16");
        }
        // list files in directory
        public void ListFiles(string path, string type1 = ".BND", string type2 = ".B16", bool enabled = false)
        {
            listBox2.Enabled = enabled; // enable or disable the palette list box based on the selected radio button
            foreach (string file in DiscoverFiles(path, type1, type2)) { listBox1.Items.Add(Path.GetFileNameWithoutExtension(file)); }
            if (radioButton1.Checked) // remove known unusable files
            {
                var counts = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
                var toRemove = new List<string>(); // Items to remove
                foreach (string item in listBox1.Items) // Count occurrences
                {
                    if (!counts.ContainsKey(item)) { counts[item] = 0; }
                    counts[item]++;
                }
                foreach (var file in removal) { if (listBox1.Items.Contains(file)) { toRemove.Add(file); } } // Add always-remove items
                foreach (var file in duplicate) // Add duplicate-only items
                {
                    if (counts.TryGetValue(file, out int count) && count > 1) { toRemove.Add(file); }
                }
                foreach (var file in toRemove) { listBox1.Items.Remove(file); } // Remove items
            }
            if (!exporting) { listBox1.SelectedIndex = 0; } // Select the first item in the list box if not exporting
        }
        // discover files in directory
        private string[] DiscoverFiles(string path, string type1 = ".BND", string type2 = ".B16")
        {
            return Directory.GetFiles(path, "*.*", SearchOption.AllDirectories).Where(file => file.EndsWith(type1) || file.EndsWith(type2)).ToArray();
        }
        // display selected file
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            string selected = listBox1.SelectedItem!.ToString()!; // get selected item
            if (selected == lastSelectedFile && !refresh) { return; } // do not reselect same file
            lastSelectedFile = selected; // store last selected file
            if (!exporting) // show controls if not exporting
            {
                label1.Visible = true; // show palette selection label
                label2.Visible = true; // show palette note 1
                label3.Visible = true; // show palette note 2
                label4.Visible = true; // show palette note 3
                listBox2.Visible = true; // show palette list
                button7.Visible = true; // show palette editor button
                button5.Enabled = true; // enable replace texture button
                checkBox1.Enabled = true; // enable backup checkbox
            }
            // determine which directory to use based on selected radio button
            if (radioButton1.Checked) { GetFile(gfxDirectory); }
            else if (radioButton2.Checked) { GetFile(enemyDirectory); }
            else if (radioButton3.Checked)
            {
                foreach (string level in levels) // determine level folder based on selected item
                {
                    if (File.Exists(level + "\\" + selected + ".B16"))
                    {
                        GetFile(level);
                        return; // exit after finding the first matching level
                    }
                }
            }
            else if (radioButton4.Checked) { GetFile(languageDirectory); }
        }
        // get the file from the selected directory then render it
        private void GetFile(string path)
        {
            string selected = listBox1.SelectedItem!.ToString()!; // get selected item
            string palettePath = paletteDirectory + "\\" + selected + ".PAL"; // actual palette path
            string filePath = "";
            foreach (string ext in new[] { ".16", ".B16", ".BND" }) // check file types in this order so that the cleanup script remains optional
            {
                string candidate = path + "\\" + selected + ext;
                if (File.Exists(candidate)) { filePath = candidate; break; }
            }
            if (File.Exists(filePath + ".BAK")) { button6.Enabled = true; } // check if a backup exists
            else { button6.Enabled = false; }
            if (string.IsNullOrEmpty(filePath)) { return; } // hacky workaround for race condition when selecting a file before the list is populated which should not be possible
            RenderImage(filePath, palettePath);
        }
        private void LoadPalette(string palettePath, bool trim, int start, int end, bool full)
        {
            listBox2.Enabled = true;
            listBox2.SelectedIndexChanged -= listBox2_SelectedIndexChanged!; // event handler removal to prevent rendering the image twice
            listBox2.SelectedItem = Path.GetFileNameWithoutExtension(palettePath);
            listBox2.SelectedIndexChanged += listBox2_SelectedIndexChanged!; // re-add the event handler
            lastSelectedPalette = palettePath; // store last selected file
            trimmed = trim; // set trimmed to true for these files
            if (full) { currentPalette = File.ReadAllBytes(palettePath); }
            else
            {
                byte[] loaded = File.ReadAllBytes(palettePath);
                currentPalette = new byte[768];
                Array.Copy(loaded, 0, currentPalette, start, end); // 96 padded bytes at the beginning for these palettes
            }
            palfile = true; // this is not always true so it gets reset if it is not a file using a palette
            compressed = false; // reset compressed to false for next detection
        }
        // loads and renders the selected image
        private void RenderImage(string binbnd, string palettePath)
        {
            pictureBox1.Image = null; // clear previous image
            lastSelectedSection = -1; // reset last selected section variable
            if (radioButton1.Checked)
            {
                if (weapons.Any(e => lastSelectedFile.Contains(e))) { UpdateChecks(true, true); }
                else if (binbnd.Contains("EXPLGFX") || binbnd.Contains("OPTGFX")) { UpdateChecks(true, false); }
                else if (palettePath.Contains("LOGOSGFX")) { LoadPalette(palettePath, false, 0, 576, false); }
                else if (palettePath.Contains("PRISHOLD") || palettePath.Contains("COLONY") || palettePath.Contains("BONESHIP")) { LoadPalette(palettePath, true, 96, 672, false); }
                else if (palettePath.Contains("LEGAL")) { LoadPalette(palettePath, false, 0, 0, true); }
                else { UpdateChecks(false, false); }
            }
            else if (radioButton2.Checked) { UpdateChecks(true, true); }
            else if (radioButton3.Checked || radioButton4.Checked) { UpdateChecks(false, false); }
            lastSelectedFilePath = binbnd;
            if (compressed) // load palette from level file or enemies
            {
                currentPalette = TileRenderer.Convert16BitPaletteToRGB(TileRenderer.ExtractEmbeddedPalette(binbnd, $"C000", 8));
                currentSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(binbnd), "F0");
                List<BndSection> decompressedF0Sections = new();
                int counter = 0;
                foreach (var section in currentSections)
                {
                    byte[] decompressedData;
                    try
                    {
                        decompressedData = TileRenderer.DecompressSpriteSection(section.Data); // Try decompressing individual F0 section
                        if (decompressedData.Length < 64) { throw new Exception("Data too small, likely not compressed"); } // Heuristic: If result is tiny, probably not valid
                    }
                    catch { decompressedData = section.Data; } // Fallback: Use raw data
                    counter++;
                    decompressedF0Sections.Add(new BndSection { Name = section.Name, Data = decompressedData }); // Store for UI
                }
                currentSections = decompressedF0Sections;
            }
            else { currentSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(binbnd), "TP"); }// Parse all sections (TP00, TP01, etc.)
            comboBox1.Items.Clear(); // Clear previous items in the ComboBox
            foreach (var section in currentSections) { comboBox1.Items.Add(section.Name); } // Populate ComboBox with section names
            if (comboBox1.Items.Count == 1) { comboBox1.Visible = false; } // hide combo box if there is only one section
            else { comboBox1.Visible = true; }
            if (!exporting) // trigger rendering if not exporting
            {
                comboBox1.SelectedIndex = 0;
                refresh = false; // reset refresh to false before any possible returns
            }
            void UpdateChecks(bool update, bool compression)
            {
                if (update) { binbnd = binbnd.Replace(".BND", ".B16"); }
                palfile = false; // reset palfile if not a file that uses external palettes
                listBox2.Enabled = false;
                compressed = compression; // set compressed to true for weapons
                trimmed = false; // reset trimmed to false
            }
        }
        // palette changed
        private void listBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            string selected = listBox2.SelectedItem!.ToString()!; // get selected item
            if (selected == lastSelectedPalette) { return; } // do not reselect same file
            lastSelectedPalette = selected; // store last selected file
            string palettePath = paletteDirectory + "\\" + selected + ".PAL";
            if (selected.Contains("LOGOSGFX")) { LoadPalette(palettePath, false, 0, 576, false); }
            else if (selected.Contains("PRISHOLD") || palettePath.Contains("COLONY") || palettePath.Contains("BONESHIP")) { LoadPalette(palettePath, true, 96, 672, false); }
            else if (selected.Contains("LEGAL")) { LoadPalette(palettePath, false, 0, 0, true); }
            checkBox2_CheckedChanged(null!, null!);
        }
        // export selected frame button
        private void button2_Click(object sender, EventArgs e)
        {
            ShowMessage($"Image saved to:\n{ExportFile(currentSections[comboBox1.SelectedIndex], comboBox1.SelectedItem!.ToString()!, true)}");
        }
        // export file
        private string ExportFile(BndSection section, string sectionName, bool single)
        {
            string filepath = Path.Combine(outputPath, $"{lastSelectedFile}");
            byte[] saving = null!;
            if (comboBox1.Items.Count != 1) { filepath = Path.Combine(outputPath, $"{lastSelectedFile}_{sectionName}"); }
            if (!compressed)
            {
                saving = section.Data; // use section data for non-compressed files
            }
            else
            {
                if (comboBox2.Items.Count != 1) { filepath = Path.Combine(outputPath, $"{lastSelectedFile}_{sectionName}_FRAME{comboBox2.SelectedIndex:D2}"); }
                saving = currentFrame!; // use current frame data for compressed files
                (w, h) = DetectDimensions.AutoDetectDimensions(lastSelectedFile, comboBox1.SelectedIndex, comboBox2.SelectedIndex);
                h = DetectDimensions.HeightFromData(w, h, saving); // ALTEngine's taller FINGERS frames
            }
            try
            {
                if (bitsPerPixel) { pictureBox1.Image.Save(filepath + ".png", ImageFormat.Png); } // save as PNG with 32bpp transparency
                else { TileRenderer.Save8bppPng(filepath + ".png", saving, TileRenderer.ConvertPalette(currentPalette!, transparentValues), w, h, transparentValues); }
                if (checkBox3.Checked) // export palettes
                {
                    filepath = Path.Combine(outputPath, $"{lastSelectedFile}");
                    if (!palfile && !compressed) // embedded palettes
                    {
                        filepath = filepath + $"_CL{comboBox1.SelectedIndex:D2}.PAL";
                        File.WriteAllBytes(filepath, currentPalette!);
                    }
                    else
                    {
                        if (comboBox1.SelectedIndex == comboBox1.Items.Count - 1 || single) // -1 to account for zero based indexing
                        {
                            filepath = filepath + ".PAL"; // external and compressed palettes
                            File.WriteAllBytes(filepath, currentPalette!);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                exception = ex.Message;
            }
            return filepath;
        }
        // export everything button click
        private void button1_Click(object sender, EventArgs e)
        {
            RadioButton[] buttons = { radioButton1, radioButton2, radioButton3, radioButton4 };
            int selectedIndex = Array.FindIndex(buttons, b => b.Checked);
            int previouslySelected = listBox1.SelectedIndex; // store previously selected index
            PreviouslySelectedFrames();
            exporting = true;
            foreach (var button in buttons)
            {
                button.Checked = true;                      // select each radio button
                for (int i = 0; i < listBox1.Items.Count; i++)
                {
                    listBox1.SelectedIndex = i;             // select each item in the list box
                    button3_Click(null!, null!);            // call the export all button click event
                }
            }
            buttons[selectedIndex].Checked = true;
            listBox1.SelectedIndex = previouslySelected;    // restore previously selected index
            RestoreSelectedFrames();
            exporting = false;
            ShowMessage($"All images saved to:\n{outputPath}");
        }
        // show message on successful export operation
        private void ShowMessage(string messageSuccess)
        {
            if (exception == "") { MessageBox.Show(messageSuccess); }
            else { MessageBox.Show("Failed to export : " + exception); }
        }
        // restore previously selected frames after export
        private void RestoreSelectedFrames()
        {
            comboBox1.SelectedIndex = lastSelectedFrame;    // restore previously selected index
            if (compressed) { comboBox2.SelectedIndex = lastSelectedSub; }
            pictureBox1.Location = originalLocation;
        }
        // setup previously selected indexes on export all frames or export everything
        private void PreviouslySelectedFrames()
        {
            originalLocation = pictureBox1.Location;
            lastSelectedFrame = (comboBox1.SelectedIndex == -1) ? 0 : comboBox1.SelectedIndex;
            if (compressed) { lastSelectedSub = comboBox2.SelectedIndex; }
            pictureBox1.Location = new Point(-9999, -9999); // move picture box off-screen to prevent it being drawn during export
        }
        // export all frames button
        private void button3_Click(object sender, EventArgs e)
        {
            if (!exporting) { PreviouslySelectedFrames(); }
            for (int i = 0; i < comboBox1.Items.Count; i++)
            {
                comboBox1.SelectedIndex = i; // select each section so that each sub frame is detected, selected and exported
                if (!compressed && !palfile) // update embedded palette for each frame
                {
                    currentPalette = TileRenderer.Convert16BitPaletteToRGB(TileRenderer.ExtractEmbeddedPalette(lastSelectedFilePath, $"CL{comboBox1.SelectedIndex:D2}", 12));
                }
                else if (compressed)
                {
                    for (int f = 0; f < comboBox2.Items.Count; f++)
                    {
                        comboBox2.SelectedIndex = f; // select each sub frame
                        ExportFile(null!, comboBox1.Items[i]!.ToString()!, false);
                    }
                }
                if (palfile || !compressed && !palfile) // export embedded palette images and external palette images
                {
                    ExportFile(currentSections[i], comboBox1.Items[i]!.ToString()!, false);
                }
            }
            if (!exporting) // restore previously selected index on export all frames
            {
                RestoreSelectedFrames();
                ShowMessage($"Images saved to:\n{outputPath}");
            }
        }
        // select output path
        private void button4_Click(object sender, EventArgs e)
        {
            using var fbd = new FolderBrowserDialog();
            fbd.Description = "Select output folder to save exported files.";
            if (fbd.ShowDialog() == DialogResult.OK)
            {
                outputPath = fbd.SelectedPath;
                textBox1.Text = outputPath; // update text box with selected path
                button2.Enabled = true; // enable extract button
                button3.Enabled = true; // enable extract all button
                button1.Enabled = true; // enable export all button
            }
        }
        // render the image when a section is selected
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox1.SelectedIndex == lastSelectedSection || comboBox1.SelectedIndex == -1) { return; }
            lastSelectedSection = comboBox1.SelectedIndex;
            transparentValues = DetectDimensions.TransparencyValues(lastSelectedFile, lastSelectedSection);
            try
            {
                if (!compressed)
                {
                    comboBox2.Visible = false;
                    label5.Visible = false;
                    if (!palfile) // update embedded palette to match selected frame
                    {
                        currentPalette = TileRenderer.Convert16BitPaletteToRGB(TileRenderer.ExtractEmbeddedPalette(lastSelectedFilePath, $"CL{lastSelectedSection:D2}", 12));
                    }
                    w = 256;
                    h = 256;
                    pictureBox1.Width = w;
                    pictureBox1.Height = h;
                    pictureBox1.Image = TileRenderer.RenderRaw8bppImage(currentSections[lastSelectedSection].Data, currentPalette!, w, h, transparentValues, bitsPerPixel);
                }
                else
                {
                    lastSelectedSubFrame = -1; // reset last selected sub frame index
                    comboBox2.Visible = true;
                    label5.Visible = true;
                    DetectFrames.ListSubFrames(lastSelectedFilePath, comboBox1, comboBox2);
                }
            }
            catch (Exception ex) { MessageBox.Show("Render failed A: " + ex.Message); }
        }
        // sub frame combo box index changed
        private void comboBox2_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (comboBox2.SelectedIndex == lastSelectedSubFrame) { return; } // still happens twice on keyboard up / down
            lastSelectedSubFrame = comboBox2.SelectedIndex; // store last selected sub frame index
            currentFrame = DetectFrames.RenderSubFrame(lastSelectedFilePath, comboBox1, comboBox2, pictureBox1, currentPalette!, transparentValues, bitsPerPixel); // render the sub frame
            if (comboBox2.Items.Count == 1)
            {
                comboBox2.Visible = false; // hide combo box if there is only one section
                label5.Visible = false; // hide label if there is only one sub frame
            }
            else
            {
                comboBox2.Visible = true; // show combo box if there is only one section
                label5.Visible = true; // show label if there are multiple sub frames
            }
            //DetectAfterRender(); // TODO : Keep this for future use
        }
        // replace button click event
        private void button5_Click(object sender, EventArgs e)
        {
            using (OpenFileDialog openFileDialog = new OpenFileDialog())
            {
                openFileDialog.Filter = "PNG Files (*.png)|*.png|All Files (*.*)|*.*";
                openFileDialog.FilterIndex = 1;
                openFileDialog.RestoreDirectory = true;
                openFileDialog.Title = "Select an image (.png) file";
                openFileDialog.Multiselect = true;
                if (openFileDialog.ShowDialog() == DialogResult.OK) { ReplaceTexture(openFileDialog.FileNames); }
            }
        }
        // replace texture
        private void ReplaceTexture(string[] filename)
        {
            int length = filename.Length;
            if (TryGetTargetPath(out string selectedFile, out string backupFile) && !File.Exists(backupFile) && checkBox1.Checked) { File.Copy(selectedFile, backupFile); }
            if (compressed)
            {
                if (length != 1)
                {
                    MessageBox.Show("Please select only one image when replacing a sub frame.");
                    return;
                }
                //DetectFrames.ReplaceSubFrame(lastSelectedFilePath, comboBox1, comboBox2, pictureBox1, filename[0]); // replace sub frame
                MessageBox.Show("Replacing compressed images is not supported yet.");
                return;
            }
            else
            {
                if (length == 1) { ReplaceFrame(comboBox1.SelectedIndex, "Texture frame replaced successfully.", true); } // replace single frame
                else if (length == currentSections.Count) // replace all frames
                {
                    for (int i = 0; i < length; i++) { ReplaceFrame(i, "All texture frames replaced successfully.", false); }
                    // CONSIDER : building a list of frames to replace : MICRO OPTIMISATION
                }
                else { MessageBox.Show($"Please select exactly {currentSections.Count} images to replace all frames."); return; }
            }
            void ReplaceFrame(int frame, string message, bool single)
            {
                int framestore = frame; // frame is the frame to be replaced
                if (single) { framestore = 0; } // get only one frame if single is true
                Bitmap frameImage;
                try { frameImage = new Bitmap(filename[framestore]); } // safety first...
                catch (Exception ex) { MessageBox.Show("Failed to load image:\n" + ex.Message); return; }
                if (!IsIndexed8bpp(frameImage.PixelFormat)) { MessageBox.Show("Image must be 8bpp indexed PNG."); return; }
                else if (!CheckDimensions(frameImage)) { return; }
                byte[] indexedData = TileRenderer.Extract8bppData(frameImage);
                currentSections[frame].Data = indexedData; // replace currently loaded data with the new indexed data
                List<Tuple<long, byte[]>> list = new() { Tuple.Create(TileRenderer.FindSectionDataOffset(selectedFile, $"TP{frame:D2}", 8), indexedData) };
                BinaryUtility.ReplaceBytes(list, selectedFile);
                if (frame + 1 == currentSections.Count || single) // account for zero based indexing
                {
                    MessageBox.Show(message);
                }
            }
            checkBox2_CheckedChanged(null!, null!); // force redraw and retain transparency settings plus selected indexes
            button6.Enabled = true; // enable restore backup button
        }
        // check if the image is indexed 8bpp
        public static bool IsIndexed8bpp(PixelFormat format) { return format == PixelFormat.Format8bppIndexed; }
        // check the image dimensions match the expected size
        private bool CheckDimensions(Bitmap frameImage)
        {
            if (compressed) { (w, h) = DetectDimensions.AutoDetectDimensions(lastSelectedFile, comboBox1.SelectedIndex, comboBox2.SelectedIndex); }
            else { w = 256; h = 256; }
            if (frameImage.Width == w && frameImage.Height == h) { return true; }
            MessageBox.Show($"Image dimensions do not match the expected size of {w}x{h} pixels.");
            return false;
        }
        // get the target path for the selected file
        private bool TryGetTargetPath(out string fullPath, out string backupPath)
        {
            string directory = "";
            string filetype = "";
            if (radioButton1.Checked)
            {
                directory = "GFX";
                filetype = lastSelectedFile switch { "EXPLGFX" or "FLAME" or "MM9" or "OPTGFX" or "PULSE" or "SHOTGUN" or "SMART" => ".B16", _ => ".BND" };
            }
            else if (radioButton2.Checked) { directory = "NME"; filetype = ".B16"; }
            else if (radioButton3.Checked)
            {
                directory = lastSelectedFile.Substring(0, 2) switch
                {
                    "11" or "12" or "13" => "SECT11",
                    "14" or "15" or "16" => "SECT12",
                    "21" or "22" or "23" => "SECT21",
                    "24" or "26" => "SECT22",
                    "31" or "32" or "33" => "SECT31",
                    "35" or "36" or "37" or "38" or "39" => "SECT32",
                    "90" => "SECT90",
                    _ => throw new Exception("Unknown section selected!")
                };
                MessageBox.Show(directory);
                filetype = ".B16";
            }
            else if (radioButton4.Checked) { directory = "LANGUAGE"; filetype = ".16"; }
            fullPath = $"{gameDirectory}\\{directory}\\{listBox1.SelectedItem}{filetype}";
            backupPath = fullPath + ".BAK";
            return true;
        }
        // restore backup click event
        private void button6_Click(object sender, EventArgs e)
        {
            if (!TryGetTargetPath(out string selectedFile, out string backupFile)) { return; }
            File.Copy(backupFile, selectedFile, true);
            File.Delete(backupFile);
            button6.Enabled = false;
            refresh = true;
            listBox1_SelectedIndexChanged(null!, null!); // re-render the image after restoring a backup
            MessageBox.Show("Backup successfully restored!");
        }
        // palette editor button click
        private void button7_Click(object sender, EventArgs e)
        {
            refresh = true;
            string choice = palfile ? lastSelectedPalette : lastSelectedFilePath; // use the last selected file path for embedded palettes or use the last selected palette
            newForm(new PaletteEditor(choice, palfile, currentSections, compressed, trimmed, transparentValues));
        }
        // create new form method
        private void newForm(Form form)
        {
            form.StartPosition = FormStartPosition.Manual;
            form.Location = this.Location;
            form.Show();
            this.Hide();
            form.FormClosed += (s, args) =>
            {
                this.Show();
                listBox1_SelectedIndexChanged(null!, null!); // Re-run selected palette loading logic and re-render image
            };
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
        }
        // checkBox1_CheckedChanged event handler for transparency checkbox
        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            lastSelectedFrame = (comboBox1.SelectedIndex == -1) ? 0 : comboBox1.SelectedIndex;
            if (compressed) { lastSelectedSub = comboBox2.SelectedIndex; }
            bitsPerPixel = checkBox2.Checked; // toggle bits per pixel transparency
            lastSelectedSection = -1; // reset last selected section variable
            comboBox1.SelectedIndex = -1; // reset combo box selection to force redraw
            comboBox1.SelectedIndex = lastSelectedFrame; // force redraw
            if (compressed) { comboBox2.SelectedIndex = lastSelectedSub; }
        }
        // double click to open output path
        private void textBox1_MouseDoubleClick(object sender, MouseEventArgs e)
        { if (outputPath != "") { Process.Start(new ProcessStartInfo() { FileName = outputPath, UseShellExecute = true, Verb = "open" }); } }
    }
}