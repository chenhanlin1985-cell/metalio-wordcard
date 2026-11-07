using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class WordcardDiagIO {
    [DllImport("kernel32.dll")] private static extern uint GetOEMCP();
    // chkdsk writes OEM bytes when redirected. Console.OutputEncoding does not
    // change its pipe encoding. Use the system OEM code page without changing it.
    public static Encoding NativeDiskEncoding() {return Encoding.GetEncoding((int)GetOEMCP(),EncoderFallback.ExceptionFallback,DecoderFallback.ExceptionFallback);}
    public static uint Fnv(string s) { unchecked { uint h=2166136261; foreach(byte b in Encoding.UTF8.GetBytes(s)) h=(h^b)*16777619; return h; } }
    public static string HashText(string s) { using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(Encoding.UTF8.GetBytes(s))).Replace("-","").ToLowerInvariant(); }
    public static string HashFile(string p) { using(var f=new FileStream(p,FileMode.Open,FileAccess.Read,FileShare.Read)) using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(f)).Replace("-","").ToLowerInvariant(); }
    public static int ErrorCode(Exception e) { while(e.InnerException!=null)e=e.InnerException; return e.HResult & 65535; }
    public static void SafePath(string root,string path) {
        root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        path=Path.GetFullPath(path);
        if(!path.StartsWith(root,StringComparison.OrdinalIgnoreCase) && path.TrimEnd('\\')!=root.TrimEnd('\\')) throw new IOException("Path escapes selected SD root");
        string cur=path;
        while(cur.Length>=root.TrimEnd('\\').Length) {
            if((File.Exists(cur)||Directory.Exists(cur)) && (File.GetAttributes(cur)&FileAttributes.ReparsePoint)!=0) throw new IOException("Reparse point refused");
            var p=Path.GetDirectoryName(cur.TrimEnd('\\'));if(String.IsNullOrEmpty(p)||p==cur) break;cur=p;
        }
    }
    public sealed class ProbeResult {
        public string directory,stage="not-started",leftover="";
        public bool passed=false,cleaned=false;
        public int win32=0;
        public long milliseconds=0;
        public List<string> completed=new List<string>();
    }
    // Only random CreateNew scratch files. Never opens an existing file for writing.
    public static ProbeResult Probe(string root,string dir) {
        var r=new ProbeResult();r.directory=dir.Substring(root.TrimEnd('\\').Length).TrimStart('\\');
        string id=Guid.NewGuid().ToString("N");string a=Path.Combine(dir,".__wcdiag_"+id+".tmp"),b=Path.Combine(dir,".__wcdiag_"+id+".moved");
        bool ownsA=false,ownsB=false;var timer=System.Diagnostics.Stopwatch.StartNew();
        try {
            SafePath(root,dir);SafePath(root,a);SafePath(root,b);
            r.stage="create";byte[] bytes=new byte[4096];for(int i=0;i<bytes.Length;i++)bytes[i]=(byte)((i*37+11)&255);
            using(var f=new FileStream(a,FileMode.CreateNew,FileAccess.Write,FileShare.None,4096,FileOptions.WriteThrough)) {
                ownsA=true;r.completed.Add("create");r.stage="write";f.Write(bytes,0,bytes.Length);r.completed.Add("write");
                r.stage="flush";f.Flush(true);r.completed.Add("flush");
            }
            r.stage="append";
            using(var f=new FileStream(a,FileMode.Open,FileAccess.Write,FileShare.None,4096,FileOptions.WriteThrough)) {f.Seek(0,SeekOrigin.End);f.Write(bytes,0,bytes.Length);f.Flush(true);}
            r.completed.Add("append");r.stage="readback";
            var actual=File.ReadAllBytes(a);if(actual.Length!=8192)throw new IOException("Readback length mismatch");
            for(int i=0;i<actual.Length;i++)if(actual[i]!=bytes[i%4096])throw new IOException("Readback data mismatch");
            r.completed.Add("readback");r.stage="rename";File.Move(a,b);ownsA=false;ownsB=true;r.completed.Add("rename");
            r.stage="delete";File.Delete(b);ownsB=false;r.completed.Add("delete");r.passed=true;r.stage="complete";
        } catch(Exception e) {r.win32=ErrorCode(e);} finally {
            // Delete only the exact random files this invocation successfully created.
            try {if(ownsA){File.Delete(a);ownsA=false;}if(ownsB){File.Delete(b);ownsB=false;}} catch {r.leftover=Path.GetFileName(ownsA?a:b);}
            r.cleaned=!ownsA&&!ownsB;r.milliseconds=timer.ElapsedMilliseconds;
        }
        return r;
    }
}
