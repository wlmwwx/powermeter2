# 双路智能计量插座 PRD

> Product Requirements Document
> 版本: v0.1 | 日期: 2026-09-09
> 适用硬件: ESP32-C3 (airm2m_core_esp32c3) + HT7017 ×1 (双通道)

## 1. 概述

基于 ESP32-C3 和 HT7017 计量芯片的双通道智能插座产品,集成本地电能计量、WiFi 联网、Web 配网和 MQTT 远程上报能力,适用于家用/办公场景的能耗监测和远程控制。

## 2. 目标

1. 在 ESP32-C3 + Arduino 框架上跑通 HT7017 双通道计量(电压+2 路电流)
2. 提供 WiFi SoftAP + Web 网页配网体验,无需额外 App
3. 通过 MQTT 周期性上报实时电参数和累计电能,兼容 Home Assistant
4. 通过 MQTT 接收控制命令,可远程切断/恢复插座电源(继电器控制)
5. 断电后累积电能不丢失

## 3. 功能需求

### 3.1 硬件接口

| 接口 | 用途 |
|------|------|
| UART0 (Serial) | 调试日志 (115200 8N1) |
| UART1 (Serial1) | HT7017 通信 (4800 8E1, 默认) |
| GPIO | LED 状态灯、继电器 1、继电器 2、按键 |
| 220V 输入 → HT7017 V3P/V3N | 电压采样 |
| CT1 → HT7017 V1P/V1N | 通道 1 电流 |
| CT2 → HT7017 V2P/V2N | 通道 2 电流 |

### 3.2 电能计量

- **双通道独立测量**:通道 1/2 同时测量电压共享、电流独立
- 实时参数(每通道):
  - 电压 U (V, 0.1V 精度)
  - 电流 I (mA, 1mA 精度)
  - 有功功率 P (W, 0.1W 精度)
  - 无功功率 Q (var)
  - 视在功率 S (VA)
  - 功率因数 PF (0.001 精度)
  - 电网频率 f (Hz, 0.01Hz)
- 累计参数(每通道):
  - 有功电能 EP (kWh, 0.01kWh)
  - 无功电能 EQ (kvarh)

### 3.3 HT7017 校准

- 启动时自动读取 SUMChecksum 校验校准参数完整性
- 上电延迟 10ms 后操作寄存器
- 默认校准参数 Ugain/Igain/Pgain/GPhs 等写入 NVS,首次启动写入默认值
- 提供运行时校准入口 (暂不要求自动校准)

### 3.4 网络接入 (SoftAP + Web 配网)

- 首次启动 / 配置缺失 / 配网失败 时自动进入 SoftAP 模式
- AP SSID: `Powermeter_XXXXXX` (MAC 后 6 位)
- AP 密码: `12345678` (默认,可在网页中修改)
- Web 服务 (端口 80):
  - `GET /` - 配网页面(WiFi/MQTT 配置表单)
  - `GET /api/status` - JSON 状态(当前模式、连接状态)
  - `POST /api/wifi` - 提交 WiFi 配置
  - `POST /api/mqtt` - 提交 MQTT 配置
  - `POST /api/reset` - 恢复出厂
- 配网成功后切换 STA 模式,SoftAP 关闭
- 长按按键(>5s) 恢复出厂设置

### 3.5 MQTT 上报

- 周期性上报,默认间隔 5 秒,可在网页中配置 (1~60s)
- 主题:
  - 上报数据: `powermeter/{device_id}/state` (JSON)
  - 上报在线状态: `powermeter/{device_id}/availability` (`online` / `offline`)
  - 订阅控制: `powermeter/{device_id}/cmd` (JSON)
  - 订阅配置: `powermeter/{device_id}/config` (set/get config)
- 上报数据格式 (每通道):
  ```json
  {
    "device_id": "PM_XXXXXX",
    "ts": 1700000000,
    "wifi": {"rssi": -55, "ssid": "..."},
    "ch1": {"u": 220.1, "i": 150, "p": 33.0, "q": 5.0, "s": 33.4, "pf": 0.987, "f": 50.00, "ep": 1.23, "eq": 0.45},
    "ch2": {"u": 220.1, "i": 0,    "p": 0.0,  "q": 0.0, "s": 0.0,  "pf": 1.000, "f": 50.00, "ep": 0.00, "eq": 0.00},
    "relay": {"ch1": true, "ch2": false}
  }
  ```
- 断网重连:指数退避,最长 60s
- MQTT Last Will 设置 availability 为 offline

### 3.6 远程控制

- 订阅 `powermeter/{device_id}/cmd`,接受 JSON:
  ```json
  {"ch1": true, "ch2": false}
  ```
- 控制继电器通断
- 立即上报继电器状态到 state

### 3.7 持久化

- **NVS** (Preferences):
  - WiFi SSID/Password
  - MQTT host/port/user/pass/topic_prefix/client_id
  - 校准参数 Ugain/Igain/Pgain/GPhs/Uoffset/Ioffset
  - 设备名称
- **LittleFS**:
  - 历史电能快照(`/energy.dat`,最近一次断电前的 EP/EQ 双通道值)
  - 启动时若 NVS 中没有累计电能,从此恢复

### 3.8 LED 状态指示

| 状态 | 颜色/行为 |
|------|----------|
| 启动中 | LED 快闪 (100ms) |
| SoftAP 配网中 | LED 慢闪 (1s on / 1s off) |
| WiFi 连接中 | LED 双闪 |
| WiFi 已连接 / MQTT 未连接 | LED 常亮 |
| 全部正常 | LED 熄灭 |
| 故障 | LED 持续快闪 5 次,停 1s 循环 |

## 4. 非功能需求

- **稳定性**:连续运行 7 天不重启
- **存储**:断电后累积电能恢复误差 < 0.01kWh
- **上电时间**:配网完成 → MQTT 连接建立 ≤ 10s
- **MQTT 上报抖动**:周期 ±10%
- **OTA 升级**:预留分区表 (暂不实现,但 partition table 需支持)

## 5. 架构总览

```
+--------------------+      UART(4800 8E1)      +------------------+
|     ESP32-C3       | <-----------------------> |      HT7017       |
|                    |                          |                  |
|  - WiFi STA/AP     |                          |  - V3P/V3N (U)   |
|  - Web Server      |                          |  - V1P/V1N (I1)  |
|  - MQTT Client     |                          |  - V2P/V2N (I2)  |
|  - Preferences     |                          |  - PF pin (Q1)   |
|  - LittleFS        |                          |  - QF pin (Q2)   |
+--------------------+                          +------------------+
        |                                                |
   GPIO (Relay1, Relay2, LED, Key)             CT1, CT2, 电压互感器
```

## 6. 目录结构

```
powermeter2/
├── docs/
│   ├── PRD.md                    # 本文档
│   ├── ht7107_user_manual.pdf
│   └── superpowers/plans/
│       └── YYYY-MM-DD-*.md       # 实施计划
├── lib/
│   ├── HT7017/                   # 重写: ESP32 Arduino 驱动
│   │   ├── HT7017.h
│   │   └── HT7017.cpp
│   ├── Meter/                    # 重写: 计量应用层
│   │   ├── Meter.h
│   │   └── Meter.cpp
│   ├── Config/                   # 新增: NVS 持久化封装
│   │   ├── Config.h
│   │   └── Config.cpp
│   ├── WebServer/                # 新增: SoftAP + Web
│   │   ├── WebServer.h
│   │   └── WebServer.cpp
│   ├── MqttClient/               # 新增: PubSubClient 封装
│   │   ├── MqttClient.h
│   │   └── MqttClient.cpp
│   └── EnergyStore/              # 新增: LittleFS 持久化
│       ├── EnergyStore.h
│       └── EnergyStore.cpp
├── src/
│   └── main.cpp                  # 入口:初始化 + loop 调度
├── include/                      # 公共头 (若需要)
├── test/                         # 单元测试
├── platformio.ini
└── README.md
```

## 7. 阶段划分

| 阶段 | 名称 | 内容 |
|------|------|------|
| M1 | HT7017 驱动 | UART 读写、寄存器抽象、计量数据读取、能量累加 |
| M2 | 校准 & 配置存储 | NVS 读写、校准参数持久化、LittleFS 电能快照 |
| M3 | Web 配网 | SoftAP 启动、Web 服务、表单提交、WiFi STA 切换 |
| M4 | MQTT 上报 | 连接、JSON 序列化、周期上报、命令订阅、Last Will |
| M5 | 整合 & 调试 | 状态机、LED 指示、按键处理、稳定性测试 |

## 8. 版本控制

- Git 分支模型:`main`(稳定) + `feat/<name>`(功能) + `fix/<name>`(修复)
- 提交规范:Conventional Commits (`feat:` / `fix:` / `docs:` / `refactor:` / `test:` / `chore:`)
- 每次任务完成 = 一次提交,需可独立编译
- 标签:`v0.1.0` (M1 完成) / `v0.2.0` (M2) / ...

## 9. 风险与限制

| 风险 | 应对 |
|------|------|
| HT7017 UART 兼容性问题 | 使用 HardwareSerial,严格按 datasheet 时序 |
| NVS 容量限制 | 大对象走 LittleFS,小配置走 NVS |
| Web 配网与 STA 共存 | 配网期间 SoftAP 强制开启,STA 仅在配置有效时尝试连接 |
| WiFi 不稳定导致电能丢失 | LittleFS 每 60s 持久化一次电能快照 |
| ESP32-C3 RAM 限制 (400KB) | 减小 MQTT payload、Web 页面分块,禁用 WebSocket |

## 10. 不在范围

- 远程 OTA 升级 (预留,不实现)
- 本地 LCD 显示 (无硬件)
- 多路扩展 (本期仅支持一片 HT7017 双通道)
- 自动校准算法 (需专业仪器)
- TLS / HTTPS (本期 MQTT 明文,Web HTTP)