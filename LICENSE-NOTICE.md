# 许可范围

根目录 `LICENSE` 保留仓库原有的 MIT 文本，适用于无其他声明的项目原创代码。它不会重新许可第三方代码、第三方派生修改、模型、字体或教学资源。文件内版权头和更具体的许可优先。

本项目链接 eSpeak-NG，**组合固件按 GPL-3.0-or-later 分发**，GPL 全文在 `COPYING`。`tools/sd-diagnostics/` 沿用此前明确的 GPL-3.0-or-later。Amy 中有单独 Apache-2.0 声明的整型实现，保留 `INTEGER-NOTICE.md`；其余 Amy 内核与前端按各自上游声明。

发布固件二进制时，应同时提供对应版本源码、构建说明和许可证，不能只以根目录 MIT 声明代表整个二进制。相关依据：[GNU 许可兼容性说明](https://www.gnu.org/licenses/license-compatibility.html)、[GNU 许可证常见问题](https://www.gnu.org/licenses/gpl-faq.html#GPLModuleLicense)。

| 组件 | 许可 / 文本 |
| --- | --- |
| Metalio BSP | MIT 及各子文件原有声明，`licenses/Metalio.txt` |
| eSpeak-NG | GPL-3.0-or-later，`COPYING` 与源文件版权头 |
| Amy / sanoTTS 派生推理代码 | `licenses/Amy-LICENSE`、`licenses/Amy-MIT.txt`；MIT 仅用于上游声明范围，前端组合仍遵循 GPL |
| ESP-IDF / esp_tinyusb | Apache-2.0 |
| TinyUSB / QuickJS-NG / nlohmann-json | 各自 MIT，见 `licenses/` 与 vendor 内许可 |
| FSRS-6 方程移植 | ts-fsrs 6.0.0，MIT，`licenses/LICENSE-ts-fsrs.txt` |

模型权重、词典、词库、语音及字体未随本仓库分发；它们不自动继承上述 MIT 或 GPL。外部资源包需保留对应的模型说明、Noto OFL、词典与 CEFR-J 来源、Tatoeba/ManyThings 逐句归属等材料；不能把出版教材目录参考当成整册教材授权。

源码内的旧课程为项目编写的兼容练习，不是出版社教材或多邻国/百词斩产品内容的完整开源版本。生成关机插画的原始素材与编码器已随源码保留。具体第三方固定版本见 [UPSTREAM.md](docs/UPSTREAM.md)。
