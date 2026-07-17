# 树莓派（kevin）使用报告与清理建议

> 巡检日期：2026-07-17　|　主机：Raspberry Pi 5（16G），Debian 6.12 内核，已连续运行 19 天
> 资源现状：内存 **10G/15.6G 已用**（偏高）｜磁盘 **42G/58G 已用（76%，偏高）**｜负载 2.3（四核，正常偏忙）

## 一、正在运行的服务清单

### A. 核心保留（用户指定：frp、小智、CasaOS、Jellyfin、+Home Assistant 建议保留）

| 服务 | 形态 | 说明 |
|---|---|---|
| **frpc** | systemd 服务 | 内网穿透客户端 → OVH VPS `51.210.7.13`，映射 8002/8003/8005/3001/3002。健康 |
| **小智智控台全家桶** | Docker ×4（`/opt/xiaozhi-server/`） | server + web(8002) + MySQL + Redis，全部健康，镜像为 2026-03 版 |
| **CasaOS** | systemd 服务 ×6 | 家庭服务器面板，管理着大部分 Docker 应用 |
| **Jellyfin** | Docker | 媒体服务器，运行正常（约 580MB 内存） |
| **Home Assistant** | Docker | 智能家居中枢（约 625MB）。**建议保留**——后续小智机器人可通过内置 hass 插件控制家里设备，是陪伴机器人的重要拼图 |
| Portainer | Docker | Docker 管理面板。可留可删；注意它占了宿主机 8000 端口（因此小智 WS 才映射到 8005） |
| Nginx Proxy Manager | Docker | 反向代理（80/443/81）。若在用它做域名转发则保留 |

### B. 发现的"遗产"——两套小智并存 ⚠️（本次最重要发现）

| 项 | 说明 |
|---|---|
| Docker 版智控台 | `/opt/xiaozhi-server/`，正常运行，**这是要保留的那套** |
| **裸跑旧版 xiaozhi-server** | `/opt/xiaozhi-esp32-server/` 下直接 `python app.py` 运行，**吃掉 2.5GB 内存**（加载了 SenseVoice 本地 ASR 模型）。与 Docker 版功能重复 |
| `~/xiaozhi-web`、`~/xiaozhi-admin` | 旧版网页测试页/管理页，由 nginx 挂在 3001/3002 端口（frp 也映射了它们） |

→ 需确认裸跑版是否还有设备在连。若无，停掉它可立刻释放 **2.5GB 内存**。

### C. 大内存但用途待确认

| 进程/容器 | 内存 | 说明 |
|---|---|---|
| **llama.cpp llama-server**（Qwen2.5-3B） | **2.45GB** | 本地小模型推理服务。若当初是实验、现在小智已用 DeepSeek API，可停掉 |
| big-bear-chromium + Xvfb | 约 1.2GB 合计 | 容器化远程浏览器（3500/3501 端口），常驻很费内存，不用就停 |
| Strapi（`/srv/shop-api`）+ ayashop-front | 约 420MB | 一个商城项目的后端 CMS，是否还在用？ |
| MOSS-TTS-Nano（`~/` 下 802MB 文件） | 磁盘 | 本地 TTS 实验遗留 |

### D. 明确的清理对象（低风险）

| 对象 | 位置 | 可回收 |
|---|---|---|
| `jellyfin_old_old_old_old_old_old_old`（运行中，与正牌 Jellyfin 重复）及同名已停容器 | Docker | 内存+磁盘 |
| `my-static-site_old`（与 my-static-site 重复） | Docker (8080) | 少量 |
| 未使用 Docker 镜像 41 个 | `docker system df` | **5.9GB 磁盘** |
| 未挂载卷 8 个 + 构建缓存 | 同上 | 约 0.7GB |
| 安装包残留（`Miniforge3.sh` 101M、`frp.tar.gz` 12M、`ttyd.tar` 26M 等） | `~/` | 约 150MB |
| ttyd（网页终端 7681）、filebrowser（8082）、node-exporter（9100） | Docker | 不用就停，均为小件 |

## 二、磁盘去向（58G 已用 42G）

```
/var/lib/docker      33G   ← 大头；清理未用镜像可回收 5.9G
~/miniforge3        2.3G   ← conda 环境（裸跑小智/实验用？与裸跑版一起决定去留）
~/e-Paper           1.5G   ← 微雪墨水屏例程（与你的 ePaper 硬件相关，建议保留）
~/llama.cpp         1.2G   ← 本地 LLM 实验
~/MOSS-TTS-Nano     0.8G   ← 本地 TTS 实验
```

## 三、安全隐患（建议尽快处理）

1. **MySQL root 密码是默认的 `123456`**，且智控台 8002 经 frp 暴露公网。MySQL 本身未暴露（仅容器内网 ✅），但建议改强密码（compose 中两处同步改）。
2. **小智 WebSocket(8005) 经 frp 公网可达且服务端认证未开**——任何知道地址的人都能连上你的服务器消耗你的 DeepSeek 额度。上线前在智控台/配置中开启认证。
3. frp 映射的 3001/3002 指向旧版页面，若旧版下线应同步删掉这两条映射，减少暴露面。

## 四、建议的清理动作（按风险从低到高排序）

**第一批（零风险，立刻可做）**：
```bash
docker rm jellyfin_old_old_old_old_old_old_old_old        # 已停的旧容器
docker image prune -a          # 清未使用镜像，回收约5.9G（会保留在用镜像）
docker builder prune           # 构建缓存 0.4G
rm ~/Miniforge3-Linux-aarch64.sh ~/frp_0.68.0_linux_arm64.tar.gz ~/ttyd.tar
```

**第二批（先确认再做）**：
- 确认无设备连接裸跑版后：停掉 `/opt/xiaozhi-esp32-server` 的 `python app.py`（+2.5G 内存）
- 确认小智全部走 DeepSeek 后：停掉 llama-server（+2.45G 内存）
- 停掉重复的 `jellyfin_old_..._-1`（运行中那个）、`my-static-site_old`
- 不再用的话停掉：big-bear-chromium（+1.2G）、ttyd、filebrowser

**第三批（决策类）**：
- Strapi 商城项目是否迁移/归档
- `~/xiaozhi-web`、`~/xiaozhi-admin` 旧版页面下线，frp 删除 3001/3002 映射
- MOSS-TTS-Nano、llama.cpp、miniforge3 目录归档或删除（合计约 4.3G）

**预期收益**：内存从 10G 降到约 **4~5G**，磁盘回收约 **10G+**，为后续 FunASR 本地识别或更多小智功能腾出充足空间。

## 五、后续（与仓库的关系）

- 本仓库已 clone 到树莓派 `~/robot-esp32`
- 智控台部署配置已收编到仓库 [deploy/pi/](../deploy/pi/)，更新流程见该目录 README
