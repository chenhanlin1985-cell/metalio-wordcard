import os,sys,subprocess
assert os.environ.get('PYTHONUTF8')=='1'
assert os.environ.get('PYTHONIOENCODING')=='utf-8'
assert sys.stdout.encoding.lower().replace('-','')=='utf8'
assert sys.stderr.encoding.lower().replace('-','')=='utf8'
print('中文 Français 日本語 ✓')
print('stderr 한국어 ✓',file=sys.stderr)
subprocess.run([sys.executable,'-c',"import os,sys;assert os.environ.get('PYTHONUTF8')=='1';assert os.environ.get('PYTHONIOENCODING')=='utf-8';print('子进程 UTF8 ✓');print('stderr Español ✓',file=sys.stderr)"],check=True)
