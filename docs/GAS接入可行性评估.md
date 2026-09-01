# GAS 接入可行性评估（以官方 Lyra 为参照）

> 评估日期：2026-09-01 ｜ 参照工程：`C:\LWS\UE\ExampleLyra`（官方 Lyra 5.8）
> 评估对象：`D:\SSDWP\UE\UE_Shell_Project`（UE 5.8 + Shell_UE 插件）
>
> **结论：可以接入 GAS —— 且本项目已实际完成 GAS 基础接入（P3 里程碑），
> 当前状态不是"能不能接"的问题，而是"与 Lyra 范式还差哪几块"的问题。**

---

## 1. 官方参照工程 ExampleLyra 的状态

| 项 | 状态 | 证据 |
| --- | --- | --- |
| 工程完整性 | ✅ 完整官方 Lyra 5.8 | `LyraStarterGame.uproject`（EngineAssociation 5.8） |
| 源码 | ✅ 496 个源文件 | `Source/LyraGame/` + `Source/LyraEditor/` |
| 编译产物 | ✅ 已编译 | `Binaries/Win64/UnrealEditor-LyraGame.dll`、`UnrealEditor-LyraEditor.dll` |
| Content | ✅ 1.8GB 完整 | `Content/`（B_LyraGameInstance、B_LyraGameMode、Characters、Effects…）——README 说的"需从 Marketplace 补内容"对此工程不成立 |
| GAS 插件 | ✅ 已启用 | uproject Plugins 中 `GameplayAbilities` Enabled |

**结论：ExampleLyra 是一个可直接打开运行、可对照源码学习的完整官方范本。** 它自身就内置了全量 GAS 架构，不存在"接入"问题，它的价值在于给本项目提供**标准范式参照**。

### Lyra 的 GAS 架构落点（本报告参照物）

| 架构件 | Lyra 文件 | 说明 |
| --- | --- | --- |
| ASC 子类 | `AbilitySystem/LyraAbilitySystemComponent.h` | 基类扩展：TagRelationshipMapping、能力激活组等 |
| 能力数据资产 | `AbilitySystem/LyraAbilitySet.h` | **能力 + InputTag + GE + AttributeSet 批量授予**，`GiveToAbilitySystem()` 一处授予 |
| 属性集 | `AbilitySystem/Attributes/LyraAttributeSet.h`、`LyraCombatSet.h`、`LyraHealthSet.h` | 基类 + 战斗属性 + 生命属性分离 |
| 伤害/治疗执行 | `AbilitySystem/Executions/LyraDamageExecution.cpp`、`LyraHealExecution.cpp` | `UGameplayEffectExecutionCalculation` 子类 |
| 玩家侧 ASC 挂载 | `Player/LyraPlayerState.cpp:34` | **ASC 放 PlayerState**（死亡/重生不丢状态） |
| 角色侧 ASC | `Character/LyraCharacterWithAbilities.cpp:15` | 需要时角色也有 ASC |
| 世界侧 ASC | `GameModes/LyraGameState.cpp:28` | 游戏阶段等世界级能力 |
| 能力授予调用 | `Player/LyraPlayerState.cpp:207` | `AbilitySet->GiveToAbilitySystem(ASC, ...)` |
| 输入触发 | `GameFeatures/GameFeatureAction_AddInputBinding.cpp` | AbilitySet.InputTag → EnhancedInput 按键映射 |
| 死亡/重生 | `Character/LyraHealthComponent.h` | 生命归零处理、玩家状态机 |
| 模块依赖 | `Source/LyraGame/LyraGame.Build.cs` | Public: GameplayAbilities/GameplayTags/GameplayTasks/ModularGameplay/GameFeatures；Private: EnhancedInput |

---

## 2. 本项目当前 GAS 状态盘点（实测代码证据）

### 2.1 已具备（✅ 基础接入已完成）

| 架构件 | 本项目文件 | 证据 |
| --- | --- | --- |
| GAS 插件启用 | `UE_Shell_Project.uproject` | `"GameplayAbilities": Enabled` |
| 模块依赖 | `Plugins/Shell_UE/Source/Shell_UE/Shell_UE.Build.cs` | Public 含 `GameplayAbilities`、`GameplayTags`、`GameplayTasks` |
| **ASC 挂载（PlayerState，与 Lyra 同范式）** | `Source/UE_Shell_Project/ShellProjectPlayerState.cpp:12` | `CreateDefaultSubobject<UAbilitySystemComponent>` + `SetIsReplicated(true)` + `Minimal` 复制模式 |
| 属性集 | `Source/UE_Shell_Project/ShellProjectAttributeSet.h` | Health / MaxHealth / Mana / MaxMana（`ATTRIBUTE_ACCESSORS_BASIC`） |
| 能力基类 + 火球 | `Source/UE_Shell_Project/ShellProjectAbility.h` | `UShellProjectAbility` → `UShellFireballAbility` |
| 消耗/冷却 GE | 同文件 `:26 / :36` | `UShellFireballCostGE`、`UShellFireballCooldownGE`（3s，授予 `Cooldown.Fireball`） |
| 角色接口 | `Source/UE_Shell_Project/ShellProjectCharacter.h:19` | `ACharacter, public IAbilitySystemInterface`，`GetAbilitySystemComponent()` 委托 PlayerState |
| 角色侧初始化 | `Source/UE_Shell_Project/ShellProjectCharacter.cpp:60` | `ASC->InitAbilityActorInfo(ShellPlayerState, this)` |
| 能力授予 | `ShellProjectPlayerState.cpp:40-48` | `BeginPlay` 权限侧 `GiveAbility(FGameplayAbilitySpec(...))` |
| GameplayTag | `Config/DefaultGameplayTags.ini` | `Ability.Skill.Fireball` / `Cooldown.Fireball` / `Cost.Mana.Fireball` |
| **终端 GAS 命令层** | `Plugins/Shell_UE/Source/Shell_UE/Private/Shell/Terminal/ShellGASCommands.cpp` | `cast`（`TryActivateAbilitiesByTag` + `FGameplayEffectQuery` 冷却查询 + 蓝耗显示）、`attributes`（反射遍历 `FGameplayAttributeData`）、`cooldowns` |
| 数据驱动 | `Plugins/Shell_UE/Content/Shell/Data/DT_ShellAbilities.uasset` | `FShellAbilityRow`（DisplayName / AbilityTag / CooldownTag / ManaCost） |
| 验收证据 | `Plugins/Shell_UE/RawContent/qa/p3-0*.png` | cast fireball、cooldowns、attributes、全链路截图 |

### 2.2 缺失 / 与 Lyra 的差距（⚠️）

| 差距 | 现状 | Lyra 参照 |
| --- | --- | --- |
| **能力授予是硬编码数组** | `StartupAbilities.Add(UShellFireballAbility::StaticClass())` 写死在 PlayerState 构造函数 | `ULyraAbilitySet` DataAsset 批量授予（`GiveToAbilitySystem`），配表即得 |
| **无伤害/治疗执行** | 无任何 `GameplayEffectExecutionCalculation`（全项目检索为空） | `Executions/LyraDamageExecution.cpp`、`LyraHealExecution.cpp` |
| **Health 属性无消费方** | `Health` 只声明未使用，无死亡/重生逻辑 | `Character/LyraHealthComponent.h`（生命归零 → 死亡状态机 → 重生） |
| **无 GameplayCue** | 技能无特效/音效反馈通道（全项目检索为空） | `AbilitySystem/LyraGameplayCueManager.h` |
| **无能力按键绑定** | 技能只能通过终端 `cast` 命令触发，无 AbilitySet.InputTag → EnhancedInput 映射 | `GameFeatureAction_AddInputBinding` + `AbilitySet.InputTag` |
| **ASC 是引擎基类** | 直接用 `UAbilitySystemComponent`，无 TagRelationshipMapping/激活组 | `ULyraAbilitySystemComponent` 子类 |
| **无 GamePhase 子系统** | 无世界级状态机 | `AbilitySystem/Phases/LyraGamePhaseSubsystem.h` |

---

## 3. 结论

1. **能接入 GAS —— 而且已经接入**。ASC-on-PlayerState、AttributeSet、Ability、Cost/Cooldown GE、GameplayTag、终端驱动层均已就位并通过 P3 验收（cast fireball / cooldowns / attributes 实测可用）。
2. **ExampleLyra 作为参照完全合格**：完整源码 + 编译产物 + Content，所有 GAS 范式和文件都可直接对照。
3. 本项目与 Lyra 的架构骨架**同构**（ASC 都挂在 PlayerState，都走 `IAbilitySystemInterface`），因此后续深化可以"按 Lyra 文件逐块移植"而非重构。

---

## 4. 深化路线（参考 Lyra，按收益排序）

### 阶段 A · 数据驱动授予（对齐 LyraAbilitySet）
- 参考 `LyraAbilitySet.h`，新建 `UShellAbilitySet : UPrimaryDataAsset`（能力 + InputTag + GE + AttributeSet 数组），
  `GiveToAbilitySystem()` 一处授予；PlayerState 改为从资产加载，删掉 ctor 硬编码数组。
- 现成的 `DT_ShellAbilities` 数据表可**双驱动**：数据表只描述"终端能看到什么"，AbilitySet 决定"授予什么"。

### 阶段 B · 伤害管线（对齐 LyraDamageExecution / HealExecution）
- 参考 `LyraDamageExecution.cpp`：新建 `UShellDamageExecution : UGameplayEffectExecutionCalculation`，
  把 `HealthSet` 的 Health 接进伤害 GE；同时把现有 `Health` 属性接上客户端回调（`OnRep_Health` / `FOnGameplayAttributeValueChange`）。

### 阶段 C · 死亡与重生（对齐 LyraHealthComponent）
- 参考 `LyraHealthComponent.h`：Health 归零 → 死亡状态 → `OnPawnDied` 广播 → 重生。
- 注意本项目已有踩坑先例：`BeginPlay` 覆盖 Avatar 时序（踩坑 09）、PlayerState 忘设（踩坑 10）——移植时对照 `ShellProjectPlayerState.cpp:36-37` 的 pawn 判断写法。

### 阶段 D · 反馈与输入（对齐 GameplayCue + InputTag）
- 参考 `LyraGameplayCueManager`：为火球加 `GameplayCue.Skill.Fireball`，终端/屏幕输出反馈。
- 参考 `GameFeatureAction_AddInputBinding`：给 `FShellAbilityRow` 补 InputTag，把火球绑到技能键（如 1-9 快捷键已经存在，可让快捷指令与 GAS 共用同一 `cast` 路径——这正是设计文档 07 的 E5 战斗热键）。

### 阶段 E · 工程化加固（可选）
- ASC 子类化（对齐 `ULyraAbilitySystemComponent`）：TagRelationshipMapping、`AbilityActivationGroup` 激活互斥。
- GamePhase 子系统（对齐 `Phases/`）：菜单 → 游戏 → 结算 的世界级状态机，与终端 `gamemode` 命令联动。

---

## 5. 移植时的既有坑位清单（本项目已踩，勿重蹈）

| 坑 | 出处 | 规避 |
| --- | --- | --- |
| BeginPlay 覆盖 Avatar 时序 | 踩坑记录 09 | `ShellProjectPlayerState.cpp:36` 已按 `GetPawn()` 判断后 Init |
| AddComponent 在 CDO 构造 Fatal | 踩坑记录 13 | ASC/AttributeSet 用 `CreateDefaultSubobject`，勿在 CDO 构造里动态 AddComponent |
| GameplayTag 字典就绪时序 | 踩坑记录 12 | 初始化期勿直接读 Tag 字符串，走 FGameplayTag 反射/先建后查 |
| GameplayEffect 需注册 GEComponents | 踩坑记录 14 | 新 GE 类记得挂 `UGameplayEffectComponent` |
| 属性反射 FStructProperty 遍历 | 踩坑记录 08 | `attributes` 命令已有现成实现可复用（`ShellGASCommands.cpp:101-121`） |
| GameMode 忘设 PlayerStateClass | 踩坑记录 10 | `ShellGameplayGameMode.cpp:23` 已设，新增模式记得带 |
| UE 5.8 GAS API 迁移 | 踩坑记录 11 | `TryActivateAbilitiesByTag` 等 5.8 签名变化参考 `ShellGASCommands.cpp:259` |

---

## 6. 相关文档

- 本项目 GAS 设计：`Plugins/Shell_UE/RawContent/doc/05-GAS联动方案.md`
- 实施复盘：`Plugins/Shell_UE/RawContent/doc/13-M1-M4实施复盘.md`
- 踩坑合集：`Plugins/Shell_UE/RawContent/doc/开发踩坑记录/`（05 / 08 / 09 / 10 / 11 / 12 / 13 / 14 / 17）
- Lyra 官方文档：https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/
