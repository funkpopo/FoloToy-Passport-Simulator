[English](README.md)

# FoloToy 身份工牌

适用于 ESP32-C3、240 × 320 屏幕的工牌固件，带内嵌手机网页编辑器与离线示例。
已下载并参考 https://github.com/folotoy/ai-passport，基准提交为
`df3990726e3751fadaaaa703a480dbba6e13c61b`。
屏幕和 ADC 按键驱动直接复用上游，未做功能性修改，MIT 许可证保留在
`components/bsp/LICENSE`。仅编译需要的显示、按键 BSP；工牌分块绘制 RGB565，
不启动 LVGL、蓝牙、音频或设备端中文字库。

## 真机使用

1. 按下文命令烧录。首次启动仅格式化新增的 `badge` 数据分区，保留工厂
   `cardid` 和 Recovery 的地址。
2. 首次开机直接显示示例工牌，长按确认键显示连接信息。手机连接 `FoloToy-Badge-XXXX` 热点，密码每台设备随机生成。
   手机提示「此网络无法上网」时，选择继续连接。
3. 用手机浏览器打开 **http://192.168.4.1**，展开「连接手机修改资料」，
   输入屏幕上的同一密码，点击连接。
4. 修改姓名、职位、公司、微信号、简介，上传微信头像和微信中保存的真实二维码，
   点击保存。中文排版在浏览器完成，设备保存并显示三页画面。
   微信号仅展示文字，不能据此生成加好友二维码。
5. 上／下键循环翻阅个人资料、头像、二维码；短按确认键切换头像和二维码；
   长按确认键显示／隐藏连接信息。电源键沿用硬件行为。

当前版本开机后持续开启热点，尚未实现自动休眠、蓝牙配网、微信登录或自动读取微信资料。
二维码完整缩放、不裁切。建议上传只含完整二维码及白边的图片，保存后务必用另一台手机
扫描实际屏幕验证。

## 在浏览器模拟器验证固件

在仓库根目录执行 `npm start`，打开 http://127.0.0.1:4190/。
点击「加载固件」，选择 `public/assets/firmware/identity-badge.bin`。
未配置时自动显示内置示例：个人资料、头像、测试二维码。按上／下翻页，确认键切换头像和二维码。
长按确认键显示热点信息，再次长按返回工牌。示例有「离线示例」标识，二维码内容是
`https://example.com`，不是微信好友码。手机保存自己的资料后，示例会被真实资料替代。

独立 `/badge.html` 页面及 Node.js 资料接口已删除，之前的 4191 演示服务已停止。
现在屏幕画面由真实固件通过 SPI 输出到 QEMU 模拟屏幕。
虚拟热点仍不能供真实手机连接；这里验证显示和按键，手机上传流程需真机验证。

## 编译与烧录

使用 **ESP-IDF 5.5.3**，目标为 ESP32-C3。在已激活 IDF 的终端进入本目录：

```text
idf.py build
idf.py merge-bin
idf.py -p COM5 flash
```

将 COM5 替换为设备实际串口。IDF 默认仅写 bootloader（`0x0`）、分区表（`0x8000`）
和应用（`0x10000`），不执行全片擦除、不写 cardid 或 Recovery。
已编译的合并镜像位于 `../../public/assets/firmware/identity-badge.bin`，也可用：

```text
python -m esptool --chip esp32c3 --port COM5 write_flash 0x0 identity-badge.bin
```

合并镜像结束于 `0x310000` 之前。**真机烧录时不能把镜像补齐到 8 MB**，否则会覆盖
工厂区域。变更分区表前，请备份旧自定义应用的数据；本应用使用原来未分配的
`0x35a000..0x700000` 区域保存 SPIFFS 数据。不要执行全片擦除。

也可以从仓库根目录使用容器（Docker 可替代 Podman）：

```powershell
podman run --rm -v "${PWD}:/project" -w /project/firmware/badge docker.io/espressif/idf:v5.5.3 idf.py build merge-bin
node tools/package-badge.mjs
```

编译会嵌入本目录 `web/` 中的手机网页，以及 `main/sample.rgb` 三页示例。
示例资源可运行 `node tools/generate-badge-sample.mjs` 重新生成（需单独安装 Playwright、qrcode 和 Chrome）。

## 数据与协议

- `PUT /api/badge/bundle`：先发送三帧，每帧 153600 字节、RGB565 大端 SPI 字节序，
  随后是 UTF-8 JSON，最多 220000 字节。七个资料字段均为字符串。
- `GET /api/badge/state`：返回 `{mode, revision, profile}`。两者均要求
  `X-Badge-Key`；保存还须携带 `If-Match` 版本号。
- 固件把上传内容分块写到备用 SPIFFS 槽，流式验证 JSON，不在堆中分配整张图片字符串。
  文件关闭成功后，在独立 `id_badge` NVS 命名空间提交版本。上传中断时仍选择旧槽。
- 屏幕使用 4800 字节 DMA 缓冲，等 SPI 完成后才复用。按键回调只投递队列，
  不执行存储或屏幕传输。
- 示例资源仅在没有有效资料时显示，不写入或覆盖用户资料分区。
- 编辑器支持 8 MB 以内的 PNG/JPEG/WebP，在浏览器中转换后上传。
  不依赖云服务，不请求外部图片。

## 验证

仓库根目录执行：`npm test`、`npm run build`、`npm run verify:release`。
固件协议测试可在本目录用任意 C11 编译器执行：

```text
cc -std=c11 -Wall -Wextra -Werror test_protocol.c -o badge-test
./badge-test
```

`tools/badge-firmware-smoke.mjs` 使用单独安装的 Playwright 和无头 Chrome，
加载真实固件，通过模拟器按键切换三页，将屏幕全部像素与嵌入数据比对，并检查长按确认和重启；
截图保存在 `artifacts/badge-firmware-*.png`。编译和模拟器测试不能替代真机验证：屏幕颜色与方向、ADC 手势、
iOS/Android 热点连接、二维码扫码成功率、断电恢复，仍需实物确认。
