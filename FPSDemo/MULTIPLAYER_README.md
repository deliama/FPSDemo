# FPSDemo 多人网络对战功能说明

## 📋 目录
1. [已实现的功能](#已实现的功能)
2. [Demo设置指南](#demo设置指南)
3. [如何使用多人功能](#如何使用多人功能)
4. [配置说明](#配置说明)
5. [网络架构说明](#网络架构说明)

---

## 已实现的功能

### 1. 核心角色系统 ✅

#### 玩家角色 (ShooterCharacter)
- **第一人称视角控制**
  - 完整的移动系统（WASD移动、鼠标视角控制、跳跃）
  - 第一人称和第三人称网格支持
  - 武器附着系统（支持第一人称和第三人称武器显示）
  
- **生命值系统**
  - 最大生命值配置（默认500HP）
  - 当前生命值网络同步
  - 受伤和死亡处理
  - 重生系统（可配置重生时间，默认5秒）
  
- **团队系统**
  - 团队ID分配（TeamByte：0=团队1, 1=团队2等）
  - 团队ID网络同步
  - 支持多团队对战

#### AI敌人 (ShooterNPC)
- **AI行为系统**
  - 使用StateTree管理AI行为
  - AI感知系统（AIPerception）检测玩家
  - 自动寻路和移动
  - 自动瞄准和射击玩家
  
- **战斗系统**
  - 装备武器并自动射击
  - 可配置的瞄准精度（AimVarianceHalfAngle）
  - 瞄准偏移系统（MinAimOffsetZ/MaxAimOffsetZ）
  
- **生命和死亡**
  - 生命值系统（默认100HP）
  - 死亡状态网络同步
  - 布娃娃物理效果
  - 延迟销毁系统（默认5秒后销毁）

### 2. 武器系统 ✅

#### 武器基类 (ShooterWeapon)
- **武器类型**
  - 支持多种武器（手枪、步枪、榴弹发射器）
  - 第一人称和第三人称网格
  - 武器动画系统（开火动画蒙太奇）
  
- **弹药系统**
  - 弹匣容量配置（MagazineSize，默认10发）
  - 当前子弹数量网络同步
  - 自动/半自动射击模式（bFullAuto）
  - 射击冷却时间（RefireRate，默认0.5秒）
  
- **射击系统**
  - 瞄准系统（支持瞄准偏差AimVariance）
  - 后坐力系统（FiringRecoil）
  - 枪口位置配置（MuzzleSocketName, MuzzleOffset）
  - 投射物生成系统
  
- **AI感知集成**
  - 射击声音生成（ShotLoudness）
  - 声音传播范围（ShotNoiseRange，默认3000cm）
  - 声音标签（ShotNoiseTag）

#### 投射物系统 (ShooterProjectile)
- **子弹投射物**
  - 物理移动和碰撞
  - 伤害计算
  - 网络同步
  
- **榴弹投射物**
  - 爆炸效果
  - 范围伤害
  - 视觉效果

### 3. 游戏模式系统 ✅

#### ShooterGameMode
- **得分系统**
  - 多团队得分追踪（使用TArray<FTeamScoreData>）
  - 击败敌人自动得分
  - 得分网络同步到所有客户端
  
- **胜利机制**
  - 可配置目标分数（TargetScore，默认10分）
  - 自动检测胜利条件
  - 胜利后自动重启游戏
  - 可配置重启延迟（VictoryRestartDelay，默认5秒）
  
- **游戏状态**
  - 游戏结束状态（bGameEnded）网络同步
  - 获胜团队ID（WinningTeam）网络同步
  - 蓝图事件支持（BP_OnTeamVictory）

### 4. UI系统 ✅
- **ShooterUI Widget**
  - 得分显示
  - 生命值显示
  - 弹药数量显示
  - 实时更新（通过Replication回调）

### 5. 拾取系统 ✅
- **武器拾取**
  - 武器拾取点（BP_ShooterPickup）
  - 武器数据表（DT_WeaponData）
  - 武器切换系统

### 6. 多人网络对战支持 ✅
已为以下系统添加了完整的网络复制支持：

#### 角色系统
- **ShooterCharacter**（玩家角色）
  - HP值网络复制（CurrentHP）
  - 团队ID网络复制（TeamByte）
  - 开火控制通过Server RPC实现
  - 伤害处理仅在服务器端执行

- **ShooterNPC**（AI敌人）
  - HP值网络复制（CurrentHP）
  - 团队ID网络复制（TeamByte）
  - 死亡状态网络复制（bIsDead）
  - 所有敌人状态在所有客户端同步

#### 武器系统
- **ShooterWeapon**
  - 子弹数量网络复制（CurrentBullets）
  - 武器状态在所有客户端同步

#### 投射物系统
- **ShooterProjectile**
  - 投射物移动和碰撞网络复制
  - 伤害处理仅在服务器端执行

#### 游戏模式
- **ShooterGameMode**
  - 团队得分网络复制（TeamScores）
  - 游戏结束状态网络复制（bGameEnded）
  - 获胜团队网络复制（WinningTeam）

## Demo设置指南

### 步骤1：设置游戏模式

1. **打开关卡**
   - 在内容浏览器中找到 `Content/Variant_Shooter/Lvl_Shooter.umap`
   - 双击打开关卡

2. **设置游戏模式**
   - 在编辑器中，点击菜单栏 `编辑` → `项目设置`
   - 或者按快捷键 `Ctrl + ,`
   - 在左侧找到 `游戏` → `地图和模式`
   - 设置 `默认游戏模式` 为 `BP_ShooterGameMode`
   - 或者直接在关卡中：
     - 在 `世界大纲视图` 中找到 `World Settings`
     - 在 `Game Mode` 部分，设置 `GameMode Override` 为 `BP_ShooterGameMode`

3. **配置游戏模式参数**（可选）
   - 在关卡中放置一个 `BP_ShooterGameMode` 实例
   - 或在蓝图中编辑 `BP_ShooterGameMode`：
     - `Target Score`: 设置获胜所需分数（默认10分）
     - `Victory Restart Delay`: 设置胜利后重启延迟（默认5秒）
     - `Shooter UI Class`: 设置UI Widget类

### 步骤2：设置玩家生成点

1. **添加玩家起始点**
   - 在 `放置Actor` 面板中搜索 `Player Start`
   - 在关卡中放置多个 `Player Start` 用于多人游戏
   - 为不同团队设置不同的生成点位置

2. **配置玩家角色**
   - 在 `World Settings` → `Game Mode` 中
   - 设置 `Default Pawn Class` 为 `BP_ShooterCharacter`
   - 或在 `BP_ShooterGameMode` 中配置

### 步骤3：设置AI敌人

1. **放置AI敌人**
   - 在内容浏览器中找到 `Content/Variant_Shooter/Blueprints/AI/BP_ShooterNPC`
   - 将 `BP_ShooterNPC` 拖拽到关卡中
   - 放置多个敌人用于测试

2. **配置敌人属性**
   - 选中关卡中的 `BP_ShooterNPC` 实例
   - 在 `Details` 面板中配置：
     - `Team Byte`: 设置敌人所属团队（例如：1=敌方团队）
     - `Current HP`: 设置初始生命值（默认100）
     - `Weapon Class`: 设置敌人使用的武器类型
     - `Aim Range`: 设置瞄准范围（默认10000cm）
     - `Aim Variance Half Angle`: 设置瞄准精度（默认10度）

3. **配置AI控制器**
   - 确保 `BP_ShooterNPC` 使用 `BP_ShooterAIController`
   - AI控制器会自动处理感知和攻击行为

### 步骤4：设置武器拾取点

1. **放置武器拾取点**
   - 在内容浏览器中找到 `Content/Variant_Shooter/Blueprints/Pickups/BP_ShooterPickup`
   - 将拾取点拖拽到关卡中
   - 配置拾取点的武器类型

2. **配置拾取数据**
   - 使用 `DT_WeaponData` 数据表配置武器属性
   - 或在拾取点蓝图中直接设置武器类

### 步骤5：设置UI

1. **配置UI Widget**
   - 在 `BP_ShooterGameMode` 中设置 `Shooter UI Class`
   - 确保UI Widget类已正确配置并继承自 `ShooterUI`

2. **测试UI显示**
   - UI会在游戏开始时自动创建
   - 通过Replication回调自动更新得分和生命值

### 步骤6：配置输入系统

1. **检查输入配置**
   - 输入配置位于 `Content/Input/` 目录
   - 确保以下输入动作已配置：
     - `FireAction`: 开火
     - `SwitchWeaponAction`: 切换武器
     - `MoveAction`: 移动
     - `LookAction`: 视角控制
     - `JumpAction`: 跳跃

### 步骤7：测试设置

1. **单机测试**
   - 直接点击 `Play` 按钮测试基本功能
   - 检查角色移动、射击、敌人AI是否正常

2. **多人测试**
   - 参考下面的"如何使用多人功能"部分

---

## 如何使用多人功能

### 方法1：Play in Editor (PIE) 测试（推荐用于开发）

1. **打开PIE设置**
   - 在UE5编辑器中，点击 `Play` 按钮旁边的下拉箭头
   - 选择 `高级设置` 或直接点击 `Play` 按钮

2. **配置多人设置**
   - `Number of Players`: 设置玩家数量（例如2-4人）
   - `Net Mode`: 
     - `Play As Listen Server`: 第一个窗口作为服务器+玩家
     - `Play As Client`: 所有窗口都是客户端（需要独立服务器）
   - `Run Dedicated Server`: 是否运行专用服务器
   
3. **创建多个窗口**
   - 勾选 `New Editor Window` 为每个玩家创建独立窗口
   - 或者使用 `Number of Players` 自动创建多个窗口

4. **开始测试**
   - 点击 `Play` 开始多人测试
   - 每个窗口代表一个玩家
   - 可以在不同窗口中看到其他玩家的动作

### 方法2：打包后测试（推荐用于最终测试）

### 方法2：打包后测试
1. 打包项目（Package Project）
2. 在一台机器上启动服务器：
   ```
   FPSDemo.exe -server -log
   ```
3. 在其他机器上启动客户端并连接到服务器：
   ```
   FPSDemo.exe <服务器IP地址>
   ```

### 方法3：使用UE5的会话系统（高级）
可以通过集成Online Subsystem（如Steam）来实现更完整的多人会话管理。

### 方法4：局域网测试

1. **在同一局域网内**
   - 确保所有机器在同一网络
   - 关闭防火墙或允许UE5端口通信（默认7777）

2. **启动服务器**
   - 在一台机器上运行：`FPSDemo.exe -server -log`
   - 或使用编辑器：`Play` → `Number of Players: 1` → `Net Mode: Play As Listen Server`

3. **客户端连接**
   - 在其他机器上运行：`FPSDemo.exe <服务器IP地址>`
   - 例如：`FPSDemo.exe 192.168.1.100`

---

## 配置说明

### 游戏模式配置 (ShooterGameMode)

在蓝图 `BP_ShooterGameMode` 或C++类中可以配置：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Target Score` | int32 | 10 | 获胜所需的目标分数 |
| `Victory Restart Delay` | float | 5.0 | 胜利后重启游戏的延迟时间（秒） |
| `Shooter UI Class` | TSubclassOf<UShooterUI> | - | UI Widget类 |

### 角色配置 (ShooterCharacter)

在蓝图 `BP_ShooterCharacter` 或C++类中可以配置：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Team Byte` | uint8 | 0 | 团队ID（0=团队1, 1=团队2等） |
| `Max HP` | float | 500.0 | 最大生命值 |
| `Current HP` | float | 0.0 | 当前生命值（自动同步） |
| `Respawn Time` | float | 5.0 | 重生时间（秒） |
| `Max Aim Distance` | float | 10000.0 | 最大瞄准距离（cm） |

### AI敌人配置 (ShooterNPC)

在蓝图 `BP_ShooterNPC` 或C++类中可以配置：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Team Byte` | uint8 | 1 | 团队ID（通常设置为敌方） |
| `Current HP` | float | 100.0 | 初始生命值 |
| `Weapon Class` | TSubclassOf<AShooterWeapon> | - | 使用的武器类型 |
| `Aim Range` | float | 10000.0 | 瞄准范围（cm） |
| `Aim Variance Half Angle` | float | 10.0 | 瞄准精度偏差（度） |
| `Min Aim Offset Z` | float | -35.0 | 最小垂直瞄准偏移 |
| `Max Aim Offset Z` | float | -60.0 | 最大垂直瞄准偏移 |
| `Deferred Destruction Time` | float | 5.0 | 死亡后销毁延迟（秒） |

### 武器配置 (ShooterWeapon)

在武器蓝图（如 `BP_ShooterWeapon_Rifle`）中可以配置：

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `Magazine Size` | int32 | 10 | 弹匣容量 |
| `Current Bullets` | int32 | 0 | 当前子弹数（自动同步） |
| `b Full Auto` | bool | false | 是否全自动射击 |
| `Refire Rate` | float | 0.5 | 射击间隔（秒） |
| `Aim Variance` | float | 0.0 | 瞄准偏差角度（度） |
| `Firing Recoil` | float | 0.0 | 后坐力强度 |
| `Muzzle Offset` | float | 10.0 | 枪口偏移距离（cm） |
| `Shot Loudness` | float | 1.0 | 射击声音大小 |
| `Shot Noise Range` | float | 3000.0 | 声音传播范围（cm） |

### 关卡设置

在 `World Settings` 中可以配置：

| 设置项 | 说明 |
|--------|------|
| `Game Mode Override` | 设置为 `BP_ShooterGameMode` |
| `Default Pawn Class` | 设置为 `BP_ShooterCharacter` |
| `Player Controller Class` | 设置为 `BP_ShooterPlayerController` |

### 网络设置

在 `Config/DefaultEngine.ini` 中可以配置网络相关设置：

```ini
[/Script/OnlineSubsystemUtils.IpNetDriver]
NetServerMaxTickRate=60
MaxInternetClientRate=20000
MaxClientRate=25000
```

## 配置说明

### 游戏模式配置
在 **ShooterGameMode** 中可以配置：
- **TargetScore**: 获胜所需的目标分数（默认10分）
- **VictoryRestartDelay**: 胜利后重启游戏的延迟时间（默认5秒）

## 网络架构说明

### 服务器权威
- 所有伤害计算在服务器端执行
- 所有得分更新在服务器端执行
- 所有游戏状态变化在服务器端验证

### 客户端预测
- 移动和视角控制使用客户端预测
- 开火操作立即在本地执行（提供即时反馈）
- 然后通过Server RPC同步到服务器

### 网络复制
- 使用UE5的Replication系统自动同步状态
- 关键变量使用 `Replicated` 或 `ReplicatedUsing` 标记
- 通过 `GetLifetimeReplicatedProps` 配置复制属性

## 注意事项

1. **网络延迟**: 在多人游戏中，某些操作可能会有轻微延迟
2. **服务器性能**: 确保服务器有足够的性能处理多个客户端
3. **防火墙**: 如果使用局域网或互联网，确保防火墙允许游戏端口通信
4. **蓝图集成**: 某些功能（如UI更新）需要在蓝图中进一步实现

## 扩展建议

1. **在线子系统**: 集成Steam、Epic Online Services等实现好友系统和匹配
2. **反作弊**: 添加服务器端验证防止作弊
3. **延迟补偿**: 实现延迟补偿系统提高游戏体验
4. **语音聊天**: 添加语音聊天功能
5. **观战模式**: 实现观战者模式

## 技术细节

### 网络复制变量

#### ShooterCharacter
- `CurrentHP`: 使用 `ReplicatedUsing=OnRep_CurrentHP` 实现变化时回调
- `TeamByte`: 使用 `Replicated` 实现基础复制

#### ShooterNPC
- `CurrentHP`: 使用 `ReplicatedUsing=OnRep_CurrentHP` 实现变化时回调
- `TeamByte`: 使用 `Replicated` 实现基础复制
- `bIsDead`: 使用 `Replicated` 实现死亡状态同步

#### ShooterWeapon
- `CurrentBullets`: 使用 `ReplicatedUsing=OnRep_CurrentBullets` 实现子弹数同步

#### ShooterGameMode
- `TeamScores`: 使用 `ReplicatedUsing=OnRep_TeamScores` 实现TArray<FTeamScoreData>复制
- `bGameEnded`: 使用 `Replicated` 实现游戏结束状态同步
- `WinningTeam`: 使用 `Replicated` 实现获胜团队同步

### Server RPC
- `ServerStartFiring()`: 服务器端开火处理
- `ServerStopFiring()`: 服务器端停止开火处理

### 权限检查
所有关键操作都使用 `HasAuthority()` 检查确保只在服务器端执行：
- 伤害计算（TakeDamage）
- 得分更新（IncrementTeamScore）
- 胜利检测（CheckVictoryCondition）
- 游戏重启（RestartGameAfterVictory）

---

## 🎮 快速开始Demo

### 最简单的测试步骤：

1. **打开关卡**
   - 打开 `Content/Variant_Shooter/Lvl_Shooter.umap`

2. **设置游戏模式**
   - `World Settings` → `Game Mode Override` → 选择 `BP_ShooterGameMode`

3. **测试单人游戏**
   - 直接点击 `Play` 按钮
   - 测试移动、射击、敌人AI

4. **测试多人游戏**
   - `Play` → `Number of Players: 2` → `Net Mode: Play As Listen Server`
   - 勾选 `New Editor Window`
   - 点击 `Play`

### 常见问题排查：

1. **敌人不攻击**
   - 检查 `BP_ShooterNPC` 的 `Weapon Class` 是否已设置
   - 检查AI控制器是否正确配置
   - 检查AI感知系统是否启用

2. **得分不更新**
   - 检查 `ShooterGameMode` 是否正确设置
   - 检查敌人死亡时是否调用了 `IncrementTeamScore`
   - 检查网络复制是否正常工作

3. **多人游戏不同步**
   - 确保所有关键变量都标记了 `Replicated`
   - 检查 `GetLifetimeReplicatedProps` 是否正确实现
   - 确保服务器端和客户端都使用相同的游戏模式

4. **UI不显示**
   - 检查 `BP_ShooterGameMode` 的 `Shooter UI Class` 是否设置
   - 检查UI Widget是否正确继承自 `ShooterUI`
   - 检查Replication回调是否正确触发

---

## 📝 更新日志

### 最新更新
- ✅ 修复了 `TMap` 复制问题，改用 `TArray<FTeamScoreData>` 实现团队得分同步
- ✅ 完善了网络复制系统，确保所有游戏状态正确同步
- ✅ 添加了完整的Demo设置指南

### 已知问题
- UI更新可能需要进一步优化
- 某些情况下敌人AI可能需要调整感知范围

