# 爆破模式 — 拆弹玩法架构设计

> **目标**: 在现有 `BombDefusalGameMode` 上叠加最简拆弹玩法  
> **原则**: 先跑通逻辑，美术资源极简；高内聚低耦合、事件驱动、服务器权威

---

## 一、最小跑通模型 — 流程图

```
RoundPrepare ──→ GameMode 随机分配炸弹给一名攻方
       │              ↓ 炸弹进入 Carried 状态
       │         RoundInProgress
       │              │
       │    ┌─────────┴──────────┐
       │    │ 攻方带包者靠近 ABombSite   │  正常交火（歼灭条件仍生效）
       │    │ 按下 Q → 5秒进度条       │
       │    │ 期间锁定所有其他操作       │
       │    │ 完成 → 炸弹 Planted      │
       │    │ 全服文字提示"炸弹已安放"   │
       │    │ 倒计时开始              │
       │    └─────────┬──────────┘
       │              │
       │    ┌─────────┴──────────┐
       │    │ 守方靠近已安放的炸弹      │  倒计时归零
       │    │ 按下 Q → 5秒进度条       │  → Exploded
       │    │ 期间锁定所有其他操作       │  → 攻方胜利
       │    │ 完成 → 炸弹 Defused     │
       │    │ 全服文字提示"炸弹已拆除"   │
       │    │ → 守方胜利              │
       │    └────────────────────┘
       │
   RoundEnd → 分发经济 → 下一局
```

**两个胜利条件并行**：歼灭（现有） + 炸弹爆炸/拆除（新增）

---

## 二、新增类与职责

### 2.1 ABombSite — 埋包点标记

**继承**: `AActor`  
**职责**: 纯空间标记，定义哪里能下包。零逻辑。

```
   场景中拖放 → 调碰撞体大小 → 设 SiteName = "A"
```

| 属性 | 类型 | 说明 |
|------|------|------|
| `SiteName` | `FString` | "A"，UE 细节面板可改 |
| `TriggerVolume` | `UBoxComponent*` | 判定"角色是否在下包区域内" |
| `bIsBombPlantedHere` | `bool` | 炸弹是否在此处（BombActor 种植时设置） |

**最简实现**：新建 `ABombSite` C++ 类 → 蓝图子类 → 场景中放一个 BoxCollision 作为 Trigger

---

### 2.2 ABombActor — 炸弹实体

**继承**: `AActor`（需网络复制）  
**职责**: 炸弹状态机 + 倒计时 + 事件广播。不依赖任何 GameMode/Character 类型。

```
状态机:
  Idle ──(GameMode分配)──→ Carried ──(Q键5秒完成)──→ Planted ──(倒计时归零)──→ Exploded
                                                                    │
                                                    (守方Q键5秒完成)──→ Defused
```

**核心属性**:

| 属性 | 类型 | 复制 | 值 |
|------|------|------|----|
| `BombState` | `EBombState` | Replicated | Idle / Carried / Planted / Exploded / Defused |
| `RemainingTime` | `float` | Replicated | 倒计时剩余秒数，仅 Planted 时有效 |
| `PlantedSite` | `ABombSite*` | Replicated | 种在哪个点位 |
| `PlantDuration` | `float` | EditDefaultsOnly | **5 秒** |
| `DefuseDuration` | `float` | EditDefaultsOnly | **5 秒** |
| `BombCountdown` | `float` | EditDefaultsOnly | 爆炸倒计时总长（如 40 秒） |

**核心方法**:

| 方法 | 谁调用 | 说明 |
|------|--------|------|
| `Server_StartPlant(ABombSite*)` | InteractionComp RPC | 服务器开始安包计时 |
| `Server_CompletePlant()` | InteractionComp（计时结束） | 安包完成，进入 Planted，启动倒计时 |
| `Server_StartDefuse()` | InteractionComp RPC | 服务器开始拆包计时 |
| `Server_CompleteDefuse()` | InteractionComp（计时结束） | 拆包完成，进入 Defused |
| `Server_CancelInteraction()` | InteractionComp | 被打断（移动/死亡），回退状态 |
| `TickBombTimer()` | 自身 Tick | Planted 状态下递减 RemainingTime，归零则 Exploded |

**事件委托**（GameMode 订阅）:

```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBombPlanted,   ABombSite* Site);
DECLARE_MULTICAST_DELEGATE(FOnBombExploded);
DECLARE_MULTICAST_DELEGATE(FOnBombDefused);
```

**模型**: 构造时挂一个 `UStaticMeshComponent`，Mesh 设为 UE 内置 Cube，材质给红色

---

### 2.3 UBombInteractionComponent — 安包/拆包交互

**继承**: `UActorComponent`，挂载在 `BlasterCharacter` 上  
**职责**: 检测 Q 键输入 → 验证条件 → RPC → 管理进度 → 锁定操作

**交互条件判断**（客户端 Tick 检测）:

```
能否下包？
  ├─ 角色是攻方 ✓
  ├─ 角色持有炸弹（BombActor 处于 Carried 且 Owner 是自己）✓
  ├─ 角色处于 ABombSite 的 TriggerVolume 内 ✓
  └─ 炸弹状态是 Carried ✓
  → 屏幕显示 "[Q] 安放炸弹"

能否拆包？
  ├─ 角色是守方 ✓
  ├─ 附近存在 ABombActor（状态为 Planted）✓
  ├─ 角色处于炸弹的可交互距离内 ✓
  └─ 炸弹状态是 Planted ✓
  → 屏幕显示 "[Q] 拆除炸弹"
```

**核心属性**:

| 属性 | 类型 | 说明 |
|------|------|------|
| `bIsInteracting` | `bool` | 是否正在进行安包/拆包 |
| `InteractionProgress` | `float` | 0.0 ~ 1.0，供 UI 进度条绑定 |
| `InteractionTarget` | `AActor*` | 当前交互目标 |
| `InteractionDuration` | `float` | 当前交互需要的总时长（5 秒） |
| `InteractionElapsed` | `float` | 已用时间 |
| `MaxInteractDistance` | `float` | 交互最大距离（200cm） |

**RPC**:

| RPC | 方向 | 说明 |
|-----|------|------|
| `Server_StartPlant(ABombSite*)` | Client→Server | 请求下包，服务器校验条件 |
| `Server_StartDefuse(ABombActor*)` | Client→Server | 请求拆包，服务器校验条件 |
| `Server_CancelInteraction()` | Client→Server | 玩家移动/松键 → 取消 |

**操作锁定**（`bIsInteracting == true` 期间）:

```
禁止: 移动、跳跃、蹲下、开火、换弹、切枪、扔投掷物、打开商店
保留: 瞄准视角（可以看但不能动）
中断条件: 玩家死亡 / 受到伤害 / 服务器判定条件失效
```

**定时器驱动**: 服务器用 `FTimerHandle` 管理 5 秒延迟，到期执行 Complete。客户端本地 Tick 累加进度条。

---

### 2.4 BombDefusalGameMode — 改动点

**在现有代码上增量修改，不重写状态机**:

| 改动 | 方法 | 内容 |
|------|------|------|
| 新增 | `AssignBombToRandomAttacker()` | RoundPrepare 阶段：从攻方存活者随机选一人，Spawn BombActor 并设为 Carried |
| 新增 | `OnBombPlanted(ABombSite*)` | 绑定 BombActor 事件 → 全服文字公告"炸弹已在X点安放" → 可选：紧急 BGM |
| 新增 | `OnBombExploded()` | 攻方本局胜利 → 进入 RoundEnd → 分发经济含炸弹爆炸奖金 |
| 新增 | `OnBombDefused()` | 守方本局胜利 → 进入 RoundEnd → 分发经济含拆弹奖金 |
| 新增 | `DropBombFromDeadPlayer()` | 携带者被击杀 → 在其位置 Spawn 一个状态为 Carried 的 BombActor（其他攻方可捡起） |
| 修改 | `CheckRoundEnd()` | 新增 BombExploded / BombDefused 两条判定路径 |
| 修改 | `PlayerEliminated()` | 检查被淘汰者是否携带炸弹 → 调用 DropBombFromDeadPlayer |
| 修改 | `CleanupRound()` | 回合结束时销毁场上所有 BombActor |

---

### 2.5 经济系统 — 改动极小

`EconomyConfig.h` 已有 `BombDetonationBonus` 和 `BombDefusalBonus` 预留字段。`DistributeRoundEconomy()` 根据本轮胜负原因决定是否叠加。

---

## 三、类角色总表

| 类 | 一句话 | 新增/改动 |
|----|--------|-----------|
| **ABombSite** | 地图上的下包区域标记（碰撞体+名字） | **新增** |
| **ABombActor** | 炸弹状态机+倒计时+Cube模型 | **新增** |
| **UBombInteractionComponent** | Q键交互、条件检测、5秒计时、操作锁定 | **新增** |
| **BombDefusalGameMode** | 分配炸弹、监听爆炸/拆除事件、判定胜负 | **改动 8 处** |
| **EconomyConfig** | 已有爆炸/拆弹奖金字段，无需改数据结构 | 不改 |
| **WBP_BombStatus** | 倒计时 + 点位名 + 状态文字 | **新增** |
| **WBP_InteractProgress** | 圆形进度条 + "[Q] 安放/拆除" 提示 | **新增** |

---

## 四、网络同步要点（Listen Server）

| 数据 | 同步方式 | 说明 |
|------|----------|------|
| `BombState` | Replicated (OnRep) | 状态变更时立即同步 |
| `RemainingTime` | Replicated，每 0.5s 更新一次 | 客户端 UI 线性插值 |
| 安包/拆包进度 | 客户端本地 Tick 预测 + 服务器 FTimerHandle | 服务器到期才真正生效 |
| 操作锁定 | `bIsInteracting` 在服务器上维护 | 服务器拒理锁定期间的非法输入 |

---

## 五、资源清单

| 需求 | 方案 |
|------|------|
| 炸弹模型 | UE 内置 `Cube` + 红色材质 |
| 下包区域标记 | `ABombSite` 的 `UBoxComponent`，开发阶段画 Debug 线框即可 |
| 进度条 UI | UMG `ProgressBar` + `TextBlock` |
| 全服文字 | 复用现有 Announcement 消息系统 |
| 爆炸 VFX | 复用 FragGrenade 的 Niagara 爆炸（爆炸时播一下） |
| 音效 | **不做**。文字提示替代 |

**一句话：代码+蓝图能搞定的，不找任何人。** 唯一的外部依赖是 UE 引擎自带的 Cube。

---

## 六、实施顺序

```
Phase 1   ABombSite + ABombActor
          在场景放一个 BoxCollision 点位 + 一个能 Spawn 的红色 Cube 炸弹
          炸弹状态机跑通：Idle → Carried → Planted → Exploded

Phase 2   UBombInteractionComponent
          挂到 Character 上，Q键 → RPC → 5秒 FTimerHandle → 完成
          操作锁定：禁止移动/开火/切枪

Phase 3   GameMode 事件接入
          分配炸弹、监听爆炸/拆除、判定胜负、全服文字公告

Phase 4   UI
          WBP_BombStatus（倒计时）
          WBP_InteractProgress（进度条 + "[Q] 安放"/"[Q] 拆除"）

Phase 5   联机测试 + 修网络同步问题
```

---

## 七、关键设计决策

1. **ABombActor 不依赖任何 GameMode/Character** — 只通过 Delegate 广播事件，谁订阅谁处理
2. **InteractionComponent 共用安包/拆包** — 同一套"Q键→5秒→锁定→完成/打断"流水线，靠 EBombState 和 ETeamID 区分
3. **服务器权威 + 客户端预测进度条** — 进度条本地立即走（体验好），服务器到时间才真正生效（防作弊）
4. **操作锁定在 InteractionComponent 层** — `CombatComponent`、`ThrowableComponent`、移动输入各自检查 `bIsInteracting` 决定是否允许

---

## 八、代码实现清单（已实施）

### 8.1 新增文件（`Source/Blaster/BombMode/`）

| 文件 | 内容 |
|------|------|
| [BombTypes.h](../Source/Blaster/BombMode/BombTypes.h) | `EBombState` 枚举（Idle/Carried/Planted/Exploded/Defused）+ `EBombInteractionType`（None/Planting/Defusing） |
| [BombSite.h](../Source/Blaster/BombMode/BombSite.h) + .cpp | `ABombSite`：`UBoxComponent` 碰撞体 (Pawn Overlap) + `SiteName` 属性 + 编辑器自动设 ActorLabel |
| [BombActor.h](../Source/Blaster/BombMode/BombActor.h) + .cpp | `ABombActor`：`UStaticMeshComponent`(Cube) + `USphereComponent`(交互检测) + 状态机 + `FTimerHandle` 5秒定时器 + Delegate 广播 |
| [BombInteractionComponent.h](../Source/Blaster/BombMode/BombInteractionComponent.h) + .cpp | `UBombInteractionComponent`：Tick 检测附近目标 + Q 键→RPC→5秒→锁定/解锁 Character |
| [BombStatusWidget.h](../Source/Blaster/BombMode/BombStatusWidget.h) | `UBombStatusWidget`：BlueprintImplementableEvent — UpdateTimer / UpdateStatusText / UpdateSiteName / SetBombUIVisible |
| [BombInteractWidget.h](../Source/Blaster/BombMode/BombInteractWidget.h) | `UBombInteractWidget`：BlueprintImplementableEvent — UpdateProgress / UpdatePromptText / SetInteractVisible |

### 8.2 修改文件

| 文件 | 改动行 | 内容 |
|------|--------|------|
| [BlasterCharacter.h](../Source/Blaster/Character/BlasterCharacter.h) | L10 + L125-126 + L224 + L67 | 新增 `UBombInteractionComponent*` 成员 + getter + `BombInteractPressed()` 声明 |
| [BlasterCharacter.cpp](../Source/Blaster/Character/BlasterCharacter.cpp) | L14 + L54 + L109 + L818-824 | 构造 `CreateDefaultSubobject` + `SetupPlayerInputComponent` 绑定 "BombInteract" + `ReceiveDamage` 受伤打断交互 |
| [BombDefusalGameMode.h](../Source/Blaster/GameMode/BombDefusalGameMode.h) | L13-14 + L89-93 + L193-207 | 前向声明 + `BombActorClass`/`CurrentBomb` 属性 + 6 个新方法声明 |
| [BombDefusalGameMode.cpp](../Source/Blaster/GameMode/BombDefusalGameMode.cpp) | L19-20 + L235-236 + L291-296 + L271-276 + L113-117 + 新代码块 | StartRoundInProgress → `AssignBombToRandomAttacker` + `CheckRoundEnd` 炸弹已安放特殊处理 + `PlayerEliminated` 掉落炸弹 + `StartRoundPrepare` 清理炸弹 + 全部新方法实现 |
| [BlasterHud.h](../Source/Blaster/GameMode/../HUD/BlasterHud.h) | L13-14 + L88-98 | 前向声明 + `BombStatusWidgetClass`/`BombInteractWidgetClass` + Create/Show/Get 方法 |
| [BlasterHud.cpp](../Source/Blaster/GameMode/../HUD/BlasterHud.cpp) | L7-8 + L133-140 + L292-319 | 创建 Widget + ShowBombStatus/ShowBombInteract 实现 |
| [BlasterPlayerController.h](../Source/Blaster/PlayerController/BlasterPlayerController.h) | L78-80 | `UpdateBombStatusUI` / `UpdateBombInteractUI` / `ShowBombPlantedAnnouncement` 声明 |
| [BlasterPlayerController.cpp](../Source/Blaster/PlayerController/BlasterPlayerController.cpp) | L24-25 + L1013-1053 | 炸弹 UI 推送方法实现 |

### 8.3 待用户完成的编辑器配置

完成代码编译后，还需在 UE 编辑器中做以下配置才能跑通完整流程：

| 步骤 | 操作 |
|------|------|
| **1. 输入映射** | Project Settings → Input → 新增 Action: `BombInteract`，绑定按键 `Q` |
| **2. 蓝图 Widget** | 创建 `WBP_BombStatus`（继承 `UBombStatusWidget`）+ `WBP_BombInteract`（继承 `UBombInteractWidget`），实现 BlueprintImplementableEvent 的 UI 逻辑 |
| **3. 蓝图炸弹** | 创建 `BP_BombActor`（继承 `ABombActor`），设置 StaticMesh = Cube + 红色材质 |
| **4. HUD 配置** | 打开 `BP_BlasterHUD`，设 `Bomb Status Widget Class = WBP_BombStatus`，`Bomb Interact Widget Class = WBP_BombInteractWidget` |
| **5. GameMode 配置** | 打开 `BP_BombDefusalGameMode`，设 `Bomb Actor Class = BP_BombActor` |
| **6. 关卡布置** | 在地图中拖放 `BP_BombSite`，调整 `UBoxComponent` 碰撞体大小覆盖下包区域，设 `SiteName = "A"` |

