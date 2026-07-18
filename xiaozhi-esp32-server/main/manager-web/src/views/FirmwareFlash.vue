<template>
  <div class="welcome">
    <HeaderBar />
    <div class="operation-bar">
      <h2 class="page-title">固件烧录</h2>
    </div>
    <div class="main-wrapper">
      <div class="content-panel">
        <!-- 环境检测提示 -->
        <el-alert v-if="!serialSupported" type="error" :closable="false" show-icon
          title="当前浏览器不支持 Web Serial，无法在线烧录">
          <div>
            请使用 <b>Chrome</b> 或 <b>Edge</b> 浏览器，并通过 <b>HTTPS</b> 或 <b>localhost</b> 访问本页面。<br />
            临时方案：Chrome 地址栏打开 <code>chrome://flags/#unsafely-treat-insecure-origin-as-secure</code>，
            填入本站地址（如 <code>http://192.168.1.29:8002</code>）并启用后重启浏览器。
          </div>
        </el-alert>
        <el-alert v-else type="success" :closable="false" show-icon
          title="浏览器支持 Web Serial，可以在线烧录">
          <div>用 USB 线连接 ESP32 板子后点击下方按钮。若连接后无法进入下载模式，请按住板上 BOOT 键再插入 USB。</div>
        </el-alert>

        <!-- 第一步：选择固件 -->
        <el-card class="step-card">
          <div slot="header"><b>第 1 步 · 选择固件</b></div>
          <el-radio-group v-model="firmwareSource">
            <el-radio label="server">从固件库选择（OTA 管理中已上传的固件）</el-radio>
            <el-radio label="local">上传本地 .bin 文件</el-radio>
          </el-radio-group>

          <div v-if="firmwareSource === 'server'" class="source-block">
            <el-select v-model="selectedFirmwareId" placeholder="请选择固件" style="width: 420px" @focus="loadFirmwareList">
              <el-option v-for="fw in firmwareList" :key="fw.id"
                :label="`${fw.firmwareName} (${fw.type} / v${fw.version})`" :value="fw.id" />
            </el-select>
            <div class="hint">提示：建议上传 <code>idf.py merge-bin</code> 生成的 merged-binary.bin（从 0x0 烧录的完整固件）</div>
          </div>

          <div v-else class="source-block">
            <input type="file" accept=".bin" @change="onLocalFile" />
            <span v-if="localFileName" class="hint">已选择：{{ localFileName }}（{{ (localFileSize / 1024 / 1024).toFixed(2) }} MB）</span>
          </div>

          <div class="source-block">
            烧录起始地址：
            <el-input v-model="flashAddress" style="width: 140px" size="small" />
            <span class="hint">合并固件(merged-binary.bin)用 0x0；仅 app 分区固件请按分区表填写</span>
          </div>
          <div class="source-block">
            <el-checkbox v-model="eraseAll">烧录前擦除整片 Flash（新板子/换固件建议勾选，耗时稍长）</el-checkbox>
          </div>
        </el-card>

        <!-- 第二步：连接并烧录 -->
        <el-card class="step-card">
          <div slot="header"><b>第 2 步 · 连接设备并烧录</b></div>
          <el-button type="primary" icon="el-icon-link" :disabled="!serialSupported || flashing" @click="startFlash">
            {{ flashing ? '烧录中…' : '连接串口并开始烧录' }}
          </el-button>
          <el-button v-if="flashing" type="danger" size="small" @click="abortRequested = true">中止</el-button>
          <el-progress v-if="flashing || progress > 0" :percentage="progress" :status="progressStatus" style="margin-top: 16px" />
        </el-card>

        <!-- 日志 -->
        <el-card class="step-card">
          <div slot="header"><b>烧录日志</b></div>
          <pre ref="logBox" class="log-box">{{ logText || '（等待开始）' }}</pre>
        </el-card>

        <el-card class="step-card">
          <div slot="header"><b>烧录完成后</b></div>
          <ol class="after-steps">
            <li>拔插 USB 或按 RST 键重启板子</li>
            <li>手机连接板子发出的配网热点，配置家里 Wi-Fi</li>
            <li>设备将自动连接本服务器 OTA 接口并语音播报 6 位激活码</li>
            <li>到「设备管理」中输入激活码完成绑定，即可开始对话</li>
          </ol>
        </el-card>
      </div>
    </div>
  </div>
</template>

<script>
import HeaderBar from "@/components/HeaderBar.vue";
import otaApi from "@/apis/module/ota";

const ESPTOOL_CDN = "https://cdn.jsdelivr.net/npm/esptool-js@0.4.7/+esm";

export default {
  name: "FirmwareFlash",
  components: { HeaderBar },
  data() {
    return {
      serialSupported: typeof navigator !== "undefined" && !!navigator.serial,
      firmwareSource: "server",
      firmwareList: [],
      firmwareListLoaded: false,
      selectedFirmwareId: null,
      localFileData: null,   // 二进制字符串
      localFileName: "",
      localFileSize: 0,
      flashAddress: "0x0",
      eraseAll: true,
      flashing: false,
      abortRequested: false,
      progress: 0,
      progressStatus: null,
      logText: "",
    };
  },
  mounted() {
    this.loadFirmwareList();
  },
  methods: {
    log(msg) {
      this.logText += (this.logText ? "\n" : "") + msg;
      this.$nextTick(() => {
        const el = this.$refs.logBox;
        if (el) el.scrollTop = el.scrollHeight;
      });
    },
    loadFirmwareList() {
      if (this.firmwareListLoaded) return;
      otaApi.getOtaList({ page: 1, limit: 100 }, ({ data }) => {
        const rows = (data && data.data && (data.data.list || data.data)) || [];
        this.firmwareList = Array.isArray(rows) ? rows : [];
        this.firmwareListLoaded = true;
      });
    },
    onLocalFile(e) {
      const file = e.target.files && e.target.files[0];
      if (!file) return;
      this.localFileName = file.name;
      this.localFileSize = file.size;
      const reader = new FileReader();
      reader.onload = () => {
        const bytes = new Uint8Array(reader.result);
        let bin = "";
        const CHUNK = 0x8000;
        for (let i = 0; i < bytes.length; i += CHUNK) {
          bin += String.fromCharCode.apply(null, bytes.subarray(i, i + CHUNK));
        }
        this.localFileData = bin;
        this.log(`已读取本地固件 ${file.name}，${file.size} 字节`);
      };
      reader.readAsArrayBuffer(file);
    },
    async fetchServerFirmware() {
      return new Promise((resolve, reject) => {
        otaApi.getDownloadUrl(this.selectedFirmwareId, async ({ data }) => {
          try {
            const url = (data && data.data) || data;
            this.log(`从固件库下载：${url}`);
            const resp = await fetch(url);
            if (!resp.ok) throw new Error(`下载失败 HTTP ${resp.status}`);
            const buf = new Uint8Array(await resp.arrayBuffer());
            let bin = "";
            const CHUNK = 0x8000;
            for (let i = 0; i < buf.length; i += CHUNK) {
              bin += String.fromCharCode.apply(null, buf.subarray(i, i + CHUNK));
            }
            this.log(`下载完成，${buf.length} 字节`);
            resolve(bin);
          } catch (err) { reject(err); }
        });
      });
    },
    async startFlash() {
      this.progress = 0;
      this.progressStatus = null;
      this.abortRequested = false;

      // 1. 准备固件数据
      let fwData = null;
      try {
        if (this.firmwareSource === "local") {
          if (!this.localFileData) return this.$message.error("请先选择本地 .bin 文件");
          fwData = this.localFileData;
        } else {
          if (!this.selectedFirmwareId) return this.$message.error("请先从固件库选择固件");
          fwData = await this.fetchServerFirmware();
        }
      } catch (err) {
        this.log(`固件准备失败：${err.message}`);
        return this.$message.error(`固件准备失败：${err.message}`);
      }

      const address = parseInt(this.flashAddress, 16);
      if (isNaN(address)) return this.$message.error("烧录地址格式不正确（如 0x0）");

      this.flashing = true;
      try {
        // 2. 加载 esptool-js（首次使用需联网加载）
        this.log("加载 esptool-js …");
        const { ESPLoader, Transport } = await import(/* webpackIgnore: true */ ESPTOOL_CDN);

        // 3. 请求串口
        this.log("请在弹窗中选择设备串口 …");
        const port = await navigator.serial.requestPort();
        const transport = new Transport(port, true);
        const self = this;
        const terminal = {
          clean() {},
          writeLine(line) { self.log(line); },
          write(s) { self.log(s); },
        };

        const loader = new ESPLoader({ transport, baudrate: 921600, romBaudrate: 115200, terminal });
        this.log("正在连接芯片（若无反应请按住 BOOT 再重插 USB）…");
        const chip = await loader.main();
        this.log(`已连接：${chip}`);

        // 4. 烧录
        this.log(`开始写入 Flash（地址 ${this.flashAddress}，${fwData.length} 字节）…`);
        await loader.writeFlash({
          fileArray: [{ data: fwData, address }],
          flashSize: "keep",
          flashMode: "keep",
          flashFreq: "keep",
          eraseAll: this.eraseAll,
          compress: true,
          reportProgress: (idx, written, total) => {
            this.progress = Math.min(100, Math.round((written / total) * 100));
            if (this.abortRequested) throw new Error("用户中止");
          },
        });
        await loader.after();
        await transport.disconnect();

        this.progress = 100;
        this.progressStatus = "success";
        this.log("✅ 烧录完成！请重启设备并按页面底部步骤配网绑定。");
        this.$message.success("烧录完成");
      } catch (err) {
        this.progressStatus = "exception";
        this.log(`❌ 失败：${err.message || err}`);
        this.$message.error(`烧录失败：${err.message || err}`);
      } finally {
        this.flashing = false;
      }
    },
  },
};
</script>

<style lang="scss" scoped>
.welcome {
  min-width: 900px;
  min-height: 506px;
  height: 100vh;
  display: flex;
  position: relative;
  flex-direction: column;
  background-size: cover;
  background: linear-gradient(to bottom right, #dce8ff, #e4eeff, #e6cbfd) center;
  -webkit-background-size: cover;
  -o-background-size: cover;
}
.operation-bar {
  display: flex;
  align-items: center;
  margin: 20px 30px 10px;
  .page-title { font-size: 24px; margin: 0; }
}
.main-wrapper {
  flex: 1;
  overflow-y: auto;
  margin: 0 22px 22px;
}
.content-panel {
  max-width: 980px;
  margin: 0 auto;
  text-align: left;
}
.step-card { margin-top: 16px; }
.source-block { margin-top: 14px; }
.hint { color: #909399; font-size: 12px; margin-left: 8px; }
.log-box {
  background: #1e1e1e;
  color: #9ae69a;
  font-family: Consolas, monospace;
  font-size: 12px;
  padding: 12px;
  border-radius: 6px;
  height: 220px;
  overflow-y: auto;
  white-space: pre-wrap;
  word-break: break-all;
  margin: 0;
}
.after-steps { margin: 0; padding-left: 20px; line-height: 2; }
</style>
