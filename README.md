# 小小单词卡 · Metalio Wordcard

运行在无名 **Metalio E-INK4 Plus（ESP32-S31、4.7 英寸前光墨水屏）** 上的离线英语单词机。

以分级背词为主线，也可以把自己书上、工作中遇到的英文整理成词表导入，在设备上查词、听发音、练词义和拼写、复习自己的单词。

## 下载 fix65

**[下载 S31 完整安装包（782 MiB）](https://github.com/chenhanlin1985-cell/metalio-wordcard/releases/download/fix65/Metalio-S31-fix65-full.zip)**。适用于 ESP32-S31 版 Metalio E-INK4 Plus。包内含固件、字库、词典、离线语音、六本故事书和对应源码。[安装步骤与 SHA-256](docs/RESOURCES.md)

## 学习功能

- 每组五个词可以前后翻看。词卡正面列出前缀、词根、后缀或复合词各部分及其中文释义，点「听例句」可查看英文和译文。
- 分级词库、PET 和 GRE 试学共用学习和复习流程。跨词库、跨故事确认是同一词义时，共用学习记录和复习日期。词义和拼写分别记录。
- 六本故事书分为分段练词、阅读理解和分支故事。《口袋里的收据》将新词放在短段落里，答错订正后，隔几个词再练。
- 支持导入个人词表、离线查词与发音。可以从已学词入口重新打开词卡。

| 需求 | 文档 |
| --- | --- |
| 获取固件与 SD 资源 | [资源下载与安装](docs/RESOURCES.md) |
| 制作或添加故事书 | [故事书内容规范](docs/故事书内容规范.md) |
| 从源码编译 | [构建说明](docs/BUILD.md) |
| 导入自己的英文词表 | [自定义词表](docs/WORDLISTS.md) |
| SD 无法写入或导入失败 | [SD 诊断工具](tools/sd-diagnostics/先读我.md) |
| 修改代码、了解架构 | [源码结构](docs/ARCHITECTURE.md) |
| 查看许可与依赖 | [许可范围](LICENSE-NOTICE.md) · [上游来源](docs/UPSTREAM.md) |

固件版本为 `fix65-card-examples`。发布用应用的 SHA-256 和固定 SDK 提交见 [构建记录](build-lock.json)。

## 仓库包含什么

`firmware/` 是可编译的 S31 固件及必要的适配代码；`src/legacy-course/` 是旧课程兼容模块的生成输入；`tools/` 包含生成器与诊断工具；`tests/` 包含主机检查。

为保证直接构建，保留了必要的生成 C/C++ 文件（内置兼容词表、课程数据、屏幕波形和关机插画）。大容量 SD 词库、音频、模型和字库单独分发。开发缓存、个人进度、生词本、照片、设备备份和测试运行数据均不在仓库中。

## 许可

保留本仓库原有的 [MIT LICENSE](LICENSE)，适用于无其他许可声明的项目原创部分；第三方及其派生文件保留各自许可。**包含 eSpeak-NG 的组合固件按 GPL-3.0-or-later 分发**，不能把整个固件及所有资源统一视为 MIT。详见 [LICENSE-NOTICE.md](LICENSE-NOTICE.md) 与 [COPYING](COPYING)。
