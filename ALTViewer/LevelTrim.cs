namespace ALTViewer
{
    // TRIMMED LEVELS - reading ALTEngine's levels (ALTEngine/src/Formats/LevelTrim.h, the same layout).
    //
    // An ALTEngine install takes out of each level file the bytes the game never reads, or reads as one fixed value,
    // or clears when the level starts (its "Padding Trim", the last step of its install, ALTEngine only). The level
    // chunk is then MAPT instead of MAP0, with shorter records; every other chunk is as it was. ALTViewer never makes
    // such files - its patch button writes files the original game runs, and refuses to patch trimmed ones - but it
    // reads them: ReadLevel puts a trimmed level back into the MAP0 layout, the fixed bytes with their value and the
    // never-read and level-start ones as 0, so everything that reads MAP0 works on either.
    //
    //   record        bytes   dropped
    //   header        36->32  +28,+29 (always 0x40 0x40); +34,+35 (vent types, never read)
    //   vertex         8->6   +6,+7 (always 0)
    //   face          20      -
    //   cell          16->13  +4, +7 (always 0); +12 (occupancy, cleared at level start)
    //   path node      8->7   +2 (always 0)
    //   monster       20->19  +9 (never read)
    //   pickup         8->6   +5 (always 0); +7 (never read)
    //   object        16->12  +5 (10 on crate types 20, 25, 29, 32, else 0); +11, +13, +15 (always 0)
    //   door           8->7   +5 (always 0)
    //   lift          16->10  +4, +11 (always 0); +6 (always 1); +13..15 (never read)
    //   action group   4->3   +3 (always 0)
    //   logic group    4      -
    //   light         28->24  +3, +7, +11, +15 (always 0)
    //   texture animators, animation frames: 4 each, unchanged
    public static class LevelTrim
    {
        private const int Zeroed = -1, CrateFlag = -2;

        // Per record type: its MAP0 size, then (offset, value put back) for each dropped byte, ascending.
        private static readonly int[][] Header = { new[] { 36 }, new[] { 28, 0x40 }, new[] { 29, 0x40 }, new[] { 34, Zeroed }, new[] { 35, Zeroed } };
        private static readonly int[][] Vertex = { new[] { 8 }, new[] { 6, 0 }, new[] { 7, 0 } };
        private static readonly int[][] Face = { new[] { 20 } };
        private static readonly int[][] Cell = { new[] { 16 }, new[] { 4, 0 }, new[] { 7, 0 }, new[] { 12, Zeroed } };
        private static readonly int[][] PathNode = { new[] { 8 }, new[] { 2, 0 } };
        private static readonly int[][] Monster = { new[] { 20 }, new[] { 9, Zeroed } };
        private static readonly int[][] Pickup = { new[] { 8 }, new[] { 5, 0 }, new[] { 7, Zeroed } };
        private static readonly int[][] Object = { new[] { 16 }, new[] { 5, CrateFlag }, new[] { 11, 0 }, new[] { 13, 0 }, new[] { 15, 0 } };
        private static readonly int[][] Door = { new[] { 8 }, new[] { 5, 0 } };
        private static readonly int[][] Lift = { new[] { 16 }, new[] { 4, 0 }, new[] { 6, 1 }, new[] { 11, 0 }, new[] { 13, Zeroed }, new[] { 14, Zeroed }, new[] { 15, Zeroed } };
        private static readonly int[][] Action = { new[] { 4 }, new[] { 3, 0 } };
        private static readonly int[][] Logic = { new[] { 4 } };
        private static readonly int[][] Light = { new[] { 28 }, new[] { 3, 0 }, new[] { 7, 0 }, new[] { 11, 0 }, new[] { 15, 0 } };
        private static readonly int[][] Word = { new[] { 4 } };

        private static int Le16(byte[] b, int p) { return b[p] | (b[p + 1] << 8); }
        private static uint Be32(byte[] b, int p) { return ((uint)b[p] << 24) | ((uint)b[p + 1] << 16) | ((uint)b[p + 2] << 8) | b[p + 3]; }
        private static void PutBe32(byte[] b, int p, uint v) { b[p] = (byte)(v >> 24); b[p + 1] = (byte)(v >> 16); b[p + 2] = (byte)(v >> 8); b[p + 3] = (byte)v; }

        // Where the chunk with this tag starts (its data) and how long it is; -1 if none.
        private static int FindChunk(byte[] file, string tag, out int length)
        {
            length = 0;
            if (file.Length < 12 || file[0] != 'F' || file[1] != 'O' || file[2] != 'R' || file[3] != 'M') { return -1; }
            int pos = 12;
            while (pos + 8 <= file.Length)
            {
                int n = (int)Be32(file, pos + 4);
                if (n < 0 || pos + 8 + n > file.Length) { return -1; }
                if (file[pos] == tag[0] && file[pos + 1] == tag[1] && file[pos + 2] == tag[2] && file[pos + 3] == tag[3]) { length = n; return pos + 8; }
                pos += 8 + n + (n & 1);
            }
            return -1;
        }

        // True when the level file is trimmed (its level chunk is MAPT).
        public static bool IsTrimmed(byte[] file) { return FindChunk(file, "MAPT", out _) >= 0; }
        public static bool IsTrimmed(string path) { return File.Exists(path) && IsTrimmed(File.ReadAllBytes(path)); }

        // True when any level in the game folder (SECT??/L???LEV.MAP) is trimmed: an ALTEngine install.
        public static bool AnyTrimmed(string gameDirectory)
        {
            if (!Directory.Exists(gameDirectory)) { return false; }
            foreach (string sect in Directory.GetDirectories(gameDirectory, "SECT*"))
            {
                foreach (string level in Directory.GetFiles(sect, "L*LEV.MAP"))
                {
                    // The tag is at byte 12 of a level file: read only that much.
                    byte[] head = new byte[16];
                    int got;
                    using (FileStream stream = File.OpenRead(level)) { got = stream.Read(head, 0, 16); }
                    if (got == 16 && head[12] == 'M' && head[13] == 'A' && head[14] == 'P' && head[15] == 'T') { return true; }
                }
            }
            return false;
        }

        // True for a game folder ALTEngine installed and patches: its levels are trimmed, or its CD folder carries the
        // LevelCleanup.json ALTEngine writes at every start. Its patches, global and its own, are already in, and its
        // levels are not the files ALTViewer's byte patches were written for (cleaned, maybe trimmed).
        public static bool IsEngineInstall(string gameDirectory)
        {
            return File.Exists(System.IO.Path.Combine(gameDirectory, "LevelCleanup.json")) || AnyTrimmed(gameDirectory);
        }

        // The level file in the MAP0 layout, whichever it is on disk. Use it wherever a level file is read for its MAP0.
        public static byte[] ReadLevel(string path) { return Expand(File.ReadAllBytes(path)); }

        // A trimmed level file back to MAP0 (a file that is not trimmed comes back as it is).
        public static byte[] Expand(byte[] file)
        {
            int at = FindChunk(file, "MAPT", out int length);
            if (at < 0) { return file; }
            if (length < 32) { throw new InvalidDataException("The trimmed level's header is cut short."); }
            var sections = new (int[][] Type, int Count)[]
            {
                (Header, 1), (Vertex, Le16(file, at + 0)), (Face, Le16(file, at + 2)), (Cell, Le16(file, at + 4) * Le16(file, at + 6)), (PathNode, file[at + 12]), (Monster, Le16(file, at + 14)),
                (Pickup, Le16(file, at + 16)), (Object, Le16(file, at + 18)), (Door, Le16(file, at + 20)), (Lift, Le16(file, at + 22)), (Action, 64), (Logic, 64), (Light, 128),
                (Word, Le16(file, at + 26)), (Word, Le16(file, at + 28)),
            };
            var full = new List<byte>(length + length / 8);
            int pos = at, end = at + length;
            foreach (var section in sections)
            {
                int[][] type = section.Type;
                int count = section.Count;
                int size = type[0][0], dropped = type.Length - 1;
                if (pos + count * (size - dropped) > end) { throw new InvalidDataException("The trimmed level's records run past its chunk."); }
                for (int r = 0; r < count; r++)
                {
                    int start = full.Count;
                    int next = 1;
                    for (int k = 0; k < size; k++)
                    {
                        if (next < type.Length && type[next][0] == k) { full.Add(0); next++; continue; }
                        full.Add(file[pos++]);
                    }
                    for (int d = 1; d < type.Length; d++)
                    {
                        int value = type[d][1];
                        if (value == CrateFlag) { byte t = full[start + 2]; value = (t == 20 || t == 25 || t == 29 || t == 32) ? 10 : 0; }
                        full[start + type[d][0]] = (byte)(value == Zeroed ? 0 : value);
                    }
                }
            }
            if (pos != end) { throw new InvalidDataException("The trimmed level's chunk does not end where its counts say."); }
            // The file with its level chunk back as MAP0, padded to an even length, the FORM size following.
            var output = new List<byte>(file.Length + full.Count - length);
            for (int i = 0; i < at - 8; i++) { output.Add(file[i]); }
            output.AddRange(new byte[] { (byte)'M', (byte)'A', (byte)'P', (byte)'0', 0, 0, 0, 0 });
            int sizeAt = output.Count - 4;
            output.AddRange(full);
            if ((full.Count & 1) != 0) { output.Add(0); }
            int after = at + length + (length & 1);
            for (int i = after; i < file.Length; i++) { output.Add(file[i]); }
            byte[] result = output.ToArray();
            PutBe32(result, sizeAt, (uint)full.Count);
            PutBe32(result, 4, (uint)(result.Length - 8));
            return result;
        }
    }
}
