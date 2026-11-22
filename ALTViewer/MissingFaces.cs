namespace ALTViewer
{
    // THE MISSING FACES - a global fix: the same as ALTEngine's data/MissingFaces.json (see Patch/README.txt and
    // Notes/uv-review-log.md, "Missing faces"). Applied after every byte patch, as the last of the global stages:
    //   1. the duplicate door/lift model removals, 2. the level fixes, 3. the texture fixes, 4. these.
    // Per level, against the file as the byte patches leave it (its CRC-32, Crc; FacedCrc the result):
    //   - the level's first cell, whose run start of 0 hides its faces from the game, given its run back (start 1);
    //   - with a copy of record 0, that cell's own first face, put in at record 1 (every later face moves up one),
    //     its texture and draw routine set where they need it (L232: texture 193, flipped, routine 2);
    //   - faces the level lacks put in by their corners at the head of their cell's run (L211's floor).
    // A level whose file is not the expected version is left alone (and reported); one already done is skipped.
    public static class MissingFaces
    {
        private sealed class Face
        {
            public readonly int[][] Corners;
            public readonly int Texture, Routine, Light, CellX, CellZ;
            public Face(int[][] corners, int texture, int routine, int light, int cellX, int cellZ)
            {
                Corners = corners; Texture = texture; Routine = routine; Light = light; CellX = cellX; CellZ = cellZ;
            }
        }

        private sealed class Entry
        {
            public readonly string File;
            public readonly uint Crc, FacedCrc;
            public readonly int FirstCell, RecordZeroTexture, RecordZeroRoutine;
            public readonly bool RecordZero;
            public readonly Face[] Add;
            public Entry(string file, uint crc, uint facedCrc, int firstCell, bool recordZero, int texture, int routine, Face[] add)
            {
                File = file; Crc = crc; FacedCrc = facedCrc; FirstCell = firstCell; RecordZero = recordZero;
                RecordZeroTexture = texture; RecordZeroRoutine = routine; Add = add;
            }
        }

        // Generated from ALTEngine/data/MissingFaces.json.
        private static readonly Entry[] Levels =
        {
            new Entry("SECT11/L112LEV.MAP", 0x768FC815u, 0xBF0A6DF9u, 4, true, -1, -1, null),
            new Entry("SECT11/L122LEV.MAP", 0x5B38C3A2u, 0x644EE359u, 0, true, -1, -1, null),
            new Entry("SECT11/L131LEV.MAP", 0x02E77C5Fu, 0xB0438C7Au, 26, true, -1, -1, null),
            new Entry("SECT12/L141LEV.MAP", 0xF6DD16E1u, 0x906184D4u, 26, true, -1, -1, null),
            new Entry("SECT12/L154LEV.MAP", 0x9A0740B9u, 0xC5B690EAu, 0, true, -1, -1, null),
            new Entry("SECT12/L155LEV.MAP", 0xB1A7783Bu, 0xA9BA2FFDu, 48, true, -1, -1, null),
            new Entry("SECT12/L161LEV.MAP", 0x28800DE6u, 0x8EDE8072u, 8, true, -1, -1, null),
            new Entry("SECT21/L211LEV.MAP", 0xF9E1E6ADu, 0x4C4C8C39u, 49, true, -1, -1, new[] { new Face(new[] { new[] { 3072, 2562, -12288 }, new[] { 3072, 2562, -11776 }, new[] { 3584, 2818, -11776 }, new[] { 3584, 2818, -12544 } }, 264, 0, 11, 49, 19) }),
            new Entry("SECT21/L212LEV.MAP", 0x861E51B2u, 0x6144B505u, 16, true, -1, -1, null),
            new Entry("SECT21/L213LEV.MAP", 0x98366B45u, 0x99EED08Eu, 34, true, -1, -1, null),
            new Entry("SECT21/L222LEV.MAP", 0x7ED584A8u, 0xE9B139F8u, 23, true, -1, -1, null),
            new Entry("SECT21/L231LEV.MAP", 0xA8C1A20Fu, 0xDBD5B512u, 17, true, -1, -1, null),
            new Entry("SECT21/L232LEV.MAP", 0xF589480Bu, 0x330FCF4Bu, 58, true, 193, 2, null),
            new Entry("SECT22/L242LEV.MAP", 0x9A268FA6u, 0xE7126815u, 68, true, -1, -1, null),
            new Entry("SECT22/L243LEV.MAP", 0xAB35DC70u, 0x1D471567u, 68, true, -1, -1, null),
            new Entry("SECT22/L262LEV.MAP", 0x5F8DA748u, 0x93125F64u, 18, true, -1, -1, null),
            new Entry("SECT22/L263LEV.MAP", 0xFA25366Bu, 0x30E7591Bu, 7, true, -1, -1, null),
            new Entry("SECT31/L321LEV.MAP", 0xB183E0C7u, 0x6C6C2280u, 31, false, -1, -1, null),
            new Entry("SECT31/L322LEV.MAP", 0xC9E3C16Du, 0x268856EDu, 31, false, -1, -1, null),
            new Entry("SECT31/L323LEV.MAP", 0x1FB4A46Cu, 0xD6132927u, 31, false, -1, -1, null),
            new Entry("SECT31/L324LEV.MAP", 0x36D7FB9Au, 0xC633D1A6u, 31, false, -1, -1, null),
            new Entry("SECT31/L325LEV.MAP", 0xB778B30Cu, 0x9801C972u, 31, true, -1, -1, null),
            new Entry("SECT32/L351LEV.MAP", 0x089BCA56u, 0x1BD573F3u, 39, true, -1, -1, null),
            new Entry("SECT32/L352LEV.MAP", 0x80502E21u, 0xFE2CE8B6u, 26, true, -1, -1, null),
            new Entry("SECT32/L353LEV.MAP", 0xA1F43525u, 0x6B5AF68Eu, 26, true, -1, -1, null),
            new Entry("SECT32/L371LEV.MAP", 0xE404EDD5u, 0x209C85DDu, 26, true, -1, -1, null),
            new Entry("SECT32/L381LEV.MAP", 0xE99FD537u, 0x35085A60u, 67, true, -1, -1, null),
            new Entry("SECT32/L391LEV.MAP", 0x4BDBB618u, 0x1B8A92C4u, 5, true, -1, -1, null),
            new Entry("SECT90/L901LEV.MAP", 0xF3B6B7CDu, 0x5F8D4210u, 0, true, -1, -1, null),
            new Entry("SECT90/L902LEV.MAP", 0xDB4FEC56u, 0x5C76718Au, 17, false, -1, -1, null),
            new Entry("SECT90/L903LEV.MAP", 0x57C9541Cu, 0xA0D4D95Cu, 26, true, -1, -1, null),
            new Entry("SECT90/L904LEV.MAP", 0x05372921u, 0xF5C7E4FCu, 73, true, -1, -1, null),
            new Entry("SECT90/L905LEV.MAP", 0xE3E2946Fu, 0x92FE5E74u, 0, true, -1, -1, null),
            new Entry("SECT90/L906LEV.MAP", 0x949E0B67u, 0x64DC56DAu, 23, true, -1, -1, null),
            new Entry("SECT90/L907LEV.MAP", 0xC7B9A932u, 0x58D0B54Cu, 17, true, -1, -1, null),
            new Entry("SECT90/L909LEV.MAP", 0xECCF6249u, 0x74845401u, 26, true, -1, -1, null),
        };

        private const int HeaderSize = 36, VertexSize = 8, RecordSize = 20, CellSize = 16;
        private const uint NoRun = 0xFFFFFFFFu;
        private const byte RunEnd = 0x80;

        private static string PathOf(string gameDirectory, string file)
        {
            string[] parts = file.Split('/');
            return Path.Combine(gameDirectory, parts[0], parts[1]);
        }

        // True when every level has its missing faces in.
        public static bool AllApplied(string gameDirectory)
        {
            foreach (Entry e in Levels)
            {
                string path = PathOf(gameDirectory, e.File);
                if (!File.Exists(path)) { return false; }
                byte[] b = File.ReadAllBytes(path);
                if (BinaryUtility.Crc32(b, 0, b.Length) != e.FacedCrc) { return false; }
            }
            return true;
        }

        // Puts them in. False if any level could not be done (a file missing or not the expected version).
        public static bool ApplyAll(string gameDirectory)
        {
            bool ok = true;
            foreach (Entry e in Levels)
            {
                string path = PathOf(gameDirectory, e.File);
                if (!File.Exists(path)) { ok = false; continue; }
                byte[] bytes = File.ReadAllBytes(path);
                uint crc = BinaryUtility.Crc32(bytes, 0, bytes.Length);
                if (crc == e.FacedCrc) { continue; }      // done already
                if (crc != e.Crc) { ok = false; continue; } // not the file the faces were listed for
                byte[] faced = Apply(bytes, e);
                if (faced == null || BinaryUtility.Crc32(faced, 0, faced.Length) != e.FacedCrc) { ok = false; continue; }
                File.WriteAllBytes(path, faced);
            }
            return ok;
        }

        // Takes them back out of any level that has them, exactly, so the byte patches meet the files they were
        // written for (the patch button runs again over a patched game). False if a faced level could not be undone.
        public static bool RemoveAll(string gameDirectory)
        {
            bool ok = true;
            foreach (Entry e in Levels)
            {
                string path = PathOf(gameDirectory, e.File);
                if (!File.Exists(path)) { continue; }
                byte[] bytes = File.ReadAllBytes(path);
                if (BinaryUtility.Crc32(bytes, 0, bytes.Length) != e.FacedCrc) { continue; } // not faced
                byte[] unfaced = Remove(bytes, e);
                if (unfaced == null || BinaryUtility.Crc32(unfaced, 0, unfaced.Length) != e.Crc) { ok = false; continue; }
                File.WriteAllBytes(path, unfaced);
            }
            return ok;
        }

        private static byte[] Cut(byte[] b, int at, int length)
        {
            byte[] o = new byte[b.Length - length];
            Array.Copy(b, 0, o, 0, at);
            Array.Copy(b, at + length, o, at, b.Length - at - length);
            return o;
        }

        private static bool Same(byte[] a, byte[] b)
        {
            if (a == null || b == null || a.Length != b.Length) { return false; }
            for (int i = 0; i < a.Length; i++) { if (a[i] != b[i]) { return false; } }
            return true;
        }

        // Apply, backwards: the added faces out, last first (each checked by putting it back in), then record 0's
        // copy, then the first cell's run. The caller checks the result is the file the faces were listed for.
        private static byte[] Remove(byte[] bytes, Entry e)
        {
            if (e.Add != null)
            {
                for (int i = e.Add.Length - 1; i >= 0; i--)
                {
                    byte[] before = UndoFace(bytes, e.Add[i]);
                    if (before == null) { return null; }
                    bytes = before;
                }
            }
            if (e.FirstCell >= 0)
            {
                Layout lay = Read(bytes);
                if (lay == null || e.FirstCell >= lay.Cells || Le32(bytes, lay.Grid + e.FirstCell * CellSize) != 1) { return null; }
                PutLe32(bytes, lay.Grid + e.FirstCell * CellSize, 0u);
                if (e.RecordZero)
                {
                    if (lay.RecordCount < 3) { return null; }
                    bytes = Cut(bytes, lay.Records + RecordSize, RecordSize);
                    PutLe16(bytes, lay.Data + 2, lay.RecordCount - 1);
                    int grid = lay.Grid - RecordSize;
                    for (int c = 0; c < lay.Cells; c++)
                    {
                        uint s = Le32(bytes, grid + c * CellSize);
                        if (s != NoRun && s != 0) { PutLe32(bytes, grid + c * CellSize, s - 1); }
                    }
                    PutBe32(bytes, lay.Map0 + 4, (uint)(lay.Map0Size - RecordSize));
                    PutBe32(bytes, 4, (uint)(bytes.Length - 8));
                }
            }
            return bytes;
        }

        // One added face out: its record at the head of its cell's run, and the 0-4 vertices it brought (the last
        // ones); the largest count that gives back, when the face is put in again, exactly these bytes.
        private static byte[] UndoFace(byte[] bytes, Face f)
        {
            Layout lay = Read(bytes);
            if (lay == null || f.CellX < 0 || f.CellZ < 0 || f.CellX >= lay.MapLength || f.CellZ >= lay.MapWidth) { return null; }
            int cell = f.CellZ * lay.MapLength + f.CellX;
            uint start = Le32(bytes, lay.Grid + cell * CellSize);
            if (start == NoRun || start == 0 || start >= lay.RecordCount) { return null; }
            // Most new vertices first: with fewer taken out, the ones left behind are found again when the face is
            // put back, and that checks out too.
            for (int fresh = Math.Min(f.Corners.Length, lay.VertexCount); fresh >= 0; fresh--)
            {
                byte[] b = Cut(bytes, lay.Records + (int)start * RecordSize, RecordSize);
                b = Cut(b, lay.Records - fresh * VertexSize, fresh * VertexSize);
                PutLe16(b, lay.Data, lay.VertexCount - fresh);
                PutLe16(b, lay.Data + 2, lay.RecordCount - 1);
                int grid = lay.Grid - fresh * VertexSize - RecordSize;
                for (int c = 0; c < lay.Cells; c++)
                {
                    uint s = Le32(b, grid + c * CellSize);
                    if (s != NoRun && s != 0 && s > start) { PutLe32(b, grid + c * CellSize, s - 1); }
                }
                PutBe32(b, lay.Map0 + 4, (uint)(lay.Map0Size - fresh * VertexSize - RecordSize));
                PutBe32(b, 4, (uint)(b.Length - 8));
                if (Same(AddFace((byte[])b.Clone(), f), bytes)) { return b; }
            }
            return null;
        }

        private sealed class Layout
        {
            public int Map0 = 12, Data = 20, Map0Size, VertexCount, RecordCount, MapLength, MapWidth, Vertices, Records, Grid, Cells;
        }

        private static uint Be32(byte[] b, int p) { return ((uint)b[p] << 24) | ((uint)b[p + 1] << 16) | ((uint)b[p + 2] << 8) | b[p + 3]; }
        private static void PutBe32(byte[] b, int p, uint v) { b[p] = (byte)(v >> 24); b[p + 1] = (byte)(v >> 16); b[p + 2] = (byte)(v >> 8); b[p + 3] = (byte)v; }
        private static uint Le32(byte[] b, int p) { return b[p] | ((uint)b[p + 1] << 8) | ((uint)b[p + 2] << 16) | ((uint)b[p + 3] << 24); }
        private static void PutLe32(byte[] b, int p, uint v) { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); b[p + 2] = (byte)(v >> 16); b[p + 3] = (byte)(v >> 24); }
        private static int Le16(byte[] b, int p) { return b[p] | (b[p + 1] << 8); }
        private static void PutLe16(byte[] b, int p, int v) { b[p] = (byte)v; b[p + 1] = (byte)(v >> 8); }

        private static Layout Read(byte[] b)
        {
            if (b.Length < 20 || b[0] != (byte)'F' || b[1] != (byte)'O' || b[2] != (byte)'R' || b[3] != (byte)'M'
                || b[12] != (byte)'M' || b[13] != (byte)'A' || b[14] != (byte)'P' || b[15] != (byte)'0') { return null; }
            Layout l = new Layout();
            l.Map0Size = (int)Be32(b, l.Map0 + 4);
            if (l.Data + l.Map0Size > b.Length || l.Map0Size < HeaderSize) { return null; }
            l.VertexCount = Le16(b, l.Data);
            l.RecordCount = Le16(b, l.Data + 2);
            l.MapLength = Le16(b, l.Data + 4);
            l.MapWidth = Le16(b, l.Data + 6);
            l.Vertices = l.Data + HeaderSize;
            l.Records = l.Vertices + l.VertexCount * VertexSize;
            l.Grid = l.Records + l.RecordCount * RecordSize;
            l.Cells = l.MapLength * l.MapWidth;
            if (l.Grid + l.Cells * CellSize > l.Data + l.Map0Size) { return null; }
            return l;
        }

        private static byte[] Insert(byte[] b, int at, byte[] what)
        {
            byte[] o = new byte[b.Length + what.Length];
            Array.Copy(b, 0, o, 0, at);
            Array.Copy(what, 0, o, at, what.Length);
            Array.Copy(b, at, o, at + what.Length, b.Length - at);
            return o;
        }

        private static byte[] Apply(byte[] bytes, Entry e)
        {
            Layout lay = Read(bytes);
            if (lay == null) { return null; }
            if (e.FirstCell >= 0)
            {
                if (e.FirstCell >= lay.Cells || Le32(bytes, lay.Grid + e.FirstCell * CellSize) != 0) { return null; }
                bool zeroAlone = (bytes[lay.Records + 19] & RunEnd) != 0;
                bool copied = false;
                if (e.RecordZero)
                {
                    if (lay.RecordCount < 2 || (int)Le32(bytes, lay.Records) == -1) { return null; }
                    // A copy of record 0 at record 1: every later record moves up one, and with it every run start
                    // but the empty ones (0) and none.
                    byte[] copy = new byte[RecordSize];
                    Array.Copy(bytes, lay.Records, copy, 0, RecordSize);
                    bytes = Insert(bytes, lay.Records + RecordSize, copy);
                    PutLe16(bytes, lay.Data + 2, lay.RecordCount + 1);
                    int grid = lay.Grid + RecordSize;
                    for (int c = 0; c < lay.Cells; c++)
                    {
                        uint s = Le32(bytes, grid + c * CellSize);
                        if (s != NoRun && s != 0) { PutLe32(bytes, grid + c * CellSize, s + 1); }
                    }
                    PutBe32(bytes, lay.Map0 + 4, (uint)(lay.Map0Size + RecordSize));
                    PutBe32(bytes, 4, (uint)(bytes.Length - 8));
                    copied = true;
                    lay = Read(bytes);
                    if (lay == null) { return null; }
                    int at = lay.Records + RecordSize;
                    if (e.RecordZeroTexture >= 0) { PutLe16(bytes, at + 16, e.RecordZeroTexture); }
                    if (e.RecordZeroRoutine >= 0) { bytes[at + 18] = (byte)e.RecordZeroRoutine; }
                }
                // The run starts at 1: the copy, or the rest of record 0's run where it goes on into record 1.
                if (lay.RecordCount > 1 && (copied || !zeroAlone)) { PutLe32(bytes, lay.Grid + e.FirstCell * CellSize, 1u); }
            }
            if (e.Add != null)
            {
                foreach (Face f in e.Add)
                {
                    bytes = AddFace(bytes, f);
                    if (bytes == null) { return null; }
                }
            }
            return bytes;
        }

        // One face at the head of its cell's run: each corner the vertex at exactly that point, or a new one.
        private static byte[] AddFace(byte[] bytes, Face f)
        {
            Layout lay = Read(bytes);
            if (lay == null || f.Corners.Length < 3 || f.Corners.Length > 4) { return null; }
            if (f.CellX < 0 || f.CellZ < 0 || f.CellX >= lay.MapLength || f.CellZ >= lay.MapWidth) { return null; }
            int cell = f.CellZ * lay.MapLength + f.CellX;
            uint start = Le32(bytes, lay.Grid + cell * CellSize);
            if (start == NoRun || start == 0 || start >= lay.RecordCount || lay.RecordCount >= 0xFFFF) { return null; }
            List<int[]> fresh = new List<int[]>();
            int[] index = { -1, -1, -1, -1 };
            for (int k = 0; k < f.Corners.Length; k++)
            {
                int[] p = f.Corners[k];
                for (int v = 0; v < lay.VertexCount && index[k] < 0; v++)
                {
                    int at = lay.Vertices + v * VertexSize;
                    if ((short)Le16(bytes, at) == p[0] && (short)Le16(bytes, at + 2) == p[1] && (short)Le16(bytes, at + 4) == p[2]) { index[k] = v; }
                }
                for (int n = 0; n < fresh.Count && index[k] < 0; n++)
                {
                    if (fresh[n][0] == p[0] && fresh[n][1] == p[1] && fresh[n][2] == p[2]) { index[k] = lay.VertexCount + n; }
                }
                if (index[k] < 0) { index[k] = lay.VertexCount + fresh.Count; fresh.Add(p); }
            }
            byte[] vertices = new byte[fresh.Count * VertexSize];
            for (int n = 0; n < fresh.Count; n++)
            {
                for (int axis = 0; axis < 3; axis++) { PutLe16(vertices, n * VertexSize + axis * 2, (short)fresh[n][axis]); }
            }
            byte[] record = new byte[RecordSize];
            for (int k = 0; k < 4; k++) { PutLe32(record, k * 4, (uint)index[k]); }
            PutLe16(record, 16, f.Texture);
            record[18] = (byte)f.Routine;
            record[19] = (byte)(f.Light & ~RunEnd);
            bytes = Insert(bytes, lay.Records + (int)start * RecordSize, record);
            bytes = Insert(bytes, lay.Records, vertices);
            PutLe16(bytes, lay.Data, lay.VertexCount + fresh.Count);
            PutLe16(bytes, lay.Data + 2, lay.RecordCount + 1);
            int grid = lay.Grid + fresh.Count * VertexSize + RecordSize;
            for (int c = 0; c < lay.Cells; c++)
            {
                uint s = Le32(bytes, grid + c * CellSize);
                if (s == NoRun || s == 0 || s < start || (s == start && c == cell)) { continue; }
                PutLe32(bytes, grid + c * CellSize, s + 1);
            }
            PutBe32(bytes, lay.Map0 + 4, (uint)(lay.Map0Size + fresh.Count * VertexSize + RecordSize));
            PutBe32(bytes, 4, (uint)(bytes.Length - 8));
            return bytes;
        }
    }
}
