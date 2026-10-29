/// <summary>
/// The BinaryUtility class is used to replace bytes at specified locations in a file.
/// </summary>
public static class BinaryUtility
{
    /// <summary>
    /// Reads a single byte from *path* at *offset* and disposes the stream automatically.
    /// </summary>
    public static byte ReadByteAtOffset(string path, long offset)
    {
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read);
        using var reader = new BinaryReader(stream);
        stream.Seek(offset, SeekOrigin.Begin);  // seek to the absolute offset
        return reader.ReadByte();               // read the byte
    }
    /// <summary>
    /// Reads a single byte like ReadByteAtOffset, but returns -1 instead of throwing when the file is missing
    /// or shorter than *offset*.
    /// </summary>
    public static int TryReadByteAtOffset(string path, long offset)
    {
        if (!File.Exists(path)) { return -1; }
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read);
        if (offset < 0 || offset >= stream.Length) { return -1; }
        stream.Seek(offset, SeekOrigin.Begin);
        return stream.ReadByte();
    }
    /// <summary>
    /// The ReplaceBndFrameWith8ByteAlignment method replaces a section of a BND file with new data, ensuring that the new data is aligned to 8-byte boundaries.
    /// </summary>
    public static void ReplaceBndFrameWith8ByteAlignment(string filePath, long offset, int lengthToReplace, byte[] newData)
    {
        int padding = (8 + ((int)offset + newData.Length % 8)) % 8;
        byte[] paddedNewData = new byte[newData.Length + padding];
        Array.Copy(newData, paddedNewData, newData.Length);
        ReplaceBytesWithResize(new List<(long, int, byte[])> { (offset, lengthToReplace, paddedNewData) }, filePath);
    }
    /// <summary>
    /// The ReplaceBytesWithResize method opens the relevant file to replace bytes in, then replaces a given amount of bytes and inserts the rest, extending the file length.
    /// </summary>
    public static void ReplaceBytesWithResize(List<(long Offset, int LengthToReplace, byte[] NewData)> edits, string filePath)
    {
        // Sort edits by offset to apply them sequentially
        edits = edits.OrderBy(e => e.Offset).ToList();
        byte[] original = File.ReadAllBytes(filePath);
        var result = new List<byte>();
        long currentPos = 0;
        long shift = 0;
        foreach (var edit in edits)
        {
            long adjustedOffset = edit.Offset + shift;
            // Copy unchanged data before this edit
            if (adjustedOffset > currentPos)
            {
                int length = (int)(adjustedOffset - currentPos);
                result.AddRange(original.Skip((int)currentPos).Take(length));
                currentPos += length;
            }
            // Skip the original data to be replaced
            currentPos += edit.LengthToReplace;
            // Insert new data
            result.AddRange(edit.NewData);
            // Update shift amount for later edits
            shift += edit.NewData.Length - edit.LengthToReplace;
        }
        // Append any remaining data after the last edit
        if (currentPos < original.Length)
        {
            result.AddRange(original.Skip((int)currentPos));
        }
        // Write back to file
        File.WriteAllBytes(filePath, result.ToArray());
    }
    /// <summary>
    /// The ReplaceByte method opens the relevant file to replace a byte in.
    /// </summary>
    /// <param name="offset">The address at which to replace a byte.</param>
    /// <param name="value">The byte to write as a replacement.</param>
    /// <param name="filename">The address of the file.</param>
    public static void ReplaceByte(long offset, byte value, string filename)
    {
        using var fs = new FileStream(filename, FileMode.Open, FileAccess.Write, FileShare.None);
        fs.Seek(offset, SeekOrigin.Begin);
        fs.WriteByte(value);
    }
    /// <summary>
    /// The ReplaceByte method opens the relevant file to replace multiple bytes in a sequence.
    /// </summary>
    /// <param name="replacements">The bytes to replace and the address at which to replace them.</param>
    /// <param name="filename">The address of the file.</param>
    public static void ReplaceBytes(List<Tuple<long, byte[]>> replacements, string filename)
    {
        using (var stream = new FileStream(filename, FileMode.Open, FileAccess.ReadWrite))
        {
            using (var reader = new BinaryReader(stream))
            {
                using (var writer = new BinaryWriter(stream))
                {
                    Replace(reader, writer, replacements);
                }
            }
        }
    }
    /// <summary>
    /// The ReplaceBytesAcrossFiles method replaces bytes across different files.
    /// Note : That the arrays must be constructed so that the indexes match.
    /// </summary>
    /// <param name="replacements">The bytes to replace and the address at which to replace them.</param>
    /// <param name="filename">The addresses of the files.</param>
    public static void ReplaceBytesAcrossFiles(List<Tuple<long, byte[]>>[] replacements, string[] filename)
    {
        for (int i = 0; i < filename.Length; i++)
        {
            ReplaceBytes(replacements[i], filename[i]);
        }
    }
    /// <summary>
    /// The Replace method is called by the ReplaceBytes method.
    /// </summary>
    /// <param name="reader">The BinaryReader Object.</param>
    /// <param name="writer">The BinaryWriter Object.</param>
    /// <param name="replacements">The bytes to replace and the address at which to replace them.</param>
    public static void Replace(BinaryReader reader, BinaryWriter writer, IEnumerable<Tuple<long, byte[]>> replacements)
    {
        byte[] bytes = new byte[reader.BaseStream.Length];
        reader.BaseStream.Position = 0;
        reader.Read(bytes, 0, bytes.Length);
        foreach (var replacement in replacements)
        {
            Array.Copy(replacement.Item2, 0, bytes, replacement.Item1, replacement.Item2.Length);
        }
        writer.BaseStream.Position = 0;
        writer.BaseStream.SetLength(0);
        writer.Write(bytes);
    }
    /// <summary>
    /// The RemoveFormChunks method removes chunks (by tag) from a FORM file such as a level .MAP, rewrites the FORM size and then applies the record edits.
    /// Each chunk is removed only if it is present with the expected size and CRC-32 of its payload. If none of them is present and the record edits are
    /// already in place it does nothing, so it is safe to run again. Anything in between leaves the file untouched and returns false.
    /// The record edits must lie before the first removed chunk (the door and lift records are in MAP0, the first chunk).
    /// </summary>
    public static bool RemoveFormChunks(string filename, FormChunk[] chunks, List<Tuple<long, byte[]>> recordEdits)
    {
        byte[] bytes = File.ReadAllBytes(filename);
        if (bytes.Length < 12 || bytes[0] != 'F' || bytes[1] != 'O' || bytes[2] != 'R' || bytes[3] != 'M') { return false; }
        var found = new List<(long Pos, long Span)>();
        long pos = 12;
        while (pos + 8 <= bytes.Length)
        {
            string tag = System.Text.Encoding.ASCII.GetString(bytes, (int)pos, 4);
            uint size = (uint)((bytes[pos + 4] << 24) | (bytes[pos + 5] << 16) | (bytes[pos + 6] << 8) | bytes[pos + 7]);
            long span = 8 + size + (size & 1);
            if (pos + 8 + size > bytes.Length) { return false; }
            foreach (var chunk in chunks)
            {
                if (chunk.Tag != tag) { continue; }
                if (chunk.Size != size || Crc32(bytes, pos + 8, size) != chunk.Crc32) { return false; } // not the expected chunk
                found.Add((pos, Math.Min(span, bytes.Length - pos)));
            }
            pos += span;
        }
        if (found.Count == 0)
        {
            foreach (var edit in recordEdits)
            {
                for (int i = 0; i < edit.Item2.Length; i++) { if (bytes[edit.Item1 + i] != edit.Item2[i]) { return false; } }
            }
            return true; // already removed
        }
        if (found.Count != chunks.Length) { return false; } // only some of them present
        foreach (var edit in recordEdits)
        {
            foreach (var f in found) { if (edit.Item1 + edit.Item2.Length > f.Pos) { return false; } }
        }
        var result = new List<byte>(bytes.Length);
        long copied = 0;
        foreach (var f in found.OrderBy(f => f.Pos))
        {
            result.AddRange(new ArraySegment<byte>(bytes, (int)copied, (int)(f.Pos - copied)));
            copied = f.Pos + f.Span;
        }
        if (copied < bytes.Length) { result.AddRange(new ArraySegment<byte>(bytes, (int)copied, (int)(bytes.Length - copied))); }
        byte[] output = result.ToArray();
        foreach (var edit in recordEdits) { Array.Copy(edit.Item2, 0, output, edit.Item1, edit.Item2.Length); }
        uint formSize = (uint)(output.Length - 8);
        output[4] = (byte)(formSize >> 24); output[5] = (byte)(formSize >> 16); output[6] = (byte)(formSize >> 8); output[7] = (byte)formSize;
        File.WriteAllBytes(filename + ".tmp", output);
        File.Move(filename + ".tmp", filename, true);
        return true;
    }
    /// <summary>
    /// The Crc32 method returns the standard CRC-32 (polynomial 0xEDB88320) of part of a byte array.
    /// </summary>
    public static uint Crc32(byte[] data, long offset, long length)
    {
        uint crc = 0xFFFFFFFF;
        for (long i = offset; i < offset + length; i++)
        {
            crc ^= data[i];
            for (int k = 0; k < 8; k++) { crc = (crc & 1) != 0 ? 0xEDB88320 ^ (crc >> 1) : crc >> 1; }
        }
        return ~crc;
    }
}
/// <summary>
/// A chunk of a FORM file for BinaryUtility.RemoveFormChunks to remove: its tag, its size field and the CRC-32 of its payload.
/// </summary>
public class FormChunk
{
    public string Tag;
    public uint Size;
    public uint Crc32;
    public FormChunk(string tag, uint size, uint crc32) { Tag = tag; Size = size; Crc32 = crc32; }
}
