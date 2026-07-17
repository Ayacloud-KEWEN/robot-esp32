# 树莓派（kevin, 192.168.1.29）部署配置

> 本目录管理树莓派上智控台全家桶的部署配置，与实机 `/opt/xiaozhi-server/` 对应。
> 实机现状详见 [../../docs/pi-usage-report.md](../../docs/pi-usage-report.md)。

## 实机部署信息

| 项 | 值 |
|---|---|
| 主机 | 树莓派 Pi 5 16G，主机名 `kevin`，内网 `192.168.1.29`，用户 `fdcaptain` |
| 部署目录 | `/opt/xiaozhi-server/`（compose + data/models/plugins_func/mysql/uploadfile） |
| 仓库克隆 | `~/robot-esp32`（本仓库，用于后续构建自定义镜像） |
| 智控台 | `http://192.168.1.29:8002` |
| OTA 接口 | `http://192.168.1.29:8002/xiaozhi/ota/`（全模块模式下 OTA 在 8002） |
| WebSocket | `ws://192.168.1.29:8005/xiaozhi/v1/`（宿主 8005 → 容器 8000，因 8000 被 Portainer 占用） |
| 视觉接口 | `http://192.168.1.29:8003/mcp/vision/explain` |
| 公网入口 | frp → OVH VPS `51.210.7.13`（映射 8002/8003/8005/3001/3002），frpc 配置在 `~/frp_0.68.0_linux_arm64/frpc.toml` |

## 与仓库的同步方式

- 树莓派当前跑的是**官方预编译镜像**（`ghcr.nju.edu.cn/xinnan-tech/...`），不含本仓库的定制修改。
- 修改 `docker-compose_all.yml` 时：改本目录文件 → commit → 在树莓派 `~/robot-esp32` 里 `git pull` → 复制到 `/opt/xiaozhi-server/` → `docker compose up -d`。
- **UI 魔改上线流程**（开始魔改 manager-web 后）：在树莓派上用上游 `Dockerfile-web` 从 `~/robot-esp32` 源码构建自有镜像，并把 compose 中 `image:` 换为本地镜像名。server 同理用 `Dockerfile-server`。

## 密码说明

MySQL root 密码通过环境变量 `MYSQL_ROOT_PASSWORD` 注入，实机存放于 `/opt/xiaozhi-server/.env`（chmod 600，不入库）。已于 2026-07-17 由默认 `123456` 改为强密码（`SPRING_DATASOURCE_DRUID_PASSWORD` 同变量注入，自动同步）。注意：MySQL 数据卷已初始化，今后再改密码需在容器内 `ALTER USER`，仅改 `.env` 不生效。
