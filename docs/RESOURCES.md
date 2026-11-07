# 资源下载与安装

## 网盘下载

**[下载 fix55h 完整配套包（百度网盘）](https://pan.baidu.com/s/1LFhJ1OKthrljqOejNbFNOw?pwd=xbct)**，提取码：`xbct`。

| 项目 | 内容 |
| --- | --- |
| 文件名 | `Metalio-Wordcard-fix55h-install-resources.zip` |
| 固件版本 | fix55h-recovery |
| 整理日期 | 2026-11-07 |
| 文件大小 | 618,157,952 字节（约 618 MB / 590 MiB） |
| 对应源码 | [`675880e`](https://github.com/chenhanlin1985-cell/metalio-wordcard/tree/675880e4f5bd1484d24ad2c8e427fe2156180b26) |
| 包内内容 | 固件、启动文件、分区表、字库、完整 SD 资源、最新词典、许可与校验清单 |
| 首次安装 | 解压后按包内 `README-先读我.md` 操作 |
| 已安装设备 | 同一包内提供升级说明；按已有版本更新应用与必要资源，保留学习记录 |

SHA-256（来自打包时的本地校验记录，供下载后核对）：

```text
abcce42a0d625afb43f972ef843642ffa81def38184428c8e859db72f2d277f9
```

Windows 可在下载目录运行：

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath '.\Metalio-Wordcard-fix55h-install-resources.zip'
```

这个包已合入 look after 等词典修正，不含个人进度、生词本或用户词表。已经运行 fix55h 的设备，无须仅为此次打包重新刷机。

不要以网上任意同名模型或词库替代配套包：文件格式、音频采样格式和缓存身份都与当前实现有关。

## 哪些资源独立分发

- 分级词库、旧课程和它们的释义、音标、例句及音频。
- 离线词典 `index.bin`、`entries.bin`，必须保持同一版本的一对文件。
- Amy Small 英语模型及 eSpeak-NG 英语语言数据。
- EFNT 字库、资源来源归属、许可证和校验清单。

SD 根目录中，学习资源的基础目录是：

```text
metalio/e-ink/wordcards/
  vocabulary/          分级词库及其 audio/ 子目录
  dictionary/          离线词典
  audio/               通用单词发音
  web-course-v1/       更多学习的课程及发音资源
  tts/
    amy-small-en-us/    Amy Small 模型
    espeak-ng-data/     eSpeak 数据
```

解压并按资源包目录结构复制，不能把整个资源目录再套进一层 ZIP 同名目录。字库是 Flash 的独立分区，不是把任意字体文件放到 SD 就能使用。

## 首次安装与升级

当前分区：应用起始 `0x20000`、字库 `0x600000`，具体以源码 `firmware/partitions.csv`、本次构建的 `flasher_args.json` 和配套包说明共同核对。首次安装需要 bootloader、分区表、应用和字库，不能只刷应用。不要套用其他 S3/S31 固件的偏移。

**已安装同一分区布局的本项目设备，通常只升级应用，不擦除整片 Flash。** 若配套资源有变更，再按包内清单更新相应资源。不要使用全盘镜像覆盖用户 SD。

网盘包用户先按包内 `README-先读我.md` 操作，文件名和目录以包内说明为准。下面示例用于从源码构建后、已经安装本项目且布局一致的设备，只更新应用；先替换串口：

```powershell
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
python -m esptool --chip esp32s31 --port COMx --baud 460800 --no-stub write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x20000 build/metalio_wordcard.bin
```

需要支持 ESP32-S31 的 esptool；串口缺失时先确认 USB 数据线和下载模式，不执行 erase-flash。先备份 SD 中个人记录、词表和生词本；升级不要覆盖 `learning-web/` 等运行记录目录，也不要删除已有发音缓存。设备正在 U 盘模式时，不同时在设备端学习；复制后安全弹出，再返回学习。

遇到 SD 无法写入，先用 [诊断工具](../tools/sd-diagnostics/先读我.md) 采集原因，不通过格式化或清空进度“解决”。

## 上传资源包时

包内只含可分发资源、许可证、版本和校验清单；排除个人 `my-words.txt`、学习日志、生词本、设备备份和测试作答数据。资源的许可独立于源码，保留逐项来源和例句归属；尚未确认再分发权利的内容不要公开上传。固件二进制需同时指向本仓库对应版本的源码与许可。
