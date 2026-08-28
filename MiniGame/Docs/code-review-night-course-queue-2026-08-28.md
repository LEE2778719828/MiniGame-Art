# Night Course Queue 回退与代码审查报告

日期：2026-08-28  
范围：NightCourse Atom 生成、LandingPoint/Bridge 变换、GameInstance 队列接入  
目标：移除新旋转跑酷模式，只保留 Day -> Night queue。

## 需求符合性

- ✅ 保留独立 UNightCourseQueueData，支持主路、是否启用岔路、ForkPair、主路 Atom 数量和岔路 Atom 数量。
- ✅ 队列默认从第一个元素开始，正常进入下一天时推进，失败重试保留当前元素。
- ✅ 队列索引写入 SaveGame，并将 SaveVersion 提升到 5。
- ✅ GameInstance Blueprint 和项目 GameInstance 设置仍指向队列配置。
- ✅ 删除 BP_Atom017 与 BP_NightAtom_017 新跑酷模式资产。
- ✅ 移除全姿态 FTransform 传递，恢复原有二维 WorldLocation + YawDeg 路线语义。

## 发现并处理的问题

### 🛑 严重：Atom、LandingPoint 与怪物落点坐标语义被混用

此前新增 bUseFullTransform/PoseTransform 后，局部 LandingPoint 在提取、路线组合、运行时 Spawn 和 Pawn 同步阶段使用了不同的空间语义。普通平面 Atom 仍按旧的 WorldLocation/YawDeg 运行，导致生成出来的 Atom、怪物和 LandingPoint 不能保证重合。

已回退：
- NightCourseTypes.h 中的全姿态字段。
- NightCourseAtomActor.cpp / NightCourseForkAtomActor.cpp 中的全姿态提取。
- NightCourseDirector.cpp 中的全姿态组合、Spawn、Pawn 朝向和Roadside读取。
- NightBridgeSegmentActor.cpp 与 NightCourseHost.cpp 中的全姿态桥处理。

### ⚠️ 重要：旧编辑器进程未加载队列

运行日志曾出现 queueIndex=-1，说明当时使用的是旧 GameInstance 实例，旧 DA_Course 仍启用岔路。保留了队列 DA 的运行时回退加载，以便重新编译后即使蓝图默认值尚未刷新也能读取项目队列资产。

## 保留的 queue 代码路径

- FNightBootstrap：队列覆盖字段。
- FSNightBootstrap：队列字段的 Day/GameInstance 层转发。
- USChefGameInstance::SelectCourseQueueEntryForNightStart：选择和推进队列。
- USChefGameInstance::ApplyActiveCourseQueueEntry：将当前条目写入 Bootstrap。
- UNightCourseDirector：按当前队列覆盖主路/岔路长度及开关。
- NightCourseHost.cpp：将 Sandbox Bootstrap 传给 NightCourse Bootstrap。

## 静态检查

- git diff --check：通过；仅有仓库既有 CRLF/LF 警告。
- 全仓源码检索 bUseFullTransform、PoseTransform、BP_NightAtom_017、BP_Atom017：无残留。
- 未启动自动化测试或运行时测试。
- UE 当前占用编辑器 DLL，因此未自动编译，避免再次触发 LNK1104。

## 结论

当前工作区的路线生成层已恢复旧的平面 Atom/LandingPoint 逻辑，新增部分只负责 queue 数据和队列状态传递。下一步需要在关闭 UE 后重新编译，再重新打开编辑器；本报告不包含自动保存或自动运行验证。