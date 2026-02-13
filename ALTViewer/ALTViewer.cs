namespace ALTViewer
{
    public partial class ALTViewer : Form
    {
        public string patchDirectory = "";
        public string gameDirectory = "";
        public ALTViewer()
        {
            InitializeComponent();
            gameDirectory = Utilities.CheckDirectory();
            if(gameDirectory == "") { MessageBox.Show("Game directory not found. Please ensure this program is in the correct directory."); Close(); }
            patchDirectory = Path.Combine(gameDirectory, "SECT90", "L906LEV.MAP"); // setup patchDirectory for the first patch
            string sect32File = Path.Combine(gameDirectory, "SECT32", "371GFX.B16");
            // first patch – SECT90
            byte first = BinaryUtility.ReadByteAtOffset(patchDirectory, 0x50BC8); // checks if the first byte of the current patch has been applied
            // second patch – SECT32
            byte last = BinaryUtility.ReadByteAtOffset(gameDirectory + "SECT32\\371GFX.B16", 0x40B0B); // checks if the last byte of the current patch has been applied
            if (first == 0xFF || last != 0xFA) { button6.Visible = true; } // show patch button if the first byte is unchanged or the last byte is not patched
        }
        // create new form method
        private void newForm(Form form)
        {
            form.StartPosition = FormStartPosition.Manual;
            form.Location = this.Location;
            form.Show();
            this.Hide();
            form.FormClosed += (s, args) => this.Show();
            form.Move += (s, args) => { if (this.Location != form.Location) { this.Location = form.Location; } };
        }
        private void button1_Click(object sender, EventArgs e) { newForm(new TextEditor()); }
        private void button2_Click(object sender, EventArgs e) { newForm(new ModelViewer()); }
        private void button3_Click(object sender, EventArgs e) { newForm(new GraphicsViewer()); }
        private void button4_Click(object sender, EventArgs e) { newForm(new SoundEffects()); }
        private void button5_Click(object sender, EventArgs e) { newForm(new MapViewer()); }
        // patch changes
        private void button6_Click(object sender, EventArgs e)
        {
            // TODO : remove patchDirectory variable and pass direct values
            // L906LEV.MAP
            List<Tuple<long, byte[]>> replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x51342L, new byte[] { 0x04 }), // fix 2, 3 & 4 ( railing transparency )
                Tuple.Create(0x51A0EL, new byte[] { 0x04 }),
                Tuple.Create(0x51EBEL, new byte[] { 0x04 }),
                Tuple.Create(0x5236EL, new byte[] { 0x04 }),
                Tuple.Create(0x52AEEL, new byte[] { 0x04 }),
                Tuple.Create(0x22BF9L, new byte[] { 0x04 }), // fix 2, 3 & 4 ( railing transparency )
                Tuple.Create(0x50BC8L, new byte[] { 0xB4, 0x22, 0x00, 0x00 }), // fix 1 ( triangle )
                Tuple.Create(0x45680L, new byte[] { 0x75, 0x00 }), // fix 5 ( bridge section )
                Tuple.Create(0x22BF9L, new byte[] { 0x01, 0x04 }), //0x22BF9 == 01 04 ( railing texture fix )
            // NOTE : This face still isn't visible in-game
                Tuple.Create(0x50BE8L, new byte[] { 0x75, 0x00 }), //0x50BE8 == 75 00 ( under railing texture fix ) // NOTE : This face still isn't visible in-game
            // NOTE : This face still isn't visible in-game
                Tuple.Create(0x456A8L, new byte[] { 0x75 }), // fix 5 ( bridge section 1 )
                Tuple.Create(0x457E8L, new byte[] { 0x75 }),
                Tuple.Create(0x45810L, new byte[] { 0x75 }),
                Tuple.Create(0x45838L, new byte[] { 0x75 }),
                Tuple.Create(0x45860L, new byte[] { 0x75 }),
                Tuple.Create(0x45888L, new byte[] { 0x75 }), // fix 5 ( bridge section 1 )
                Tuple.Create(0x41C60L, new byte[] { 0x75 }), // fix 5 ( bridge section 2 )
                Tuple.Create(0x41C38L, new byte[] { 0x75 }),
                Tuple.Create(0x41C10L, new byte[] { 0x75 }),
                Tuple.Create(0x41BE8L, new byte[] { 0x75 }),
                Tuple.Create(0x41BC0L, new byte[] { 0x75 }),
                Tuple.Create(0x41B98L, new byte[] { 0x75 }), // fix 5 ( bridge section 2 )
                Tuple.Create(0x3D00CL, new byte[] { 0x75 }), // fix 5 ( bridge section 3 )
                Tuple.Create(0x3CA94L, new byte[] { 0x75 }),
                Tuple.Create(0x36E64L, new byte[] { 0x75 }),
                Tuple.Create(0x36068L, new byte[] { 0x75 }), // fix 5 ( bridge section 3 )
                Tuple.Create(0x35F00L, new byte[] { 0x75 }), // fix 5 ( bridge section 4 )
                Tuple.Create(0x36CC0L, new byte[] { 0x75 }),
                Tuple.Create(0x37B5CL, new byte[] { 0x75 }),
                Tuple.Create(0x38908L, new byte[] { 0x75 }),
                Tuple.Create(0x396A0L, new byte[] { 0x75 }),
                Tuple.Create(0x3A474L, new byte[] { 0x75 }),
                Tuple.Create(0x3AD34L, new byte[] { 0x75 }),
                Tuple.Create(0x3C044L, new byte[] { 0x75 }),
                Tuple.Create(0x3C8F0L, new byte[] { 0x75 }),
                Tuple.Create(0x3CEA4L, new byte[] { 0x75 }), // fix 5 ( bridge section 4 )
                Tuple.Create(0x35C44L, new byte[] { 0x75 }), // fix 5 ( bridge section 5 )
                Tuple.Create(0x35C6CL, new byte[] { 0x75 }),
                Tuple.Create(0x35C94L, new byte[] { 0x75 }),
                Tuple.Create(0x35CBCL, new byte[] { 0x75 }),
                Tuple.Create(0x35CE4L, new byte[] { 0x75 }),
                Tuple.Create(0x35D0CL, new byte[] { 0x75 }), // fix 5 ( bridge section 5 )
                Tuple.Create(0x31BD0L, new byte[] { 0x75 }), // fix 5 ( bridge section 6 )
                Tuple.Create(0x31BA8L, new byte[] { 0x75 }),
                Tuple.Create(0x31B80L, new byte[] { 0x75 }),
                Tuple.Create(0x31B58L, new byte[] { 0x75 }),
                Tuple.Create(0x31B30L, new byte[] { 0x75 }),
                Tuple.Create(0x31B08L, new byte[] { 0x75 }), // fix 5 ( bridge section 6 )
                Tuple.Create(0x3C620L, new byte[] { 0x75 }), // fix 5 ( bridge section 7 )
                Tuple.Create(0x3CDF0L, new byte[] { 0x75 }),
                Tuple.Create(0x3D1ECL, new byte[] { 0x75 }),
                Tuple.Create(0x3D5E8L, new byte[] { 0x75 }),
                Tuple.Create(0x3DB38L, new byte[] { 0x75 }),
                Tuple.Create(0x3E164L, new byte[] { 0x75 }), // fix 5 ( bridge section 7 )
                Tuple.Create(0x3E1DCL, new byte[] { 0x75 }), // fix 5 ( bridge section 8 )
                Tuple.Create(0x3DBB0L, new byte[] { 0x75 }),
                Tuple.Create(0x3D660L, new byte[] { 0x75 }),
                Tuple.Create(0x3D264L, new byte[] { 0x75 }),
                Tuple.Create(0x3CE68L, new byte[] { 0x75 }),
                Tuple.Create(0x3C698L, new byte[] { 0x75 }), // fix 5 ( bridge section 8 )
                // fix texture flips
                Tuple.Create(0x3A476L, new byte[] { 0x02 }),
                Tuple.Create(0x35D36L, new byte[] { 0x02 }),
                Tuple.Create(0x35E26L, new byte[] { 0x02 }),
                Tuple.Create(0x35D86L, new byte[] { 0x02 }),
                Tuple.Create(0x35DD6L, new byte[] { 0x02 }),
                Tuple.Create(0x35E76L, new byte[] { 0x02 }),
                Tuple.Create(0x35EC6L, new byte[] { 0x02 }),
                Tuple.Create(0x2549AL, new byte[] { 0x02 }),
                Tuple.Create(0x278B2L, new byte[] { 0x02 }),
                Tuple.Create(0x293A6L, new byte[] { 0x02 }),
                Tuple.Create(0x2B0B6L, new byte[] { 0x02 }),
                Tuple.Create(0x2D2EEL, new byte[] { 0x02 }),
                Tuple.Create(0x2F116L, new byte[] { 0x02 }),
                Tuple.Create(0x30476L, new byte[] { 0x02 }),
                Tuple.Create(0x31DB2L, new byte[] { 0x02 }),
                Tuple.Create(0x5132EL, new byte[] { 0x02 }),
                Tuple.Create(0x4F51AL, new byte[] { 0x02 }),
                Tuple.Create(0x4E2D2L, new byte[] { 0x02 }),
                Tuple.Create(0x4CFEAL, new byte[] { 0x02 }),
                Tuple.Create(0x4BE1AL, new byte[] { 0x02 }),
                Tuple.Create(0x458B2L, new byte[] { 0x02 }),
                Tuple.Create(0x45902L, new byte[] { 0x02 }),
                Tuple.Create(0x408B2L, new byte[] { 0x02 }),
                Tuple.Create(0x3E936L, new byte[] { 0x02 }),
                Tuple.Create(0x41B72L, new byte[] { 0x02 }),
                Tuple.Create(0x4123AL, new byte[] { 0x02 }),
                Tuple.Create(0x3EB66L, new byte[] { 0x02 }),
                Tuple.Create(0x3DDE2L, new byte[] { 0x02 }),
                // fix texture flips
                Tuple.Create(0x35D5EL, new byte[] { 0x00 }),
                Tuple.Create(0x35DAEL, new byte[] { 0x00 }),
                Tuple.Create(0x35DFEL, new byte[] { 0x00 }),
                Tuple.Create(0x35E4EL, new byte[] { 0x00 }),
                Tuple.Create(0x35E9EL, new byte[] { 0x00 }),
                Tuple.Create(0x254C2L, new byte[] { 0x00 }),
                Tuple.Create(0x269EEL, new byte[] { 0x00 }),
                Tuple.Create(0x2A0DAL, new byte[] { 0x00 }),
                Tuple.Create(0x2E496L, new byte[] { 0x00 }),
                Tuple.Create(0x30FDEL, new byte[] { 0x00 }),
                Tuple.Create(0x4EC46L, new byte[] { 0x00 }),
                Tuple.Create(0x4D8FAL, new byte[] { 0x00 }),
                Tuple.Create(0x4C8BAL, new byte[] { 0x00 }),
                Tuple.Create(0x4B442L, new byte[] { 0x00 }),
                Tuple.Create(0x458DAL, new byte[] { 0x00 }),
                Tuple.Create(0x4592AL, new byte[] { 0x00 }),
                Tuple.Create(0x3F552L, new byte[] { 0x00 }),
                Tuple.Create(0x41B4AL, new byte[] { 0x00 }),
                Tuple.Create(0x3FACAL, new byte[] { 0x00 }),
                Tuple.Create(0x3E40EL, new byte[] { 0x00 }),
                Tuple.Create(0x3D842L, new byte[] { 0x00 }),
                Tuple.Create(0x3D306L, new byte[] { 0x00 }),
                // L906LEV.MAP - D000 - flip texture
                Tuple.Create(0x801C2L, new byte[] { 0x0B }),
                Tuple.Create(0x80172L, new byte[] { 0x0B }),
                Tuple.Create(0x80262L, new byte[] { 0x0B }),
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // L905LEV.MAP
            patchDirectory = gameDirectory + "SECT90\\L905LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x56ADAL, new byte[] { 0x00 }), // fix 1 ( pipes )
                Tuple.Create(0x57A8EL, new byte[] { 0x00 }), // fix 1 ( pipes )
                Tuple.Create(0x41356L, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x4645AL, new byte[] { 0x00 }),
                Tuple.Create(0x46432L, new byte[] { 0x00 }),
                Tuple.Create(0x4A8F2L, new byte[] { 0x00 }),
                Tuple.Create(0x50F4AL, new byte[] { 0x00 }),
                Tuple.Create(0x5186EL, new byte[] { 0x00 }),
                Tuple.Create(0x5211AL, new byte[] { 0x00 }),
                Tuple.Create(0x50FC2L, new byte[] { 0x00 }),
                Tuple.Create(0x51896L, new byte[] { 0x00 }),
                Tuple.Create(0x52142L, new byte[] { 0x00 }),
                Tuple.Create(0x51012L, new byte[] { 0x00 }),
                Tuple.Create(0x518BEL, new byte[] { 0x00 }),
                Tuple.Create(0x5216AL, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x5103AL, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x518E6L, new byte[] { 0x00 }),
                Tuple.Create(0x52192L, new byte[] { 0x00 }),
                Tuple.Create(0x42C42L, new byte[] { 0x00 }),
                Tuple.Create(0x4434EL, new byte[] { 0x00 }),
                Tuple.Create(0x44222L, new byte[] { 0x00 }),
                Tuple.Create(0x44236L, new byte[] { 0x00 }),
                Tuple.Create(0x42B2AL, new byte[] { 0x00 }),
                Tuple.Create(0x4410AL, new byte[] { 0x00 }),
                Tuple.Create(0x42B66L, new byte[] { 0x00 }),
                Tuple.Create(0x42B7AL, new byte[] { 0x00 }),
                Tuple.Create(0x42B8EL, new byte[] { 0x00 }),
                Tuple.Create(0x442AEL, new byte[] { 0x00 }),
                Tuple.Create(0x42BCAL, new byte[] { 0x00 }),
                Tuple.Create(0x42BF2L, new byte[] { 0x00 }),
                Tuple.Create(0x462B6L, new byte[] { 0x00 }),
                Tuple.Create(0x4619EL, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x461DAL, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x461EEL, new byte[] { 0x00 }),
                Tuple.Create(0x46216L, new byte[] { 0x00 }),
                Tuple.Create(0x4622AL, new byte[] { 0x00 }),
                Tuple.Create(0x46266L, new byte[] { 0x00 }),
                Tuple.Create(0x48796L, new byte[] { 0x00 }),
                Tuple.Create(0x4867EL, new byte[] { 0x00 }),
                Tuple.Create(0x48692L, new byte[] { 0x00 }),
                Tuple.Create(0x49E52L, new byte[] { 0x00 }),
                Tuple.Create(0x49D3AL, new byte[] { 0x00 }),
                Tuple.Create(0x49D76L, new byte[] { 0x00 }),
                Tuple.Create(0x49D8AL, new byte[] { 0x00 }),
                Tuple.Create(0x49D9EL, new byte[] { 0x00 }),
                Tuple.Create(0x49DDAL, new byte[] { 0x00 }),
                Tuple.Create(0x49E16L, new byte[] { 0x00 }),
                Tuple.Create(0x488C2L, new byte[] { 0x00 }),
                Tuple.Create(0x487AAL, new byte[] { 0x00 }), // fix 2 ( pipes )
                Tuple.Create(0x487D2L, new byte[] { 0x00 }),
                Tuple.Create(0x462F2L, new byte[] { 0x00 }),
                Tuple.Create(0x4632EL, new byte[] { 0x00 }),
                Tuple.Create(0x4637EL, new byte[] { 0x00 })
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // L111LEV.MAP - incorrect texture flags
            patchDirectory = gameDirectory + "SECT11\\L111LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x2E33EL, new byte[] { 0x00 }),
                Tuple.Create(0x2F3E2L, new byte[] { 0x00 }),
                Tuple.Create(0x2F40AL, new byte[] { 0x00 }),
                Tuple.Create(0x2F432L, new byte[] { 0x00 }),
                Tuple.Create(0x307BAL, new byte[] { 0x00 }),
                Tuple.Create(0x307E2L, new byte[] { 0x00 }),
                Tuple.Create(0x3080AL, new byte[] { 0x00 }),
                Tuple.Create(0x2EE92L, new byte[] { 0x00 }),
                Tuple.Create(0x38D7AL, new byte[] { 0x00 }),
                Tuple.Create(0x38D66L, new byte[] { 0x00 }),
                Tuple.Create(0x30936L, new byte[] { 0x00 }),
                Tuple.Create(0x30922L, new byte[] { 0x00 })
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            patchDirectory = gameDirectory + "SECT90\\L900LEV.MAP";
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            patchDirectory = gameDirectory + "SECT11\\L111LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x3054EL, new byte[] { 0x02 }),
                Tuple.Create(0x3053AL, new byte[] { 0x02 }),
                Tuple.Create(0x391DAL, new byte[] { 0x02 }),
                Tuple.Create(0x391C6L, new byte[] { 0x02 })
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            patchDirectory = gameDirectory + "SECT90\\L900LEV.MAP";
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // L162LEV.MAP - D001 - incorrect texture index
            patchDirectory = gameDirectory + "SECT12\\L162LEV.MAP";
            BinaryUtility.ReplaceByte(0x323E8, 0x67, patchDirectory);
            // L161LEV.MAP - D002 - incorrect texture index
            patchDirectory = gameDirectory + "SECT12\\L161LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x72800L, new byte[] { 0xD3 }),
                Tuple.Create(0x72801L, new byte[] { 0x00 }),
                Tuple.Create(0x72802L, new byte[] { 0x00 })
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // L901LEV.MAP - Incorrect player store positioning
            patchDirectory = gameDirectory + "SECT90\\L901LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x5BAD5L, new byte[] { 0x15, 0x29 }),
                Tuple.Create(0x5BAE9L, new byte[] { 0x16, 0x29 }),
                Tuple.Create(0x5BAFDL, new byte[] { 0x17, 0x29 }),
                Tuple.Create(0x5BB11L, new byte[] { 0x18, 0x29 })
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // L371LEV.MAP - inaccessible secret fix discovered by @bambamalicious
            patchDirectory = gameDirectory + "SECT32\\L371LEV.MAP";
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x6F78EL, new byte[] { 0x01 }),    // inaccessible secret fix discovered by @bambamalicious
                Tuple.Create(0x274ACL, new byte[] { 0xEA }),    // incorrect texture indexes fixed by @thor110
                Tuple.Create(0x1EDE8L, new byte[] { 0x23 })     // incorrect texture indexes fixed by @thor110
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            // 371GFX.B16 - graphics background fix
            patchDirectory = gameDirectory + "SECT32\\371GFX.B16";
            int i;
            byte[] bufferA = new byte[64];
            for (i = 0; i < 64; i++) { bufferA[i] = 0xFA; }
            byte[] bufferB = new byte[128];
            for (i = 0; i < 128; i++) { bufferB[i] = 0xFA; }
            byte[] bufferC = new byte[16448];
            for (i = 0; i < 16448; i++) { bufferC[i] = 0xFA; }
            replacements = new List<Tuple<long, byte[]>>()
            {
                Tuple.Create(0x3AB0CL, bufferA),    //3AB0C - 64 bytes of FA
                Tuple.Create(0x3ABCCL, bufferB),    //3ABCC - 128 bytes of FA
                Tuple.Create(0x3ACCCL, bufferB),    //3ACCC - 128 bytes of FA
                Tuple.Create(0x3ADCCL, bufferB),    //3ADCC - 128 bytes of FA
                Tuple.Create(0x3AECCL, bufferB),    //3AECC - 128 bytes of FA
                Tuple.Create(0x3AFCCL, bufferB),    //3AFCC - 128 bytes of FA
                Tuple.Create(0x3B0CCL, bufferB),    //3B0CC - 128 bytes of FA
                Tuple.Create(0x3B1CCL, bufferB),    //3B1CC - 128 bytes of FA
                Tuple.Create(0x3B2CCL, bufferB),    //3B2CC - 128 bytes of FA
                Tuple.Create(0x3B3CCL, bufferB),    //3B3CC - 128 bytes of FA
                Tuple.Create(0x3B4CCL, bufferB),    //3B4CC - 128 bytes of FA
                Tuple.Create(0x3B5CCL, bufferB),    //3B5CC - 128 bytes of FA
                Tuple.Create(0x3B6CCL, bufferB),    //3B6CC - 128 bytes of FA
                Tuple.Create(0x3B7CCL, bufferB),    //3B7CC - 128 bytes of FA
                Tuple.Create(0x3B8CCL, bufferB),    //3B8CC - 128 bytes of FA
                Tuple.Create(0x3B9CCL, bufferB),    //3B9CC - 128 bytes of FA
                Tuple.Create(0x3BACCL, bufferB),    //3BACC - 128 bytes of FA
                Tuple.Create(0x3BBCCL, bufferB),    //3BBCC - 128 bytes of FA
                Tuple.Create(0x3BCCCL, bufferB),    //3BCCC - 128 bytes of FA
                Tuple.Create(0x3BDCCL, bufferB),    //3BDCC - 128 bytes of FA
                Tuple.Create(0x3BECCL, bufferB),    //3BECC - 128 bytes of FA
                Tuple.Create(0x3BFCCL, bufferB),    //3BFCC - 128 bytes of FA
                Tuple.Create(0x3C0CCL, bufferB),    //3C0CC - 128 bytes of FA
                Tuple.Create(0x3C1CCL, bufferB),    //3C1CC - 128 bytes of FA
                Tuple.Create(0x3C2CCL, bufferB),    //3C2CC - 128 bytes of FA
                Tuple.Create(0x3C3CCL, bufferB),    //3C3CC - 128 bytes of FA
                Tuple.Create(0x3C4CCL, bufferB),    //3C4CC - 128 bytes of FA
                Tuple.Create(0x3C5CCL, bufferB),    //3C5CC - 128 bytes of FA
                Tuple.Create(0x3C6CCL, bufferB),    //3C6CC - 128 bytes of FA
                Tuple.Create(0x3C7CCL, bufferB),    //3C7CC - 128 bytes of FA
                Tuple.Create(0x3C8CCL, bufferB),    //3C8CC - 128 bytes of FA
                Tuple.Create(0x3C9CCL, bufferB),    //3C9CC - 128 bytes of FA
                Tuple.Create(0x3CACCL, bufferC)     //3CACC - 16448 bytes of FA
            };
            BinaryUtility.ReplaceBytes(replacements, patchDirectory);
            //
            button6.Visible = false; // hide button after patching
            MessageBox.Show("Patch applied successfully!");
        }
    }
}
