# 下载与安装 fix65

[下载 Windows 完整安装包（782 MiB）](https://github.com/chenhanlin1985-cell/metalio-wordcard/releases/download/fix65/Metalio-S31-fix65-full.zip) · [查看 GitHub Release](https://github.com/chenhanlin1985-cell/metalio-wordcard/releases/tag/fix65)

设备型号为 Metalio E-INK4 Plus（ESP32-S31，4.7 英寸）。文件名为 `Metalio-S31-fix65-full.zip`，大小为 819,823,555 字节，SHA-256 如下。

```text
635405914cb7353e38e00cb2b21d821591d67360bd1aaaae2a4ea5d1942b70d0
```

Windows 下可运行 `Get-FileHash -Algorithm SHA256 .\Metalio-S31-fix65-full.zip` 核对下载文件。

## 首次安装或从最早版升级

1. 把 SD 卡内容复制到电脑备份，特别是学习记录、个人词表和生词本。最早版的学习记录迁移取决于原固件版本。
2. 在 Windows 上完整解压 ZIP。用 USB 数据线连接 S31，双击 `开始刷机.cmd`，选 2「首次安装」，确认实际串口。安装器会按清单写入启动程序、分区表、应用、字库和离线语音模型。
3. 用读卡器接入 SD 卡，双击 `安装SD资源.cmd`，选 1「全部安装」，确认 SD 卡盘符。完成后安全弹出，装回设备并重启。
4. 在「内容库 → 故事书」打开《口袋里的收据》。两个安装器都可离线运行。

## 以后分别更新

- 更新固件时运行 `开始刷机.cmd`。使用相同分区布局、字库和模型的设备可选择只更新应用。
- 更新词库、语音或故事书时运行 `安装SD资源.cmd`，选 2，选择所需资源。安装故事会检查它依赖的词库和版本。重复安装会校验并跳过相同文件。
- 添加自己的故事书时，可从仓库内同类型的书复制一份，修改正文和清单，再用内容打包工具校验。[故事书内容规范](故事书内容规范.md)

包内 `05-许可与源码` 提供对应固件的构建源码和第三方许可。遇到 SD 无法写入时，可使用[SD 诊断工具](../tools/sd-diagnostics/先读我.md)。
