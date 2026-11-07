# 从源码构建

## 固定环境

- 硬件：Metalio E-INK4 Plus / ESP32-S31，16 MB Flash、16 MB PSRAM，ED047TC2 4.7 英寸屏。
- ESP-IDF v6.1，提交 `fff9895c82d744c7237be8847347bdd1b07c6643`。
- RISC-V GCC `esp-15.2.0_20251204`。
- `esp_tinyusb == 2.4.0`；TinyUSB `0.21.0~2`，由 `firmware/dependencies.lock` 固定。

SDK、编译器、Git、CMake/Ninja 及 SDK Python 环境需自行安装。首次配置会通过 IDF Component Manager 获取 USB 依赖，需要网络；仓库不重复存放下载缓存。板级、显示、QuickJS 和 TTS 的项目适配源码已保留，不能简单换成上游原版。

## Windows

先按 ESP-IDF 官方方式安装支持 `esp32s31` 的固定 SDK 及工具链。在仓库根目录 PowerShell 执行（替换 SDK 路径）：

```powershell
.\Build.ps1 -IdfPath 'E:\Espressif\esp-idf'
```

可选参数：`-BuildDirectory 'E:\build-wordcard'`、`-CompilerBin 'E:\rv32\bin'`。Windows 建议短路径；CompilerBin 只是指定已经安装的编译器目录，不安装或替换工具链。

入口会在任何 Python/IDF 调用前设置 `PYTHONUTF8=1`、`PYTHONIOENCODING=utf-8`，校验多语言 stdout/stderr 和子进程继承。失败即停止；不能因目录里还有旧 BIN 就视为成功。有效配置来自随源码提交的 `firmware/sdkconfig.defaults`，可变的 sdkconfig 写在构建目录。

成功产物在 `build/`：`metalio_wordcard.bin`、`bootloader/bootloader.bin`、`partition_table/partition-table.bin` 和 `flasher_args.json`。仓库不提交这些文件。编译不需要 SD 模型、音频或字库，实际使用需要，见 [资源文档](RESOURCES.md)。内部 CMake 项目版本仍为 0.3.5，功能基线按 fix55h-recovery 标识。

## 主机检查

Windows 上需要 Visual Studio 2022 C++ 开发工具及 CMake，在 Developer PowerShell 中执行：

```powershell
cmake -S tests/native_learning -B work/host -G "Visual Studio 17 2022" -A x64
cmake --build work/host --config Release
ctest --test-dir work/host -C Release --output-on-failure
powershell -NoProfile -ExecutionPolicy Bypass -File tests/sd_diagnostic_test.ps1 -Run local-check-1
```

CTest 包含无需 SD 资源的刷新预算、键盘输入、电源键、关机、音频增益和缓存检查。SD 诊断测试只操作 `work/` 下新建的模拟目录，不连接设备；每次使用新的 Run 名称。

完整词典、分级词汇和界面回归还需要配套词库和 EFNT 字库。相应测试源码保留，各测试的命令行参数见其 `main()`；公开仓库的基础检查不冒充这些资源测试或实机验收。

## 修改生成代码

平时直接编译已提交的生成文件。修改旧课程逻辑后，用 Node.js 重新生成：

```powershell
node tools/native_learning_data.cjs
node tools/web_firmware_bundle.cjs
```

原始输入在 `src/legacy-course/`；它是“更多学习”的兼容模块，不是最新分级背词页面的浏览器副本。主线页面在原生 C++ 中。

关机图源文件在 `assets/shutdown/`。如需重新编码，先安装 Pillow，再用 UTF-8 环境运行 `tools/shutdown_asset.py`。该步骤不属于普通编译的前置条件。其他内置兼容词表和波形表已作为 C/C++ 输入提交；大容量词库制作与模型转换材料随外部资源包另行提供。

构建工具参考：[ESP-IDF 官方仓库](https://github.com/espressif/esp-idf)。更换 SDK、USB 组件或屏幕波形后须重新验证硬件；本次公开整理的检查范围见 [VALIDATION.md](VALIDATION.md)。
