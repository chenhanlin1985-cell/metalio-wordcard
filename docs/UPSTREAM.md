# 上游与固定版本

| 来源 | 固定版本 / 说明 |
| --- | --- |
| [ESP-IDF](https://github.com/espressif/esp-idf) | v6.1 / `fff9895c82d744c7237be8847347bdd1b07c6643` |
| [Metalio-E-INK4-Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus) | `b400605e9a6a276a2c39864c0cff63798c78e8d8`，保留项目适配后的 BSP |
| [arduino-espeak-ng](https://github.com/pschatzmann/arduino-espeak-ng) | `9376e2a2a57d41351b234ae24b67a373e9c7e6f2`，组件中是编译所用 C 源码与 S31 适配 |
| [QuickJS-NG](https://github.com/quickjs-ng/quickjs) | v0.17.0 / `6d46d07d04041b40f4f49eaa7fdebe44c314c699`，保留中断调度适配 |
| [Amy / sanoTTS 内核](https://github.com/Ampixa/sanoTTS) | 原有来源记录固定 `f73b8382bc98a1c8de3488ba30d727ab82587828`，实际使用的代码已随组件提交 |
| [Amy Small 模型来源](https://huggingface.co/ampixa/sanoTTS) | `c532a5d21c078a16cb633718e9182bfd71a5b760`；模型未提交，不能直接拿任意权重替换配套格式 |
| [ts-fsrs](https://github.com/open-spaced-repetition/ts-fsrs) | 6.0.0 / FSRS-6，C++ 移植见 `vocabulary_fsrs.cpp` |
| ESP TinyUSB / TinyUSB | 2.4.0 / 0.21.0~2，见 `firmware/dependencies.lock` |

USB 下载组件在公开整理前与其 CHECKSUMS.json 全量比对，未发现本地修改，因此由组件管理器获取，不再重复提交。主应用对 MSC 回调的编译期适配保留在 `firmware/main/CMakeLists.txt`。

这些来源与版本用于追溯当前实现，不代表项目由上游作者背书；各源文件原始作者与许可证均保留。
