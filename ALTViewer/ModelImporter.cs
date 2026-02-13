namespace ALTViewer
{
    internal class ModelImporter
    {
        public static void importModel(string fileName, string levelNumber, string levelPath)
        {
            //MessageBox.Show(levelNumber);
            //MessageBox.Show(levelPath + $"\\{levelNumber}GFX.B16");
            //MessageBox.Show(levelPath + $"\\L{levelNumber}LEV.MAP");

            // parse original file up to level data to determine offset

            //391GFX.B16
            //205D8 -> BX01
            //207F4 -> TP02 start
            //263GFX.B16
            //510DC -> BX04
            //512F8 -> EOF
            //162GFX.B16
            //40D18 -> BX03
            //40F34 -> TP04 start

            return;

            List<BndSection> uvSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(levelPath + $"\\{levelNumber}GFX.B16"), "BX");
            var uvRects = new List<(int X, int Y, int Width, int Height)>[5];
            for (int i = 0; i < 5; i++)
            {
                uvRects[i] = ModelRenderer.ParseBxRectangles(uvSections[i].Data);
            }

            // uv rectangles are actually just in the B16/BND files?!!!!
            return;

            // determine data length for safety check
            long length = 0x51342;
            // parse obj data to bytes
            byte[] newModel = { };
            // write new bytes
            List<Tuple<long, byte[]>> replacements = new List<Tuple<long, byte[]>>() { Tuple.Create(length, newModel) };
            BinaryUtility.ReplaceBytes(replacements, levelPath + $"\\L{levelNumber}LEV.MAP");
            
        }
    }
}
