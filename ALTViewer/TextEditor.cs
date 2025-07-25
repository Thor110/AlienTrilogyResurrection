﻿using System.Text;

namespace ALTViewer
{
    public partial class TextEditor : Form
    {
        public bool setup;
        // instead of hard coding language names, dynamically read them from the executable at the relevant offsets
        // this way it supports user changes
        public string languageDirectory = "";
        public List<string> languages = new List<string> { "English", "Français", "Italiano", "Español" };
        public List<string> missions = new List<string>
        {
            "1.1.1 Entrance", "1.1.2 Outer Complex", "1.1.3 Ammunition Dump 1", "1.2.2 Recreation Rooms", "1.3.1 Medical Laboratory",
            "1.1.4 Ammunition Dump 2", "1.4.1 Garage", "1.1.5 Ammunition Dump 3", "1.5.4 Atmosphere Sub-level", "1.5.5 Security Catwalks",
            "1.6.1 Atmosphere Sub-Basement", "1.6.2 Queen's Lair", "2.1.1 Living Area", "2.1.2 Canteen", "2.1.3 Meeting Tower",
            "2.2.2 Leadworks", "2.4.2 Tunnels and Ducts", "2.3.1 Mining and Smelting", "2.3.2 Furnace Controls", "2.4.3 Tunnels and Ducts",
            "2.6.2 Lead Mould", "2.6.3 Queen's Lair", "3.1.1 Tunnels", "3.2.1 Pilot's Chamber", "3.3.1 Canyons and Catacombs",
            "3.2.2 Pilot's Chamber", "3.5.1 Secrets", "3.5.2 Inorganics 1", "3.2.3 Pilot's Chamber", "3.7.1 Droplifts",
            "3.5.3 Inorganics 2", "3.2.4 Pilot's Chamber", "3.8.1 Egg Chambers", "3.2.5 Pilot's Chamber", "3.9.1 Queen's lair"
        };
        // executable text offsets
        private long englishOffset = 0xB24D0;
        private long frenchOffset = 0xB24D8;
        private long italianOffset = 0xB24E4;
        private long spanishOffset = 0xB24F0;
        // executable text length limits
        private UInt16 englishLength = 7;
        private UInt16 frenchLength = 8;
        private UInt16 italianLength = 8;
        private UInt16 spanishLength = 7;
        public TextEditor()
        {
            InitializeComponent();
            languageDirectory = Utilities.CheckDirectory() + "LANGUAGE\\";
            ToolTip tooltip = new ToolTip();
            ToolTipHelper.EnableTooltips(this.Controls, tooltip, new Type[] { typeof(PictureBox), typeof(Label), typeof(ListBox) });
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            foreach (string language in languages) { comboBox1.Items.Add(language); }
            foreach (string mission in missions) { listBox1.Items.Add(mission); }
            comboBox1.SelectedIndex = 0; // Default to English
            setup = true;
            MessageBox.Show("The text editor is not yet finished, UI text loading and saving is not implemented yet.");
        }
        // language selection
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            textBox2.Text = languages[comboBox1.SelectedIndex];
            if (!setup) { return; }
            if (listBox1.SelectedIndex != -1)
            {
                richTextBox1.Text = GetMissionText(listBox1.SelectedIndex, languages[comboBox1.SelectedIndex]);
            }
        }
        // entry selection ( missions or UI text )
        private void listBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            textBox1.Text = missions[listBox1.SelectedIndex];
            richTextBox1.Text = GetMissionText(listBox1.SelectedIndex, languages[comboBox1.SelectedIndex]);
        }
        // get mission text from file based on index and language
        private string GetMissionText(int index, string language)
        {
            string missionText = "";
            string filePath = languageDirectory + "MISSION";
            switch (comboBox1.SelectedIndex)
            {
                case 0: filePath = filePath + "E"; break;
                case 1: filePath = filePath + "F"; break;
                case 2: filePath = filePath + "I"; break;
                case 3: filePath = filePath + "S"; break;
                default: MessageBox.Show("Unsupported language selected.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return missionText;
            }
            filePath += ".TXT";
            if (!File.Exists(filePath))
            {
                MessageBox.Show("Missions file not found: " + filePath, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return missionText;
            }
            using (StreamReader reader = new StreamReader(filePath, Encoding.GetEncoding(858))) // 858	IBM00858	OEM Multilingual Latin I
            {
                int entryIndex = -1;
                bool insideBlock = false;
                List<string> blockLines = new List<string>();

                string line;
                while ((line = reader.ReadLine()!) != null)
                {
                    if (line.Trim() == "*")
                    {
                        if (!insideBlock)
                        {
                            // Start of a block
                            insideBlock = true;
                            blockLines.Clear();
                        }
                        else
                        {
                            // End of a block
                            entryIndex++;
                            if (entryIndex == index)
                            {
                                missionText = string.Join(Environment.NewLine, blockLines);
                                break;
                            }
                            insideBlock = false;
                        }
                        continue;
                    }
                    if (insideBlock) { blockLines.Add(line); }
                }
            }
            return missionText; // Placeholder for the method that retrieves the mission text based on index and language
        }
        // Mission Text Selected
        private void radioButton2_CheckedChanged(object sender, EventArgs e)
        {
            setup = false;
            listBox1.Items.Clear();
            foreach (string mission in missions) { listBox1.Items.Add(mission); }
            setup = true;
            label8.Visible = true;
            label9.Visible = true;
            label10.Visible = true;
            label11.Visible = true;
            label12.Visible = true;
            label13.Visible = true;
            textBox1.Text = "";
            richTextBox1.Text = "";
        }
        // UI Text Selected
        private void radioButton1_CheckedChanged(object sender, EventArgs e)
        {
            setup = false;
            listBox1.Items.Clear();
            // parse BIN files here
            setup = true;
            label8.Visible = false;
            label9.Visible = false;
            label10.Visible = false;
            label11.Visible = false;
            label12.Visible = false;
            label13.Visible = false;
            textBox1.Text = "";
            richTextBox1.Text = "";
        }
        // restore backup button clicked
        private void button1_Click(object sender, EventArgs e)
        {
            string filePath = languageDirectory + "MISSION";
            string backupPath = "";
            if (radioButton1.Checked) // restore mission text file
            {
                switch (comboBox1.SelectedIndex)
                {
                    case 0: filePath = filePath + "E"; break;
                    case 1: filePath = filePath + "F"; break;
                    case 2: filePath = filePath + "I"; break;
                    case 3: filePath = filePath + "S"; break;
                    default: MessageBox.Show("This error should never happen! Error : A", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return;
                }
                filePath += ".TXT";
                backupPath = filePath + ".BAK";
            }
            else if (radioButton2.Checked) // restore ui text bin file
            {
                switch (comboBox1.SelectedIndex)
                {
                    case 0: filePath = languageDirectory + "ENGL"; break;
                    case 1: filePath = languageDirectory + "FREN"; break;
                    case 2: filePath = languageDirectory + "ITAL"; break;
                    case 3: filePath = languageDirectory + "SPAN"; break;
                    default: MessageBox.Show("This error should never happen! Error : B", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); return;
                }
                filePath += ".BIN";
                backupPath = filePath + ".BAK";
            }
            if (File.Exists(filePath))
            {
                File.Move(backupPath, filePath, true);
                File.Delete(backupPath);
                MessageBox.Show("Text restored from backup.");
                button1.Enabled = false;
            }
        }
    }
}