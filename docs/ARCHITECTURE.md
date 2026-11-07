# 源码结构

| 目录 | 职责 |
| --- | --- |
| `firmware/main` | 页面、触控、音频播放、电源、USB 模式与应用调度 |
| `components/native_learning` | C++ 分级背词、单词本、FSRS、逐题记录、恢复与原生页面 |
| `components/learning_core` | 词典、共享学习规则、旧课程与测评 |
| `components/learning_web` | QuickJS-NG 与旧课程兼容桥接；主线背词不通过 HTML 页面运行 |
| `components/metalio_board` | S31 板级、触控、并口屏扫描、波形、前光与震动 |
| `components/amy_tts` / `espeak_ng` / `speech_tts` | Amy Small FP32、eSpeak-NG 与统一发音接口 |
| `src/legacy-course` | 兼容课程原始 JS/HTML/CSS 与生成数据 |
| `tools/sd-diagnostics` | Windows 设备只读诊断、SD 临时文件诊断与报告 |
| `tests` | 主机检查源码；测试运行产物不提交 |

表中 components 路径都在 firmware 下。

学习数据存于 SD，资源与用户记录分离。修改状态字段、文件格式或迁移逻辑时，应先用独立测试目录验证；不能让失败重试重复推进题目，也不能用空状态覆盖不可读的旧记录。时间不可信、磁盘满、只读卡、坏日志需要明确退出路径。

板级代码与显示波形有项目适配，不能仅因为上游版本更新就整体覆盖。USB 模式切换必须保证 SD 访问的所有权交接；发音优先资源与缓存，不能每次重新生成或全量扫描。

旧 TTS 实验、历次网页原型、开发机脚本、私有运行证据不纳入此仓库。保留下来的整型内核/参考实现只是当前组件中的辅助代码，不代表界面提供多个生产语音引擎。当前对外使用 Amy Small FP32 与 eSpeak 托底。
