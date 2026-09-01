# Shell UI 实现调研

> 调研日期：2026-09-01 ｜ 工程：UE_Shell_Project（UE 5.8）
> 结论：**Shell UI = 纯 C++ 手写 Slate 终端模拟器**，无第三方 TUI 库、无 UMG 蓝图、无 Canvas/DOM/Render-to-Texture。

---

## 1. 实现位置

Shell UI 不在游戏模块 `Source/UE_Shell_Project/` 中，而在独立插件里：

| 角色 | 路径 |
| --- | --- |
| 插件清单 | `Plugins/Shell_UE/Shell_UE.uplugin`（`Type: Runtime`，`LoadingPhase: Default`） |
| 模块入口 | `Plugins/Shell_UE/Source/Shell_UE/Shell_UE.cpp`（`IMPLEMENT_MODULE`，非主游戏模块） |
| 构建规则 | `Plugins/Shell_UE/Source/Shell_UE/Shell_UE.Build.cs` |
| **渲染核心** | `Plugins/Shell_UE/Source/Shell_UE/Private/Shell/Terminal/ShellTerminalPanel.cpp`（1108 行） |
| Slate 面板头文件 | `Plugins/Shell_UE/Source/Shell_UE/Public/Shell/Terminal/ShellTerminalPanel.h` |
| UMG 壳 | `Private/Shell/Terminal/ShellTerminalWidget.cpp` / `Public/.../ShellTerminalWidget.h` |
| 业务子系统 | `Private/Shell/Terminal/ShellSubsystem.cpp`（896 行） |
| 纯逻辑层 | `Private/Shell/Terminal/ShellTerminalLogic.cpp` / `Public/.../ShellTerminalLogic.h` |
| 样式与材质 | `Private/Shell/Terminal/ShellTerminalStyle.cpp` |
| CJK 字体 | `Private/Shell/Terminal/ShellFontUtil.cpp` |
| 会话 HUD | `Private/Shell/Terminal/ShellSessionHUD.cpp` |
| 宿主接入 | `Source/UE_Shell_Project/ShellProjectPlayerController.cpp` |

全工程**不存在任何 `WBP_*` / Widget Blueprint 资产**（已全盘检索确认），控件树 100% 由 C++ 在运行时构建。

### 依赖声明（`Shell_UE.Build.cs`）

```csharp
PublicDependencyModuleNames:  Core, CoreUObject, Engine, InputCore,
                              EnhancedInput, UMG, GameplayAbilities,
                              GameplayTags, GameplayTasks
PrivateDependencyModuleNames: Slate, SlateCore, DeveloperSettings
```

`Slate / SlateCore` 是 Private 依赖——Slate 实现细节不外泄，对外只暴露 UObject 层接口。

---

## 2. 技术方案

### 2.1 选型定论

`RawContent/doc/08-TUI绘制方案.md` 记录了完整选型调研，结论是**手写原生控件**：

- 市面无生产级可嵌入游戏的 TUI 库。唯一真 TUI 框架 UltraTerm 停更未上架；Hiraeth Terminal System、YetiTech OS Simulator 等商业品是"模拟命令终端"，无 ANSI/VT 解析。
- FTXUI / notcurses 渲染目标是真终端 stdout，UE 运行时无终端状态机，不能直接画进游戏 UI。
- libvterm / ANSI 解析器 / Render-to-Texture **均未引入**（当前需求不需要）。
- 参考了引擎自带 `UConsole`（FCanvas 行模型）与编辑器 `SOutputLog`（滚动回看 + 用户滚动暂停）的范式。

### 2.2 关键否决：为什么不用 `UListView`

设计文档 07 原本推荐 `UListView` 做虚拟化，落地时**被否决**：

> `SListView` 会销毁离屏行（含输入行，焦点/IME 状态丢失）。实际采用 `SScrollBox` + 行池化 `STextBlock`。

因此输入行不是独立的固定行，而是**作为 `SScrollBox` 的最后一个 slot 嵌入**，随内容一起滚动（见 `ShellTerminalPanel.cpp:283-290`）。这决定了滚动容器不能虚拟化。

---

## 3. 结构与工作原理

### 3.1 三层职责切分

| 层 | 类型 | 职责 |
| --- | --- | --- |
| `UShellTerminalWidget` | `UUserWidget` | 瘦壳。只负责 `RebuildWidget()` 建面板、订阅/退订 `OnShellOutput` 与 `OnQuickCommandsChanged`、转发 API |
| `SShellTerminalPanel` | `SCompoundWidget` | 全部渲染与输入：控件树、行池化、光标、补全、历史导航、快捷栏 |
| `Shell::Terminal::*` | 自由函数 | 纯函数：配色、提示符拼接、回显行、补全、历史导航、自动滚动判定。无 UObject / Slate 依赖，可单测 |

### 3.2 Slate 控件树（面板实际结构）

```
SOverlay                                   根容器，五层叠放
├─ SColorBlock       背景遮罩 (0.02,0.02,0.02,0.88)，OnMouseButtonDown 吞掉穿透点击
├─ SImage            CRT 背景材质 M_CRT_Background
├─ SVerticalBox      主布局
│  ├─ STextBlock     标题 "SHELL v0.1 — 伪终端"
│  ├─ SScrollBox     输出滚动区（FillHeight=1）
│  │  ├─ STextBlock ×N   输出行（行池化，超上限复用旧控件）
│  │  └─ SBox        输入行（随内容一起滚动）
│  │     ├─ SHorizontalBox  快捷指令 chip 栏（数字键 1-9，每页 9 条，« » 翻页）
│  │     ├─ SHorizontalBox  提示符 STextBlock + SOverlay(SEditableText + 方块光标)
│  │     └─ STextBlock      补全建议行
│  └─ STextBlock     状态栏 "history:N | Tab 补全 | Esc 关闭"
├─ SImage            扫描线（4×4 程序化贴图，平铺，HitTestInvisible）
└─ SImage            CRT 暗角 M_CRT_Vignette（HitTestInvisible）
```

### 3.3 行池化（`AppendLine`，`ShellTerminalPanel.cpp:309`）

```
Lines.Add(行)
while Lines.Num() > Settings->HistoryCapacity:
    Lines.RemoveAt(0)
    回收弹出的 STextBlock 作为 ReusedWidget      ← 控件复用，不销毁重建
若无可复用控件：SNew(STextBlock) 并 ScrollBox->InsertSlot(末尾)
否则：SetText + SetColorAndOpacity 后重新 InsertSlot
若 bAutoScroll：ScrollBox->ScrollToEnd()
```

行颜色由 `Shell::Terminal::GetColorForType(EShellOutputType)` 决定（`Info/Success/Error/Input/System`）。

### 3.4 自制方块光标

`SEditableText` 自带的是竖线 caret，不符合终端观感，因此**自制块状光标**：

- 结构上用 `SBox CaretOffsetHost`（`HAlign_Right`）包 `SBox CaretBlock` → `SColorBlock`（`0.498,1.0,0,1`）。
- `UpdateCaret()` 用 `FSlateFontMeasure` 测量完整输入文本宽度，把宿主宽度设为该值，光标块右对齐即精确落在文本末尾。
- 块宽 = 一个拉丁字符 `W` 宽；块高 = 一个全角字符 `中` 高（整格光标）。
- 密码态把测量文本替换为等长 `•`（`ShellTerminalPanel.cpp:1001-1005`）。
- 闪烁用 `RegisterActiveTimer(0.5f, HandleCaretBlink)`，失焦时 `StopCaretBlink()`。

### 3.5 CRT 视觉

| 元素 | 实现 |
| --- | --- |
| 背景 / 暗角 | `LoadObject` 加载 `M_CRT_Background` / `M_CRT_Vignette`（DreamShader Graph 生成的**标准 UMaterial**），填进 `FSlateBrush` → `SImage` |
| 扫描线 | `UTexture2D::CreateTransient(4,4)` 逐像素写：第 0 行 `(0,255,128,255)`，其余透明；`Filter=Nearest`、`SRGB=false`、平铺（`ESlateBrushTileType::Both`） |
| 闪烁 | `RegisterActiveTimer(0.05f, HandleScanlineFlicker)`，8.5 秒周期正弦调制 alpha |
| 字体 | 等宽 CJK 字体 `F_CJK`（MapleMono NF CN），经 `FCompositeFont` 包装 |

> **字体坑（UE 5.8）**：`FSlateFontInfo(UFontFace*, size)` 是陷阱——`GetCompositeFont()` 会把字体 Cast 为 `IFontProviderInterface`，而 `UFontFace` 未实现该接口（只有 `UFont` 实现），Cast 返回 null 后 Slate 静默回退 LastResort，所有字形变方块。正解是把 `FFontData(CJKFace)` 装进 `FCompositeFont.DefaultTypeface`（`FFontData` 走 `IFontFaceInterface`，`UFontFace` 确实实现）。见 `ShellFontUtil.cpp`。
>
> 字体以 `UPROPERTY(Transient) TObjectPtr<UFontFace> ShellCJKFont` 持有作 GC 根——`FCompositeFont` 的 UObject 指针对 GC 不可见。

### 3.6 输入处理（`OnPreviewKeyDown`，`ShellTerminalPanel.cpp:650`）

| 按键 | 行为 |
| --- | --- |
| `Ctrl+C` | 回显 `^C`，调 `Shell->RequestScriptCancel()` 中断运行中的脚本 |
| `Esc` | `Shell->CloseTerminal()` |
| `Tab` | 输入框为空 → 关闭终端；否则 `BuildAutocomplete()` 填最长公共前缀，多候选时在 `SuggestionText` 列出 |
| `↑` / `↓` | `NavigateHistory(±1, ...)`，非提示态才生效；`INDEX_NONE` 表示位于草稿，向下越界恢复草稿 |
| `1-9` / `PageUp` / `PageDown` | 快捷指令。**仅当输入框为空且无修饰键时接管**，避免抢占打字（如 `echo 1`） |

回车提交走 `SEditableText::OnTextCommitted` → `HandleTextCommitted`：

```
若 IsPromptActive():
    先回显（必须在 SubmitPromptLine 之前——回调可能同步启动下一个提示，
            改写 PromptText 标签与掩码状态）→ Shell->SubmitPromptLine(Line)
否则:
    回显 → Shell->ExecuteCommand(Line)
清空输入框 → 重置历史导航 → bAutoScroll=true → ScrollToEnd → FocusInput
```

`SubmitLine()`（程序化驱动）刻意复用同一条路径：写入 `SEditableText` 后直接调 `HandleTextCommitted(..., OnEnter)`，保证宿主/快捷指令/脚本触发与真实键入行为完全一致。

---

## 4. 初始化与入口流程

### 4.1 模块装载

```
引擎启动 → Shell_UE.uplugin (Runtime / Default)
         → IMPLEMENT_MODULE(FDefaultModuleImpl, Shell_UE)   [Shell_UE.cpp]
```

### 4.2 子系统初始化（`ShellSubsystem.cpp:59`）

```
UShellSubsystem::Initialize()
  Registry.Clear()
  RegisterBuiltInCommands / GameFlowCommands / SpawnCubeCommands
  RegisterGASCommands / QuickCommandCommands / ScriptCommands / StoreCommands
  若 Settings->ShellCommandsTable 有效 → Registry.RegisterCommandsFromDataTable()  // 可选，缺表非致命
```

### 4.3 宿主接线（`Source/UE_Shell_Project/ShellProjectPlayerController.cpp`）

```
BeginPlay:            AddMappingContext(IMC_Shell, 0)
SetupInputComponent:  BindAction(IA_TerminalToggle, Started, HandleTerminalToggle)
HandleTerminalToggle: GameInstance->GetSubsystem<UShellSubsystem>()->ToggleTerminal(this)
```

资产路径硬编码加载：`/Shell_UE/Shell/Input/IA_TerminalToggle`、`/Shell_UE/Shell/Input/IMC_Shell`。

### 4.4 打开终端（`ShellSubsystem.cpp:202`）

```
ToggleTerminal(PC)
  if !TerminalWidget.IsValid():
      TerminalWidget = CreateWidget<UShellTerminalWidget>(PC)
      bTerminalOpen = false      // 控件随关卡旅行销毁；重建后必然是关闭态，
                                 // 不重置会导致跨关卡首次 Toggle 反向关闭
  if bTerminalOpen: CloseTerminal() + ApplyGameInputMode()
  else:            bTerminalOpen = true
                   Widget->Open()            → AddToViewport(50)
                   ApplyUIInputMode(PC, Widget)
```

`ApplyUIInputMode` = `FInputModeUIOnly` + `SetWidgetToFocus(Widget->GetFocusTarget())` + `SetShowMouseCursor(true)`。
（设计文档 07 规划的"打开·未打字 = GameAndUI 可 WASD 移动"三态 FSM **未实现**，当前终端打开即 `UIOnly`。）

### 4.5 面板构建时机（UE 5.8 关键坑）

面板在 `UShellTerminalWidget::RebuildWidget()` 中构建，**不是** `NativeConstruct`：

> UE 5.8 引擎在 `NativeConstruct` 触发之前就于 `RebuildWidget()` 中快照 Slate 内容，因此面板必须在这里构建。

`RebuildWidget()` 幂等（只构建一次，后续返回缓存 `Panel`），这是 `GetFocusTarget()` 敢在控件入 viewport 前强制调用它的前提。`UShellSessionHUD` 同理在 `RebuildWidget()` 里程序化构建 `UBorder → UVerticalBox → UTextBlock` 树。

### 4.6 输出回传链路

```
任意命令 Shell->Print(Text, Type)
  → OnShellOutput.Broadcast(FShellOutputLine)
  → UShellTerminalWidget::HandleShellOutput   （NativeConstruct 中 AddUniqueDynamic 订阅）
  → SShellTerminalPanel::AppendLine
  → 行池化 STextBlock + ScrollToEnd
```

---

## 5. 已知设计取舍

| 项 | 现状 |
| --- | --- |
| 虚拟化 | 放弃 `UListView`，改行池化（保护输入行焦点/IME） |
| 输入模式 | 终端打开即 `UIOnly`，无 `GameAndUI` 中间态 |
| 未实现（文档 07 前瞻设计） | 底部快捷栏 / 侧边快速面板（E4）、战斗热键（E5）、TPS-FPS 双实例定位（E7） |
| 已实现 | 快捷指令 chip 栏（1-9 + 分页）、GAS 命令层（E6）、脚本引擎、虚拟文件系统、登录会话、关卡旅行 |

---

## 6. 相关文档（插件内）

- `Plugins/Shell_UE/RawContent/doc/07-UI面板与输入策略.md` — 输入策略与前瞻设计（含"未实现"状态标注）
- `Plugins/Shell_UE/RawContent/doc/08-TUI绘制方案.md` — 现成 TUI 库调研与选型结论
- `Plugins/Shell_UE/RawContent/doc/02-终端架构设计.md` — 终端总体架构
- `Plugins/Shell_UE/RawContent/doc/04-字体方案.md` — CJK 字体方案
- `Plugins/Shell_UE/RawContent/开发踩坑记录/02-UE5.8-Slate与渲染.md` — Slate 相关坑位合集
- `docs/宿主接入指南.md` — 宿主接入指南
