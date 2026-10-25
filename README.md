# NyakoMCL

使用 C++17、Qt Quick 编写的 Minecraft Java 离线启动器。界面通过主线程控制器管理实例登记，文件检查、安装、Java 探测和启动准备在后台执行。

## 功能与使用

- **下载**：获取官方正式版清单，创建独立实例，下载版本文件、库、资源和原生库；支持文件检查与安装 / 修复、进度、日志及取消。
- **实例**：以稳定 ID 登记实例，各实例拥有自己的游戏目录、显示名、离线玩家名及 Java 偏好。
- **启动**：检查文件、修复缺失内容，选择兼容 Java；需要时下载托管运行时，启动游戏并显示进程日志。
- **全局设置**：扫描并查看本机 Java 环境。扫描只更新环境列表，不保存任何实例偏好。
- **实例设置**：独立模态窗口编辑显示名、玩家名和 Java；空 Java 路径表示自动选择。保存前验证指定 Java，一次原子提交；取消、Esc 或关闭窗口丢弃草稿。

先在“下载”选择正式版本，填写显示名和完整目标目录，然后“创建并安装”。目标须为空目录或尚不存在的绝对目录，不能与已有实例目录重叠。安装完成后，在启动页或实例列表打开“实例设置”，配置玩家名，再到启动页选择实例并启动。

GUI 允许先保存空玩家名，但启动时会明确拒绝空名。名字不能含英文引号或控制字符；1.20.3 起最多 16 个字符。非标准名字会提示服务器兼容风险。指定的 Java 只要能够正常探测便可保存，即使它不兼容；启动时会自动选择或下载版本元数据所需主版本的 Java。

四个导航页常驻，切换不会丢失下载表单。安装、启动准备、游戏运行、检查和 Java 扫描期间不能打开或提交实例设置。

安装 / 修复及启动准备自动进入独立任务页，保存任务开始时的实例身份、真实执行阶段、阶段文件计数、详情及最终错误。任务页属于内部导航，不增加第五个主导航，也不选中下载页。底栏“查看任务”可从任意页面进入；“返回”回到本次进入的来源页，不取消任务，下载表单草稿继续保留。成功、失败或取消只显示一次非阻塞提示，不强制跳页；结果保留至关闭或下次任务，在任务页关闭结果会返回来源页。取消先显示“正在取消”，后台线程退出后才确认已取消。

网络接收量按本阶段实际读取的响应体累计，包含失败重试收到的数据；缓存命中仅计入已处理文件。速度由主线程每 250ms 采样计算，切换阶段重置计数；未确定数量时显示不定进度，不显示总体百分比或总字节分母。失败可复制完整错误，失败或取消后可按任务冻结的实例 ID 重新安装 / 修复或重新启动；重试重新校验并复用完整文件，不创建重复实例，也不提供暂停续传或任务队列。

## 数据位置与隔离

每个实例的版本、库、资源、原生库、游戏配置、存档及日志位于创建时指定的游戏目录。显示名不决定路径，重命名不会迁移游戏数据。实例设置不允许改版本或迁移目录。

应用登记 `instances.json`、版本清单缓存 `catalog/` 和托管 Java `runtime/` 使用 Qt `QStandardPaths::AppDataLocation`。Windows 通常位于 `%APPDATA%/NyakoMCL/`，具体位置由 Qt 和运行环境决定。托管 Java 是应用级环境缓存，实例保存各自的选择路径。不要将整个部署目录当作便携数据目录。

## 构建

需要：

- C++17 编译器、CMake >= 3.21，以下示例使用 Ninja。
- Qt >= 6.8，包含 Quick、QuickControls2、Network、Concurrent 及 Qt 的 QML 构建工具。
- zlib 开发头文件与库。

Qt kit 必须与编译器和架构匹配。在 Qt kit 的命令行环境中执行，或设置 `CMAKE_PREFIX_PATH` 为本机对应 kit 的安装目录：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="<Qt kit 路径>"
cmake --build build --parallel
```

Release 使用独立构建目录：

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt kit 路径>"
cmake --build build-release --parallel
```

Windows MinGW 构建优先定位该编译器发行包中的 zlib；其他环境可通过 CMake 的 `ZLIB_INCLUDE_DIR`、`ZLIB_LIBRARY` 指定匹配的库。也可用 Qt Creator 打开 `CMakeLists.txt`，选取匹配的桌面 kit。

Windows PowerShell 部署示例（从已配置 Qt kit 的环境运行）：

```powershell
New-Item -ItemType Directory -Force dist
Copy-Item build-release/NyakoMCL.exe dist/NyakoMCL.exe
windeployqt --release --qmldir qml dist/NyakoMCL.exe
./dist/NyakoMCL.exe
```

交付的是包含 Qt DLL、插件及 QML 依赖的目录，不能只复制 EXE。当前没有公开的自动测试目标；本地验证用脚本和游戏数据不属于源码交付。

## 命令行

有额外参数时进入 CLI，不创建界面。当前提供四个命令：

```sh
NyakoMCL --list-java
NyakoMCL --install "<游戏目录>" "<版本ID>"
NyakoMCL --check "<游戏目录>" "<版本ID>"
NyakoMCL --launch "<游戏目录>" "<版本ID>" "<java可执行文件>" "<离线名字>"
```

CLI 按显式目录工作，不创建 GUI 实例登记。`--install` 安装 / 修复文件；`--check` 检查文件并报告缺失和损坏数量。`--list-java` 列出能探测的环境。

`--launch` 要求文件已经安装完整，显式指定 Java 必须匹配版本要求；不会执行 GUI 的自动修复和托管 Java 准备。CLI 名字规则为 **3–16 位英文字母、数字或下划线**，与 GUI 规则不同。运行期间转发游戏输出，最终返回游戏退出码；其他命令用零表示成功、非零表示错误或未就绪。

## 范围与限制

GUI 下载清单筛选 1.13 起使用现代启动参数的独立原版正式版，不表示这些版本已逐一实测。当前实际游戏初始化验证集中在 Windows x64、Qt 6.12 / MinGW 与原版 1.20.4；其他版本、架构和操作系统仍需实机验证。

目前没有微软正版账户登录、Mod 加载器、整合包或继承版本支持。离线身份不提供正版在线认证。网络不可用时只能使用已有缓存和已安装内容；首次安装或缺失文件修复仍需要可用下载源。

## 源码结构

```text
qml/             四个主页面、独立任务页、实例设置窗口和最小公共控件
src/app/         GUI 控制器、程序入口与 CLI
src/instance/    实例登记、校验及原子持久化
src/install/     文件下载、安装和启动准备
src/java/        Java 探测与托管运行时
src/version/     官方版本元数据、启动计划及文件检查
CMakeLists.txt   Qt QML 模块与 C++ 构建配置
```
