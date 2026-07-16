# 小智（Xiaozhi）ESP32 陪伴机器人 · 定制化二次开发工程

> 本仓库是基于开源"小智"生态的**完全定制化二次开发**工作区，目标是：
> 搭建**自有的在线小智服务器**，并通过它控制**定制的 ESP32 小智陪伴机器人**。
>
> 本 README 记录当前代码基线（Benchmark），作为后续定制开发的对照基准。
> 基线建立日期：2026-07-16

---

## 一、目录总览

```
D:\xiaozhi20260403
├── xiaozhi-esp32/          # 设备端固件（虾哥官方开源项目，v2 版本）
├── xiaozhi-esp32-server/   # 服务器端（xinnan-tech 开源后端，Python + Java + Vue）
└── xiaozhi-web/            # 浏览器端测试页（HTML/JS，含 Live2D 形象，无需硬件即可联调）
```

三者的关系：

```
┌─────────────────┐   WebSocket / MQTT+UDP    ┌──────────────────────┐
│  xiaozhi-esp32   │ ◄──────────────────────► │ xiaozhi-esp32-server │
│  (ESP32 固件)    │   OPUS 音频流 + JSON 消息  │  (自建在线服务器)      │
└─────────────────┘                           │  ASR → LLM → TTS     │
┌─────────────────┐   WebSocket               │  插件 / MCP / 声纹    │
│  xiaozhi-web     │ ◄──────────────────────► │  知识库 / 记忆        │
│  (浏览器测试页)   │                           └──────────────────────┘
└─────────────────┘
```

---

## 二、各模块详细说明

### 1. `xiaozhi-esp32/` — 设备端固件

- **来源**：[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)（虾哥官方项目），**v2 版本**（注意：v2 与 v1 分区表不兼容，无法通过 OTA 从 v1 升级）
- **框架**：ESP-IDF（C/C++，CMake 构建），本目录已包含 `build/` 与 `sdkconfig`，说明此前已成功编译过
- **支持芯片**：ESP32、ESP32-C3、ESP32-C5、ESP32-C6、ESP32-S3、ESP32-P4（对应各 `sdkconfig.defaults.*`）
- **支持硬件**：`main/boards/` 下共 **96 块开发板**的适配代码（立创实战派、ESP-BOX3、M5Stack CoreS3、微雪 AMOLED 等）

**核心能力**：

| 能力 | 说明 |
|---|---|
| 离线语音唤醒 | 基于 ESP-SR，可自定义唤醒词 |
| 音频链路 | OPUS 编解码，流式 ASR + LLM + TTS 语音交互 |
| 通信协议 | WebSocket 或 MQTT+UDP 两种方式连接服务器 |
| 声纹识别 | 3D-Speaker，识别当前说话人身份 |
| 显示 | OLED/LCD 屏幕、表情显示、自定义字体/表情/聊天背景 |
| 设备端 MCP | 通过 MCP 协议暴露设备控制能力（音量、灯光、电机、GPIO 等）给大模型 |
| 其他 | 电量显示与电源管理、多语言（中/英/日）、Wi-Fi / ML307 4G |

**关键目录**：
- `main/boards/` — 各开发板硬件适配（**定制硬件时主要改这里**）
- `main/audio/` — 音频编解码与音频服务
- `partitions/v2/` — v2 分区表
- `docs/websocket.md` — 通信协议文档

---

### 2. `xiaozhi-esp32-server/` — 服务器端（本项目的核心）

- **来源**：[xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server)，MIT 协议
- **作用**：按"小智通信协议"实现的完整自建后端，替代虾哥官方的在线服务
- **部署**：支持 Docker（`docker-compose.yml` 单服务 / `docker-compose_all.yml` 全模块）

包含四个子模块（`main/` 下）：

| 模块 | 技术栈 | 作用 |
|---|---|---|
| `xiaozhi-server` | Python | **核心服务**：WebSocket(:8000) + HTTP/OTA/视觉(:8003)，负责设备接入与 ASR→LLM→TTS 流水线 |
| `manager-api` | Java (Spring Boot, Maven) | 智控台后端 API |
| `manager-web` | Vue 2 | 智控台网页（配置设备、模型、角色、提示词等） |
| `manager-mobile` | — | 移动端管理 |

**`xiaozhi-server` 核心结构**：

```
xiaozhi-server/
├── app.py                    # 入口
├── config.yaml               # 全量默认配置（勿直接改，用 data/.config.yaml 覆盖）
├── config_from_api.yaml      # 使用智控台时的配置模式
├── agent-base-prompt.txt     # 机器人基础人格提示词（定制人格改这里）
├── core/
│   ├── providers/            # 可插拔的模型提供商层
│   │   ├── asr/  llm/  tts/  vad/  vllm/   # 语音识别 / 大模型 / 语音合成 / 活动检测 / 视觉
│   │   ├── intent/  memory/  tools/        # 意图识别 / 记忆 / 工具(MCP)
│   ├── handle/               # 消息与会话处理
│   ├── api/                  # HTTP 接口（OTA、视觉分析等）
│   └── utils/
├── plugins_func/functions/   # 内置插件：天气、新闻、点歌、Home Assistant 家电控制、
│                             # RAGFlow 知识库检索、角色切换、退出意图等
├── mcp_server_settings.json  # 云端 MCP 接入点配置
└── performance_tester.py     # 各模型供应商响应速度测试工具
```

**配置要点**（重要约定）：
- 开发时在 `xiaozhi-server/` 下创建 `data/.config.yaml`，只写需要覆盖的项；系统优先读它，避免改动 `config.yaml` 泄漏密钥
- 若启用智控台（manager-api/web），则以智控台数据库中的配置为准，yaml 配置不生效
- 默认端口：WebSocket `:8000`（路径 `/xiaozhi/v1/`）、HTTP/OTA `:8003`
- 认证默认关闭（`server.auth.enabled: false`），支持设备白名单

**已支持的能力**：MQTT+UDP 网关、WebSocket、MCP 接入点、声纹识别、知识库、多种 LLM（Qwen / DeepSeek / ChatGLM 等）与 ASR/TTS 供应商自由组合。

---

### 3. `xiaozhi-web/` — 浏览器测试页

- 纯静态 HTML/JS/CSS，含 Live2D 虚拟形象（`js/live2d/`）
- 作用：在浏览器中直接通过 WebSocket 连接自建服务器，进行语音对话联调，**无需 ESP32 硬件在手**
- 使用方式：不能用 `file://` 直接打开，需 HTTP 服务托管，例如：
  ```
  cd xiaozhi-web
  python -m http.server 8006
  ```
- ⚠️ 注意：`index.html` 等文件为 **GBK 编码**，编辑时注意不要用 UTF-8 强行保存导致乱码

---

## 三、基线（Benchmark）状态

| 项 | 状态 |
|---|---|
| 固件编译 | `xiaozhi-esp32/build/` 存在历史编译产物（sdkconfig 已生成） |
| 服务器部署 | 未部署，配置为出厂默认（`config.yaml` 未覆盖，无 `data/.config.yaml`） |
| 版本控制 | ✅ 已初始化 git 并推送基线至 [robot-esp32](https://github.com/Ayacloud-KEWEN/robot-esp32)（分支 main） |
| 定制修改 | 无——当前为上游原始代码基线 |

> 后续所有定制开发都与本基线对照，每一处修改均可通过 `git diff` 追溯。

---

## 四、本项目选型定稿（2026-07-16）

| 项 | 选型 | 说明 |
|---|---|---|
| 首台设备 / 开发板 | 微雪 ESP32-S3-ePaper-1.54 套件 | 固件已内置板级适配：`xiaozhi-esp32/main/boards/waveshare/esp32-s3-epaper-1.54/`，menuconfig 选中即可 |
| 定制机器人核心 | ESP32-S3 N16R8 机芯盒 | 16MB Flash + 8MB PSRAM，满足小智 v2 + 离线唤醒词要求 |
| LLM | DeepSeek API（deepseek-chat） | 服务器 `config.yaml` 内置 `DeepSeekLLM`，key 填入 `data/.config.yaml`（已被 .gitignore 排除，不入库） |
| ASR | FunASR（本地，免费） | 初期方案，后续可按延迟/效果升级 |
| TTS | EdgeTTS（免费） | 初期方案，后续可换可克隆音色的方案（如 CosyVoice） |
| 代码仓库 | https://github.com/Ayacloud-KEWEN/robot-esp32 | 单仓库（monorepo），基线已提交 |

注意事项：
- 墨水屏（ePaper）刷新慢，表情/动画设计需比 LCD 简化，但省电、观感温润，适合陪伴场景
- API 密钥只写在 `xiaozhi-server/data/.config.yaml`，**永远不提交到 git**

## 五、定制开发路线图（Roadmap）

1. **建立版本控制**：根目录 `git init`，提交上游基线代码
2. **服务器跑通**（最小闭环）：
   - 部署 `xiaozhi-server`（本地 Docker 或云服务器）
   - 在 `data/.config.yaml` 中配置所选 LLM / ASR / TTS 的 API Key
   - 用 `xiaozhi-web` 测试页验证语音对话全链路
3. **智控台部署**：启用 manager-api + manager-web，实现网页化配置管理
4. **人格与能力定制**：
   - 修改 `agent-base-prompt.txt` 定制陪伴机器人性格
   - 在 `plugins_func/functions/` 编写自定义插件；通过 MCP 扩展能力（家电控制、提醒、知识库等）
5. **固件定制**：
   - 修改 OTA / WebSocket 地址指向自建服务器
   - 在 `main/boards/` 为定制硬件新建板级适配（屏幕、按键、电机、GPIO）
   - 自定义唤醒词、表情、UI
6. **真机联调**：烧录固件 → 连接自建服务器 → 端到端验证
7. **性能基准**：用 `performance_tester.py` 测试各模型供应商延迟，选定最优组合

---

## 六、学习文档

配套的循序渐进学习教程位于 [docs/learning/](docs/learning/)：

1. [00-学习路线总览](docs/learning/00-学习路线总览.md) — 学习地图与周计划
2. [01-ESP32基础与开发环境](docs/learning/01-ESP32基础与开发环境.md)
3. [02-小智固件架构解析](docs/learning/02-小智固件架构解析.md)
4. [03-服务器与网络部署](docs/learning/03-服务器与网络部署.md)
5. [04-大模型与语音AI流水线](docs/learning/04-大模型与语音AI流水线.md)
6. [05-陪伴机器人定制实战](docs/learning/05-陪伴机器人定制实战.md)

## 七、参考资料

- 小智官方固件：https://github.com/78/xiaozhi-esp32
- 自建服务器项目：https://github.com/xinnan-tech/xiaozhi-esp32-server
- 小智通信协议（飞书文档）：https://ccnphfhqs21z.feishu.cn/wiki/M0XiwldO9iJwHikpXD5cEx71nKh
- 《小智 AI 聊天机器人百科全书》：https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb
- 自定义 Assets 生成器：https://github.com/78/xiaozhi-assets-generator
