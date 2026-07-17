# 树莓派（kevin）使用报告与清理记录

> 巡检：2026-07-16　|　清理执行：2026-07-17
> 主机：Raspberry Pi 5（16G），Debian 6.12 内核
> 资源现状（清理后）：内存 9.8G/15.6G 已用｜磁盘 **36G/58G（64%）**｜负载正常

## 一、正在运行的服务清单

### A. 核心保留

| 服务 | 形态 | 说明 |
|---|---|---|
| **frpc** | systemd | 内网穿透 → OVH VPS `51.210.7.13`，映射 8002/8003/8005/3001/3002 |
| **小智智控台全家桶** | Docker ×4（`/opt/xiaozhi-server/`） | server + web(8002) + MySQL + Redis，健康。**核心容器内加载了 SenseVoice 本地 ASR 模型（约 2.5G 内存），属正常工作占用** |
| **CasaOS** | systemd ×6 | 家庭服务器面板 |
| **Jellyfin** | Docker | 媒体服务器（约 580MB） |
| **Home Assistant** | Docker | 智能家居中枢（约 625MB），保留——后续小智经 hass 插件控制家电 |
| **llama.cpp**（Qwen2.5-3B） | 进程 + `llama.service` | 约 2.45GB。**用户确认：游戏 app 测试在用，保留** |
| Portainer | Docker | 占用宿主 8000 端口（故小智 WS 映射在 8005） |
| Nginx Proxy Manager | Docker | 反向代理 80/443/81 |

> ⚠️ 勘误：初版报告误判存在"裸跑旧版小智服务器（2.5G）"。经核实，该 `python app.py` 进程就是 Docker 智控台核心容器本身（`/opt/xiaozhi-esp32-server` 为容器内路径），**只有一套小智在运行**，未做任何停止操作。

### B. 旧版遗留（静态文件，不占内存，待决策）

- `~/xiaozhi-web`（64M）、`~/xiaozhi-admin`（75M）：旧版网页测试页/管理页，nginx 挂在 3001/3002，frp 已映射公网。若确认弃用：删目录 + 删 nginx 站点 + 删 frp 3001/3002 映射。

### C. 用途待确认（本次未动）

| 项 | 占用 | 说明 |
|---|---|---|
| big-bear-chromium + Xvfb | 约 1.2G 内存 | 容器化远程浏览器（3500/3501） |
| Strapi（`/srv/shop-api`）+ ayashop-front | 约 420MB | 商城项目后端 |
| MOSS-TTS-Nano | 0.8G 磁盘 | 本地 TTS 实验遗留 |
| ttyd(7681) / filebrowser(8082) / node-exporter(9100) | 小 | 工具类容器 |
| `~/miniforge3`（2.3G）、`~/llama.cpp`（1.2G）、`~/e-Paper`（1.5G，微雪墨水屏例程建议保留） | 磁盘 | — |

## 二、已执行的清理（2026-07-17）

| 动作 | 结果 |
|---|---|
| 删除已停容器 `jellyfin_old×8` | ✅ |
| `docker image prune -af`（未使用镜像 41 个） | ✅ 回收 **6.7GB** |
| `docker builder prune -af` | ✅ 回收 0.44GB |
| `docker volume prune -f` | ✅（无未挂载数据卷可回收，仅 2KB） |
| 删除安装包残留（Miniforge3.sh / frp.tar.gz / ttyd.tar） | ✅ 约 140MB |
| **合计** | 磁盘 42G→36G，可用 14G→21G |

未执行（用户指示保留）：llama.cpp 及游戏测试相关 app。
未执行（待决策）：`jellyfin_old..._-1`（运行中的重复 Jellyfin）、`my-static-site_old`、B/C 类项目。

## 三、风险清单与处置记录（2026-07-17 更新）

| # | 风险 | 状态 |
|---|---|---|
| 1 | MySQL root 密码为默认 `123456` | ✅ **已处理**：已 ALTER USER 改为强密码，密码经 `/opt/xiaozhi-server/.env`（chmod 600，不入库）注入 compose；实机 compose 已切换为 [deploy/pi/](../deploy/pi/) 仓库版本（原文件备份为 `docker-compose_all.yml.bak`）；web 容器重建后验证正常（HTTP 200 / OTA 正常） |
| 2 | 小智 WebSocket(8005) 公网可达、疑似未开认证 | ✅ **核实无此风险**：数据库确认 `server.auth.enabled = true`，设备需经智控台绑定认证，初版报告推测有误 |
| 3 | frp 映射 3001/3002 指向旧版页面 | ⏳ 待决策：与旧版页面（`~/xiaozhi-web`、`~/xiaozhi-admin`）一起下线（见 B 节） |
| 4 | 智控台 8002 管理界面公网可达 | ⏳ 依赖管理员口令强度；条件允许改为 VPN/白名单访问 |
| 5 | 运行中的重复容器 `jellyfin_old..._-1` | ⏳ 待确认正牌 Jellyfin 数据无缺后停删（约省 580MB 内存） |

## 四、与仓库的关系

- 本仓库已 clone 至树莓派 `~/robot-esp32`
- 智控台部署配置收编于 [deploy/pi/](../deploy/pi/)，更新与 UI 魔改上线流程见该目录 README
