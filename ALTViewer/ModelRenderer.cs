using System.Drawing.Imaging;

namespace ALTViewer
{
    public static class ModelRenderer
    {
        public const float texSize = 256f;
        public static int[] unknownValues = new int[] // level specific unknown values
        {
            3, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
            22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 38,
            40, 43, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 59, 61,
            62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 75, 77, 78, 79,
            80, 81, 82, 83, 84, 85, 87, 88, 89, 90, 91, 93, 94, 95, 96, 97,
            98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
            111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
            124, 125, 126, 127, 128, 133, 134, 135, 136, 137, 138, 139, 140,
            141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153,
            154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 166, 168,
            171, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184,
            187, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200,
            201, 203, 205, 206, 207, 208, 209, 210, 211, 212, 213, 215, 216,
            217, 218, 219, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
            232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244,
            245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
        };
        public static int[] textureFlags = new int[] { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 13, 14, 26, 28, 30, 32, 34, 255 }; // level specific flags
        // lift unknown values only use 0
        public static int[] liftFlags = new int[] { 0, 2, 11, 128, 130, 139 }; // lift specific flags
        // door unknown values only use 0
        // 0, 2, 11, 128, 139 // door specific flags
        public static void ExportLevel(string levelName, List<BndSection> uvSections, byte[] levelSection, string textureName, string outputPath, bool debug, bool unknown, bool patch)
        {
            bool L111LEVFIX = false;
            bool L141LEVFIX = false;
            bool L161LEVFIX = false;
            bool L906LEVFIX = false;
            // switch on level name after checking if the first patch is applied
            // only update incorrect UVs if the first patch is applied
            // otherwise retain original imperfections for those that might want to see them
            if (patch && !debug && !unknown) // test adjustments necessary for unity version (pre-patched)
            {
                switch (levelName) // level specific booleans so that string comparison is only done once and only booleans have to be checked when writing every face
                {
                    case "L111LEV": L111LEVFIX = true; break;
                    case "L141LEV": L141LEVFIX = true; break;
                    case "L161LEV": L161LEVFIX = true; break;
                    case "L903LEV": L141LEVFIX = true; break; // L903LEV is the same as L141LEV
                    case "L900LEV": L111LEVFIX = true; break; // L900LEV is the same as L111LEV
                    case "L906LEV": L906LEVFIX = true; break;
                }
            }
            // above are unique patch specific variables
            using var br = new BinaryReader(new MemoryStream(levelSection)); // skip first 20 bytes + 36 below = 56
            ushort vertCount = br.ReadUInt16();         // Number of vertices
            ushort quadCount = br.ReadUInt16();         // Number of quads
            /* // uncomment if you want to read the level header
            ushort mapLength = br.ReadUInt16();         // Length of the map section
            ushort mapWidth = br.ReadUInt16();          // Width of the map section
            ushort playerStartX = br.ReadUInt16();      // Player start X coordinate
            ushort playerStartY = br.ReadUInt16();      // Player start Y coordinate
            byte unknown = br.ReadByte();               // unknown object type ( possibly lights )
            br.ReadByte();                              // unknown 1 ( unused? 128 on all levels )
            ushort monster = br.ReadUInt16();           // Number of monsters
            ushort pickups = br.ReadUInt16();           // Number of pickups
            ushort boxes = br.ReadUInt16();             // Number of boxes
            ushort doors = br.ReadUInt16();             // Number of doors
            br.ReadBytes(2);                            // unknown 2
            ushort playerStartAngle = br.ReadUInt16();  // Player start angle
            br.ReadBytes(10);                           // unknown 3 & 4
            */ // comment out the next line if you want to read the level header
            br.BaseStream.Seek(32, SeekOrigin.Current); // Skip 36 bytes to reach vertex and quad data
            List<(short X, short Y, short Z)> vertices = new();
            for (int i = 0; i < vertCount; i++) // Count Vertices
            {
                short x = br.ReadInt16();
                short y = br.ReadInt16();
                short z = br.ReadInt16();
                br.ReadInt16();                         // padding bytes always 00 00 in every level
                vertices.Add((x, y, z));
            }
            List<(int A, int B, int C, int D, ushort TexIndex, byte Flags, byte Other)> quads = new();
            for (int i = 1; i < quadCount; i++) // Count Quads ( start at 1 to avoid the final face which is always FF FF FF FF )
            {
                int a = br.ReadInt32();
                int b = br.ReadInt32();
                int c = br.ReadInt32();
                int d = br.ReadInt32();
                ushort texIndex = br.ReadUInt16();
                byte flags = br.ReadByte();
                byte light = br.ReadByte();             // light id x // Kaiser
                quads.Add((a, b, c, d, texIndex, flags, light));
            }
            // Read UV rectangles BX00-BX04
            var uvRects = new List<(int X, int Y, int Width, int Height)>[5];
            for (int i = 0; i < 5; i++)
            {
                uvRects[i] = ParseBxRectangles(uvSections[i].Data);
            }
            using var sw = new StreamWriter(outputPath + $"\\{levelName}.obj");

            using var mtlWriter = new StreamWriter(Path.Combine(outputPath, $"{levelName}.mtl"));
            sw.WriteLine($"# OBJ exported from Alien Trilogy {levelName}");

            sw.WriteLine($"mtllib {levelName}.mtl");

            if (debug) // create debug material file
            {
                foreach (int f in textureFlags)
                {
                    mtlWriter.WriteLine($"newmtl Texture{f:D2}");
                    mtlWriter.WriteLine($"map_Kd FLAGS{f}.png");
                }
            }
            else if (unknown) // create unknown material file
            {
                for (int t = 0; t < 222; t++)
                {
                    mtlWriter.WriteLine($"newmtl UnkByte_{unknownValues[t]}");
                    mtlWriter.WriteLine($"map_Kd UnkByte_{unknownValues[t]}.png");
                }
            }
            else // create standard material file
            {
                for (int t = 0; t < 5; t++)
                {
                    mtlWriter.WriteLine($"newmtl Texture{t:D2}");
                    mtlWriter.WriteLine($"map_Kd {textureName}_TP{t:D2}.png");
                }
            }
            // Write vertex positions
            foreach (var v in vertices)
            {
                sw.WriteLine($"v {v.X:F4} {v.Y:F4} {v.Z:F4}");
            }
            // Store unique UVs and their indices
            var uvDict = new Dictionary<(float, float), int>();
            var uvList = new List<(float, float)>();
            // Ensure at least one dummy UV exists (for fallback cases using index 1)
            if (uvList.Count == 0)
            {
                uvDict[(0f, 0f)] = 1;
                uvList.Add((0f, 0f));
            }
            // Map of per-face vertex UV indices
            var faceUvs = new List<int[]>();

            for (int i = 0; i < quads.Count; i++)
            {
                var q = quads[i];
                var uvIndices = new int[4];

                // Resolve texture group + local UV rect index
                bool found = false;
                int texGroup = 0;
                int localIndex = q.TexIndex;

                for (int t = 0; t < 5; t++)
                {
                    int count = uvRects[t].Count;
                    if (localIndex < count)
                    {
                        texGroup = t;
                        found = true;
                        break;
                    }
                    localIndex -= count;
                }

                if (!found) // L905LEV:6358 // this only pops for one face in one level //MessageBox.Show($"{levelName}:{i}");  
                {
                    // a : 7285 // b : 7340 // c : 7315 // d : 7316 // texIndex : 65535 ( FF FF ) @ 0x3A6D0 // flags : 14 // other : 138 // L905LEV:6358 
                    faceUvs.Add(new int[] { 1, 1, 1, 1 }); // Fallback rectangle or skip invalid quad // or log + continue
                    continue;
                }

                var rect = uvRects[texGroup][localIndex];
                float x0 = rect.X / texSize;
                float y0 = rect.Y / texSize;
                float x1 = (rect.X + rect.Width) / texSize;
                float y1 = (rect.Y + rect.Height) / texSize;

                var baseUvs = new (float, float)[]
                {
                    (x0, y1), // top-left
                    (x1, y1), // bottom-left
                    (x1, y0), // bottom-right
                    (x0, y0), // top-right
                };

                switch (q.Flags)
                {
                    case 1:
                    case 5:
                    case 13:
                        // Triangle with special order: A → 0, C → 2, D → 3
                        baseUvs = new[] { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                        break;
                    case 2:
                        // Flip texture 180
                        baseUvs = new[] { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
                        break;
                    default:
                        // Standard quad order - no change
                        break;
                }

                for (int j = 0; j < 4; j++)
                {
                    if (!uvDict.TryGetValue(baseUvs[j], out int idx))
                    {
                        idx = uvList.Count + 1;
                        uvDict[baseUvs[j]] = idx;
                        uvList.Add(baseUvs[j]);
                    }
                    uvIndices[j] = idx;
                }

                faceUvs.Add(uvIndices);
            }

            // Write UVs
            foreach (var uv in uvList)
            {
                sw.WriteLine($"vt {uv.Item1:F6} {1 - uv.Item2:F6}"); // Flip Y for OBJ
            }
            // Write faces with material switching
            string currentMtl = null!;
            for (int i = 0; i < quads.Count; i++)
            {
                var q = quads[i];
                var uv = faceUvs[i];

                // Resolve which BX section this texIndex belongs to
                int texGroup = 0;
                int localIndex = q.TexIndex;
                for (int t = 0; t < 5; t++)
                {
                    int count = uvRects[t].Count;
                    if (localIndex < count)
                    {
                        texGroup = t;
                        break;
                    }
                    localIndex -= count;
                }
                //
                string matName = "";
                if (debug) { matName = $"usemtl Texture{q.Flags:D2}"; }
                else if (unknown) { matName = $"usemtl UnkByte_{q.Other}"; }
                else { matName = $"usemtl Texture{texGroup:D2}"; }
                //
                if (matName != currentMtl) { sw.WriteLine(currentMtl = matName); } // assign and write current material
                // Faces
                if (q.D == -1)
                {
                    sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]}");
                }
                else // TODO : I plan to use all of this as reference for fixing the UVs in the original BX sections if possible and when I get a chance
                {
                    // NOTE : These will fix the incorrect UVs in Unity incase I cannot fix the BX sections and are also for me to use to cross reference them
                    if (L111LEVFIX) // fix some incorrect textures on the first level
                    {
                        switch (i)
                        {
                            case 10208: // starting lift panel ( this is +1 as well???? )
                                sw.WriteLine($"f {q.A + 1}/{uv[1]} {q.B + 1}/{uv[0]} {q.C + 1}/{uv[3]} {q.D + 1}/{uv[2]}");
                                continue;
                            case 1062: // hallway crate top as side ( this isn't +1 either???? )
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[6527][0]} {q.B + 1}/{faceUvs[6527][1]} {q.C + 1}/{faceUvs[6527][2]} {q.D + 1}/{faceUvs[6527][3]}");
                                continue;
                            case 6527: // rotate crate UV large room ( and this isn't +1???? )
                                sw.WriteLine($"f {q.A + 1}/{uv[1]} {q.B + 1}/{uv[2]} {q.C + 1}/{uv[3]} {q.D + 1}/{uv[0]}");
                                continue;
                            case 3622: // right side room upside down crate UV small room ( this isn't +1 either???? )
                            case 7780: // upside down crate UV large room ( why is this +1???? )
                                sw.WriteLine($"f {q.A + 1}/{uv[2]} {q.B + 1}/{uv[3]} {q.C + 1}/{uv[0]} {q.D + 1}/{uv[1]}");
                                continue;
                            default:
                                sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                                continue;
                        }
                    }
                    else if (L141LEVFIX) // fix some incorrect textures on weyland yutani crates
                    {
                        switch (i)
                        {
                            case 2736: // door texture on back of weyland yutani crate side
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[2727][0]} {q.B + 1}/{faceUvs[2727][1]} {q.C + 1}/{faceUvs[2727][2]} {q.D + 1}/{faceUvs[2727][3]}");
                                continue;
                            case 4449:
                            case 4450: // incorrect weyland yutani crate sides
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[4268][0]} {q.B + 1}/{faceUvs[4268][1]} {q.C + 1}/{faceUvs[4268][2]} {q.D + 1}/{faceUvs[4268][3]}");
                                continue;
                            case 4451: // incorrect weyland yutani crate top // maybe adjust more????
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[4269][0]} {q.B + 1}/{faceUvs[4269][1]} {q.C + 1}/{faceUvs[4269][2]} {q.D + 1}/{faceUvs[4269][3]}");
                                continue;
                            case 6168:
                            case 6169: // incorrect weyland yutani crate sides
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[6173][0]} {q.B + 1}/{faceUvs[6173][1]} {q.C + 1}/{faceUvs[6173][2]} {q.D + 1}/{faceUvs[6173][3]}");
                                continue;
                            case 6186: // weyland yutani sideways crate
                                sw.WriteLine($"f {q.A + 1}/{uv[1]} {q.B + 1}/{uv[2]} {q.C + 1}/{uv[3]} {q.D + 1}/{uv[0]}");
                                continue;
                            /*case 7431: // rotate weyland yutani crate top // open crate or not open crate????
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[7413][0]} {q.B + 1}/{faceUvs[7413][1]} {q.C + 1}/{faceUvs[7413][2]} {q.D + 1}/{faceUvs[7413][3]}");
                                continue;*/
                            case 7413: // rotate weyland yutani crate top
                                sw.WriteLine($"f {q.A + 1}/{uv[3]} {q.B + 1}/{uv[0]} {q.C + 1}/{uv[1]} {q.D + 1}/{uv[2]}");
                                continue;
                            /*case 7962: // weyland yutani crate top // open crate or not open crate????
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[7965][0]} {q.B + 1}/{faceUvs[7965][1]} {q.C + 1}/{faceUvs[7965][2]} {q.D + 1}/{faceUvs[7965][3]}");
                                continue;*/
                            case 8108: // grey crate side
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[8110][1]} {q.B + 1}/{faceUvs[8110][2]} {q.C + 1}/{faceUvs[8110][3]} {q.D + 1}/{faceUvs[8110][0]}");
                                continue;
                            case 8111: // grey crate side
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[8110][3]} {q.B + 1}/{faceUvs[8110][0]} {q.C + 1}/{faceUvs[8110][1]} {q.D + 1}/{faceUvs[8110][2]}");
                                continue;
                            case 8112: // grey crate top
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[8111][0]} {q.B + 1}/{faceUvs[8111][1]} {q.C + 1}/{faceUvs[8111][2]} {q.D + 1}/{faceUvs[8111][3]}");
                                continue;
                            default:
                                sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                                continue;
                        }
                    }
                    else if (L161LEVFIX)
                    {
                        switch (i)
                        {
                            case 3505: // secretion covered wall
                                sw.WriteLine($"f {q.A + 1}/{faceUvs[3315][2]} {q.B + 1}/{faceUvs[3315][3]} {q.C + 1}/{faceUvs[3315][0]} {q.D + 1}/{faceUvs[3315][1]}");
                                continue;
                            default:
                                sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                                continue;
                        }
                    }
                    else if (L906LEVFIX) // fix for the misaligned UVs on L906LEV
                    {
                        switch(i)
                        {
                            case 10900:
                                sw.WriteLine($"f {q.A + 1}/{uv[2]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[0]} {q.D + 1}/{uv[3]}");
                                continue;
                            case 8580:
                            case 8598:
                            case 8602:
                            case 7826:
                            case 7830:
                            case 7834:
                            case 5428:
                            case 5586:
                            case 5948:
                            case 6655:
                            case 6839:
                            case 5375:
                            case 5379:
                            case 5383:
                            case 4550:
                            case 4546:
                            case 4542:
                            case 6830:
                            case 6932:
                            case 7079:
                            case 7006:
                            case 6887:
                            case 6736:
                            case 6787:
                                sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[3]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[1]}");
                                continue;
                            case 8596:
                            case 8600:
                            case 8604:
                            case 7824:
                            case 7828:
                            case 7832:
                            case 6857:
                            case 5607:
                            case 5410:
                            case 5773:
                            case 6411:
                            case 6766:
                            case 5377:
                            case 5381:
                            case 5385:
                            case 4548:
                            case 4544:
                            case 4540:
                            case 6730:
                            case 6881:
                            case 7000:
                            case 7085:
                            case 6938:
                            case 6836:
                                sw.WriteLine($"f {q.A + 1}/{uv[1]} {q.B + 1}/{uv[2]} {q.C + 1}/{uv[3]} {q.D + 1}/{uv[0]}");
                                continue;
                            case 5357:
                            case 5373:
                            case 4538:
                            case 4522:
                                sw.WriteLine($"f {q.A + 1}/{uv[3]} {q.B + 1}/{uv[2]} {q.C + 1}/{uv[1]} {q.D + 1}/{uv[0]}");//upside down?
                                continue;
                            default:
                                sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                                continue;
                        }
                    }
                    else
                    {
                        sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                    }
                }
            }
        }
        public static void GenerateFlagTextures(string outputDir, string levelName, bool levelLifts = false)
        {
            for (int i = 0; i < 18; i++) // textureFlags.Count = 18
            {
                using var bmp = new Bitmap(256, 256);
                using var g = Graphics.FromImage(bmp);

                if(levelLifts)
                {
                    if (i == 6) { break; }
                    switch (liftFlags[i])
                    {
                        case 0: g.Clear(Color.Black); break;
                        case 2: g.Clear(Color.DarkGray); break;
                        case 11: g.Clear(Color.DarkRed); break;
                        case 128: g.Clear(Color.Red); break;
                        case 130: g.Clear(Color.Orange); break;
                        case 139: g.Clear(Color.Yellow); break;
                    }
                    bmp.Save(Path.Combine(outputDir, $"FLAGS{liftFlags[i]}.png"), ImageFormat.Png);
                }
                else // levels
                {
                    switch (textureFlags[i])
                    {
                        case 0: g.Clear(Color.Black); break;
                        case 1: g.Clear(Color.DarkGray); break;
                        case 2: g.Clear(Color.DarkRed); break;
                        case 3: g.Clear(Color.Red); break;
                        case 4: g.Clear(Color.Orange); break;
                        case 5: g.Clear(Color.Yellow); break;
                        case 6: g.Clear(Color.Green); break;
                        case 8: g.Clear(Color.Blue); break;
                        case 10: g.Clear(Color.DarkBlue); break;
                        case 12: g.Clear(Color.Purple); break;
                        case 13: g.Clear(Color.White); break;
                        case 14: g.Clear(Color.LightGray); break;
                        case 26: g.Clear(Color.Brown); break;
                        case 28: g.Clear(Color.Pink); break;
                        case 30: g.Clear(Color.Gold); break;
                        case 32: g.Clear(Color.Tan); break;
                        case 34: g.Clear(Color.LimeGreen); break;
                        case 255: g.Clear(Color.SkyBlue); break;
                    }
                    bmp.Save(Path.Combine(outputDir, $"FLAGS{textureFlags[i]}.png"), ImageFormat.Png);
                }
                
            }
        }
        public static void GenerateUnknownTextures(string outputDir, string levelName)
        {
            for (int i = 0; i < 222; i++) // unknownValues.Count = 222
            {
                using var bmp = new Bitmap(256, 256);
                using var g = Graphics.FromImage(bmp);
                g.Clear(Color.Black);

                string number = $"{unknownValues[i]}";
                using var font = new Font("Arial", 8, FontStyle.Bold);
                using var brush = new SolidBrush(Color.White);
                var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };

                for (int y = 0; y < 8; y++)
                {
                    for (int x = 0; x < 8; x++)
                    {
                        int tileIndex = y * 8 + x;
                        if (tileIndex >= 64) break;

                        Rectangle tileRect = new Rectangle(x * 32, y * 32, 32, 32);
                        g.DrawRectangle(Pens.Gray, tileRect);
                        g.DrawString(number, font, brush, tileRect, sf);
                    }
                }

                bmp.Save(Path.Combine(outputDir, $"UnkByte_{unknownValues[i]}.png"), ImageFormat.Png);
            }
        }
        public static void ExportModel(string modelName, string textureDirectory, string modelDirectory, string textureName, string outputPath)
        {
            bool special = false;
            List<BndSection> modelSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(modelDirectory), "M0");
            List<BndSection> uvSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(textureDirectory), "BX"); // PICKMOD case
            List<(int X, int Y, int Width, int Height)> uvRects = ParseBxRectangles(uvSections[0].Data); // PICKMOD case
            string backupName = textureName; // for OBJ3D special case
            string backupDirectory = textureDirectory; // for OBJ3D special case
            if (modelName == "OBJ3D") { special = true; } // OBJ3D has special handling
            for (int m = 0; m < modelSections.Count; m++)
            {
                using var br = new BinaryReader(new MemoryStream(modelSections[m].Data));
                // 0 / 1 / 2 are fine to default // TODO : reduce duplicate code when all cases are resolved
                if (special && m >= 3 && m <= 18 || special && m == 35) // OBJ3D LOCKERS & COIL OBSTACLE
                {
                    textureDirectory = Utilities.CheckDirectory() + "LANGUAGE\\PNL0GFXE.16";
                    textureName = "PNL0GFXE";
                }
                else if (special && m >=19 && m <= 34 || special && m == 41) // OBJ3D BONESHIP SWITCHES && EGGHUSK
                {
                    textureDirectory = Utilities.CheckDirectory() + "LANGUAGE\\PNL1GFXE.16";
                    textureName = "PNL1GFXE";
                }
                else if (special && m >= 36 && m <= 40) // OBJ3D PYLON AND COMPUTER -> uses PICKGFX
                {
                    textureDirectory = backupDirectory; // restore previous texture directory
                    textureName = backupName; // restore previous texture name
                }
                if (uvSections.Count != 1 && !special) // OPTOBJ case
                {
                    textureName = $"{backupName}_TP{m:D2}";
                    uvRects = ParseBxRectangles(uvSections[m].Data);
                }
                else if (special) // OBJ3D case // loads the texture file once per model section, try to reduce maybe...
                {
                    uvSections = TileRenderer.ParseBndFormSections(File.ReadAllBytes(textureDirectory), "BX");
                    uvRects = ParseBxRectangles(uvSections[0].Data);
                }

                br.ReadBytes(12);
                // 4 header bytes = OBJ1
                // 4 padding bytes, always 00 00 00 00
                // 4 identifier bytes, possibly unused.
                // OBJ3D.BND
                // 00 00 00 00 = All Other Models Use This
                // 80 40 5A 00 = Switch
                // E4 40 5A 00 = Egg Husk
                // OPTOBJ.BND  = Menu Models
                // FC 56 5A 00 = Joystick
                // 8C 47 5A 00 = Camera
                // 5C 94 02 83 = Controller
                // 98 66 5A 00 = Gravis Grip Controller?
                // A4 59 5A 00 = Harddrive <-
                // 90 59 5A 00 = Harddrive ->
                // 4C 67 5A 00 = Camera X
                // 34 66 5A 00 = Keyboard
                // 88 67 5A 00 = Mouse
                // 14 68 5A 00 = Computer
                // 00 72 5A 00 = Networked Computers
                // B0 67 5A 00 = Speaker Music From Disc
                // 68 48 5A 00 = Speaker SFX
                // 9C 67 5A 00 = Headphones
                // PICKMOD.BND
                // 00 00 00 00 = All Models Use This

                int quadCount = br.ReadInt32();
                int vertexCount = br.ReadInt32();

                var quads = new List<(int A, int B, int C, int D, ushort TexIndex, byte Flags, byte Other)>();
                var vertices = new List<(short X, short Y, short Z)>();

                for (int i = 0; i < quadCount; i++)
                {
                    int a = br.ReadInt32();
                    int b = br.ReadInt32();
                    int c = br.ReadInt32();
                    int d = br.ReadInt32();
                    ushort texIndex = br.ReadUInt16();
                    byte flags = br.ReadByte();
                    byte other = br.ReadByte();
                    quads.Add((a, b, c, d, texIndex, flags, other));
                }

                for (int i = 0; i < vertexCount; i++)
                {
                    short x = br.ReadInt16();
                    short y = br.ReadInt16();
                    short z = br.ReadInt16();
                    br.ReadUInt16();                        // 127 = last vertex // 0 = vertex // 128 = first vertex ( I think )
                    vertices.Add((x, y, z));
                }

                string nameAndNumber = $"{modelName}_{modelSections[m].Name}";

                using var sw = new StreamWriter(outputPath + $"\\{nameAndNumber}.obj");

                sw.WriteLine($"# OBJ exported from Alien Trilogy {nameAndNumber}");

                sw.WriteLine($"mtllib {nameAndNumber}.mtl");
                sw.WriteLine("usemtl Texture01");

                File.WriteAllText(outputPath + $"\\{nameAndNumber}.mtl", $"newmtl Texture01\nmap_Kd {textureName}.png\n");

                // Write vertex positions
                foreach (var v in vertices)
                {
                    sw.WriteLine($"v {v.X:F4} {v.Y:F4} {v.Z:F4}");
                }

                // Store unique UVs and their indices
                var uvDict = new Dictionary<(float, float), int>();
                var uvList = new List<(float, float)>();

                // Map of per-face vertex UV indices
                var faceUvs = new List<int[]>();

                foreach (var q in quads)
                {
                    var uvIndices = new int[4];

                    if (q.TexIndex >= uvRects.Count)
                    {
                        Array.Fill(uvIndices, 1);
                        faceUvs.Add(uvIndices);
                        continue;
                    }

                    var rect = uvRects[q.TexIndex];
                    float x0 = rect.X / texSize;
                    float y0 = rect.Y / texSize;
                    float x1 = (rect.X + rect.Width) / texSize;
                    float y1 = (rect.Y + rect.Height) / texSize;

                    var baseUvs = new (float, float)[]
                    {
                        (x0, y1), // A → top-left
                        (x1, y1), // B → bottom-left
                        (x1, y0), // C → bottom-right
                        (x0, y0), // D → top-right
                    };

                    var uvs = baseUvs;

                    switch (q.Flags)
                    {
                        case 2:
                            // Triangle with special order: A → 0, C → 2, D → 3
                            uvs = new[] { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                            break;
                        case 11:
                            // Flip texture 180
                            uvs = new[] { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
                            break;
                        default:
                            // Standard quad order
                            uvs = baseUvs;
                            break;
                    }

                    for (int i = 0; i < 4; i++)
                    {
                        if (!uvDict.TryGetValue(uvs[i], out int idx))
                        {
                            idx = uvList.Count + 1;
                            uvDict[uvs[i]] = idx;
                            uvList.Add(uvs[i]);
                        }
                        uvIndices[i] = idx;
                    }

                    faceUvs.Add(uvIndices);
                }

                // Write UVs
                foreach (var uv in uvList)
                {
                    sw.WriteLine($"vt {uv.Item1:F6} {1 - uv.Item2:F6}"); // Flip Y for OBJ
                }

                // Write faces
                for (int i = 0; i < quads.Count; i++)
                {
                    var q = quads[i];
                    var uv = faceUvs[i];

                    if ((uint)q.D == 0xFFFFFFFF)
                    {
                        sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]}");
                    }
                    else
                    {
                        sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                    }
                }
            }
        }
        // Parse BX rectangles from the BX section data
        public static List<(int X, int Y, int Width, int Height)> ParseBxRectangles(byte[] bxData)
        {
            var rectangles = new List<(int X, int Y, int Width, int Height)>();
            using var ms = new MemoryStream(bxData);
            using var br = new BinaryReader(ms);
            int rectCount = br.ReadInt16();
            for (int i = 0; i < rectCount; i++)
            {
                byte x = br.ReadByte();
                byte y = br.ReadByte();
                byte width = br.ReadByte();
                byte height = br.ReadByte();
                br.ReadInt16();                         // texture identifier 00-04
                rectangles.Add((x, y, width + 1, height + 1)); // +/- 1 and no +/- all have their own problems???...
            }
            return rectangles;
        }
        // export doors or lifts as OBJ
        public static void ExportDoorLift(string levelName, List<BndSection> uvSections, byte[] levelSection, string textureName, string outputPath, bool debug, bool unknown)
        {
            using var br = new BinaryReader(new MemoryStream(levelSection));
            br.BaseStream.Seek(12, SeekOrigin.Current); // Skip 12 bytes to reach vertex and quad data
            int quadCount = br.ReadInt32();             // Number of quads
            int vertCount = br.ReadInt32();             // Number of vertices
            List<(int A, int B, int C, int D, ushort TexIndex, byte Flags, byte Other)> quads = new();
            List<(short X, short Y, short Z)> vertices = new();
            // Read quads
            for (int i = 0; i < quadCount; i++)
            {
                int a = br.ReadInt32();
                int b = br.ReadInt32();
                int c = br.ReadInt32();
                int d = br.ReadInt32();
                ushort texIndex = br.ReadUInt16();
                byte flags = br.ReadByte();
                byte other = br.ReadByte();             // unknown byte
                quads.Add((a, b, c, d, texIndex, flags, other));
            }
            // Read vertex positions
            for (int i = 0; i < vertCount; i++)
            {
                short x = br.ReadInt16();
                short y = br.ReadInt16();
                short z = br.ReadInt16();
                br.ReadUInt16();                        // padding
                vertices.Add((x, y, z));
            }
            // Read UV rectangles BX00-BX04
            var uvRects = new List<(int X, int Y, int Width, int Height)>[5];
            for (int i = 0; i < 5; i++)
            {
                uvRects[i] = ParseBxRectangles(uvSections[i].Data);
            }
            using var sw = new StreamWriter(outputPath + $"\\{levelName}.obj");

            using var mtlWriter = new StreamWriter(Path.Combine(outputPath, $"{levelName}.mtl"));
            sw.WriteLine($"# OBJ exported from Alien Trilogy {levelName}");

            sw.WriteLine($"mtllib {levelName}.mtl");

            if (debug) // show debug flags
            {
                foreach (int f in liftFlags)
                {
                    mtlWriter.WriteLine($"newmtl Texture{f:D2}");
                    mtlWriter.WriteLine($"map_Kd FLAGS{f}.png");
                }
            }
            else
            {
                for (int t = 0; t < 5; t++)
                {
                    mtlWriter.WriteLine($"newmtl Texture{t:D2}");
                    mtlWriter.WriteLine($"map_Kd {textureName}_TP{t:D2}.png");
                }
            }
            // Write vertex positions
            foreach (var v in vertices)
            {
                sw.WriteLine($"v {v.X:F4} {v.Y:F4} {v.Z:F4}");
            }
            // Store unique UVs and their indices
            var uvDict = new Dictionary<(float, float), int>();
            var uvList = new List<(float, float)>();
            // Ensure at least one dummy UV exists (for fallback cases using index 1)
            if (uvList.Count == 0)
            {
                uvDict[(0f, 0f)] = 1;
                uvList.Add((0f, 0f));
            }
            // Map of per-face vertex UV indices
            var faceUvs = new List<int[]>();
            // Loop through quads to resolve UVs
            for (int i = 0; i < quads.Count; i++)
            {
                var q = quads[i];
                var uvIndices = new int[4];

                // Resolve texture group + local UV rect index
                bool found = false;
                int texGroup = 0;
                int localIndex = q.TexIndex;
                // Iterate through texture groups to find the correct one
                for (int t = 0; t < 5; t++)
                {
                    int count = uvRects[t].Count;
                    if (localIndex < count)
                    {
                        texGroup = t;
                        found = true;
                        break;
                    }
                    localIndex -= count;
                }
                // Check if we found a valid texture group and local index
                if (!found || localIndex >= uvRects[texGroup].Count)
                {
                    // Fallback rectangle or skip invalid quad
                    faceUvs.Add(new int[] { 1, 1, 1, 1 }); // or log + continue
                    continue;
                }
                // Resolve UV rectangle
                var rect = uvRects[texGroup][localIndex];
                float x0 = rect.X / texSize;
                float y0 = rect.Y / texSize;
                float x1 = (rect.X + rect.Width) / texSize;
                float y1 = (rect.Y + rect.Height) / texSize;
                // Define base UV coordinates
                var baseUvs = new (float, float)[]
                {
                    (x0, y1), // top-left
                    (x1, y1), // bottom-left
                    (x1, y0), // bottom-right
                    (x0, y0), // top-right
                };
                var uvs = baseUvs;
                // levels and lifts
                switch (q.Flags)
                {
                    case 2:     // 0000 0010
                    case 130:   // 1000 0010
                        // Triangle with special order: A → 0, C → 2, D → 3
                        uvs = new[] { baseUvs[0], baseUvs[2], baseUvs[3], baseUvs[3] };
                        break;
                    case 11:
                    case 139:
                        // Flip texture 180
                        uvs = new[] { baseUvs[1], baseUvs[0], baseUvs[3], baseUvs[2] };
                        break;
                    default:
                        // Standard quad order
                        uvs = baseUvs;
                        break;
                }
                // Store UV indices
                for (int j = 0; j < 4; j++)
                {
                    if (!uvDict.TryGetValue(uvs[j], out int idx))
                    {
                        idx = uvList.Count + 1;
                        uvDict[uvs[j]] = idx;
                        uvList.Add(uvs[j]);
                    }
                    uvIndices[j] = idx;
                }
                faceUvs.Add(uvIndices);
            }
            // Write UVs
            foreach (var uv in uvList)
            {
                sw.WriteLine($"vt {uv.Item1:F6} {1 - uv.Item2:F6}"); // Flip Y for OBJ
            }
            // Write faces with material switching
            string currentMtl = null!;
            for (int i = 0; i < quads.Count; i++)
            {
                var q = quads[i];
                var uv = faceUvs[i];
                // Resolve which BX section this texIndex belongs to
                int texGroup = 0;
                int localIndex = q.TexIndex;
                for (int t = 0; t < 5; t++)
                {
                    int count = uvRects[t].Count;
                    if (localIndex < count)
                    {
                        texGroup = t;
                        break;
                    }
                    localIndex -= count;
                }
                // Store the material name
                string matName = $"Texture{texGroup:D2}";
                // Check if the material name has changed
                if (matName != currentMtl)
                {
                    currentMtl = matName;
                    if (debug)
                    {
                        if (q.Flags == 255) { sw.WriteLine($"usemtl Texture{q.Flags:D3}"); }
                        else { sw.WriteLine($"usemtl Texture{q.Flags:D2}"); }
                    }
                    else
                    {
                        sw.WriteLine($"usemtl {matName}");
                    }
                }
                // Faces
                if (q.D == -1)
                {
                    sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]}");
                }
                else
                {
                    sw.WriteLine($"f {q.A + 1}/{uv[0]} {q.B + 1}/{uv[1]} {q.C + 1}/{uv[2]} {q.D + 1}/{uv[3]}");
                }
            }
        }
        public static void ExportCollision(string levelName, byte[] levelSection, string outputPath)
        {
            using var br = new BinaryReader(new MemoryStream(levelSection)); // skips first 20 bytes
            ushort vertCount = br.ReadUInt16();         // Number of vertices
            ushort quadCount = br.ReadUInt16();         // Number of quads
            // uncomment if you want to read the level header
            ushort mapLength = br.ReadUInt16();         // Length of the map section
            ushort mapWidth = br.ReadUInt16();          // Width of the map section
            br.BaseStream.Seek(vertCount * 8 + quadCount * 20 + 28, SeekOrigin.Current); // Skip vertex and quad data plus unread level data

            //4
            //2
            //2
            //1
            //1
            //1
            //1
            //2
            //1
            //1

            //br.BaseStream.Seek(mapLength * mapWidth * 16, SeekOrigin.Current);
        }
    }
}
